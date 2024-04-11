/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ql/instruments/makecapfloor.hpp>
#include <ql/option.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <qle/termstructures/optionletstripper1.hpp>

#include <boost/make_shared.hpp>

using QuantLib::ext::shared_ptr;

namespace QuantExt {

OptionletStripper1::OptionletStripper1(const shared_ptr<QuantExt::CapFloorTermVolSurface>& termVolSurface,
                                       const shared_ptr<IborIndex>& index, Rate switchStrike, Real accuracy,
                                       Natural maxIter, const Handle<YieldTermStructure>& discount,
                                       const VolatilityType type, const Real displacement,
                                       const optional<VolatilityType> targetVolatilityType,
                                       const optional<Real> targetDisplacement)
    : OptionletStripper(termVolSurface, index, discount, targetVolatilityType ? *targetVolatilityType : type,
                        targetDisplacement ? *targetDisplacement : displacement),
      volQuotes_(nOptionletTenors_, std::vector<shared_ptr<SimpleQuote> >(nStrikes_)),
      floatingSwitchStrike_(switchStrike == Null<Rate>() ? true : false), capFlooMatrixNotInitialized_(true),
      switchStrike_(switchStrike), accuracy_(accuracy), maxIter_(maxIter), inputVolatilityType_(type),
      inputDisplacement_(displacement) {

    capFloorPrices_ = Matrix(nOptionletTenors_, nStrikes_);
    optionletPrices_ = Matrix(nOptionletTenors_, nStrikes_);
    capletVols_ = Matrix(nOptionletTenors_, nStrikes_);
    capFloorVols_ = Matrix(nOptionletTenors_, nStrikes_);

    Real firstGuess = 0.14; // guess is only used for shifted lognormal vols
    optionletStDevs_ = Matrix(nOptionletTenors_, nStrikes_, firstGuess);

    capFloors_ = CapFloorMatrix(nOptionletTenors_);
    capFloorEngines_ = std::vector<std::vector<QuantLib::ext::shared_ptr<PricingEngine> > >(nOptionletTenors_);
}

void OptionletStripper1::performCalculations() const {

    // update dates
    populateDates();

    if (floatingSwitchStrike_) {
        Rate averageAtmOptionletRate = 0.0;
        for (Size i = 0; i < nOptionletTenors_; ++i) {
            averageAtmOptionletRate += atmOptionletRate_[i];
        }
        switchStrike_ = averageAtmOptionletRate / nOptionletTenors_;
    }

    const Handle<YieldTermStructure>& discountCurve =
        discount_.empty() ? index_->forwardingTermStructure() : discount_;

    const std::vector<Rate>& strikes = termVolSurface_->strikes();
    DayCounter dc = termVolSurface_->dayCounter();
    // initialize CapFloorMatrix
    if (capFlooMatrixNotInitialized_) {
        for (Size i = 0; i < nOptionletTenors_; ++i) {
            capFloors_[i].resize(nStrikes_);
            capFloorEngines_[i].resize(nStrikes_);
        }
        // construction might go here
        for (Size j = 0; j < nStrikes_; ++j) {
            for (Size i = 0; i < nOptionletTenors_; ++i) {
                volQuotes_[i][j] = shared_ptr<SimpleQuote>(new SimpleQuote());
                if (inputVolatilityType_ == ShiftedLognormal) {
                    capFloorEngines_[i][j] = QuantLib::ext::make_shared<BlackCapFloorEngine>(
                        discountCurve, Handle<Quote>(volQuotes_[i][j]), dc, inputDisplacement_);
                } else if (inputVolatilityType_ == Normal) {
                    capFloorEngines_[i][j] =
                        QuantLib::ext::make_shared<BachelierCapFloorEngine>(discountCurve, Handle<Quote>(volQuotes_[i][j]), dc);
                } else {
                    QL_FAIL("unknown volatility type: " << volatilityType_);
                }
            }
        }
        capFlooMatrixNotInitialized_ = false;
    }

    for (Size j = 0; j < nStrikes_; ++j) {
        // using out-of-the-money options - but these are not always out of the money, for different tenors we may need
        // to switch
        CapFloor::Type capFloorType = strikes[j] < switchStrike_ ? CapFloor::Floor : CapFloor::Cap;

        // we do this with the above to keep the variables capFloors_ etc consistent, but really its the
        // optionletStdDevs_ below that we want.
        Real previousCapFloorPrice = 0.0;
        for (Size i = 0; i < nOptionletTenors_; ++i) {

            capFloorVols_[i][j] = termVolSurface_->volatility(capFloorLengths_[i], strikes[j], true);
            volQuotes_[i][j]->setValue(capFloorVols_[i][j]);
            capFloors_[i][j] = MakeCapFloor(capFloorType, capFloorLengths_[i], index_, strikes[j], -0 * Days)
                                   .withPricingEngine(capFloorEngines_[i][j]);
            capFloorPrices_[i][j] = capFloors_[i][j]->NPV();
            optionletPrices_[i][j] = capFloorPrices_[i][j] - previousCapFloorPrice;
            previousCapFloorPrice = capFloorPrices_[i][j];
        }

        // now try to strip
        std::vector<Real> optionletStrip(nOptionletTenors_);
        Real firstGuess = optionletStDevs_[0][j]; // assumes we have a constant first guess here (as we do)
        bool ok = stripOptionlets(optionletStrip, capFloorType, j, discountCurve, firstGuess);
        if (!ok) {
            // try the reverse
            capFloorType = capFloorType == CapFloor::Cap ? CapFloor::Floor : CapFloor::Cap;
            ok = stripOptionlets(optionletStrip, capFloorType, j, discountCurve, firstGuess);
            QL_REQUIRE(ok, "Failed to strip Caplet vols");
        }
        // now copy
        for (Size i = 0; i < nOptionletTenors_; ++i) {
            optionletStDevs_[i][j] = optionletStrip[i];
            optionletVolatilities_[i][j] = optionletStDevs_[i][j] / std::sqrt(optionletTimes_[i]);
        }
    }
}

bool OptionletStripper1::stripOptionlets(std::vector<Real>& out, CapFloor::Type capFloorType, Size j,
                                         const Handle<YieldTermStructure>& discountCurve, Real firstGuess) const {

    Real strike = termVolSurface_->strikes()[j];

    // floor is put, cap is call
    Option::Type optionletType = capFloorType == CapFloor::Floor ? Option::Put : Option::Call;

    Real previousCapFloorPrice = 0.0;
    for (Size i = 0; i < nOptionletTenors_; ++i) {

        // we have capFloorVols_[i][j] & volQuotes_[i][j]
        CapFloor capFloor = MakeCapFloor(capFloorType, capFloorLengths_[i], index_, strike, -0 * Days)
                                .withPricingEngine(capFloorEngines_[i][j]);
        Real capFloorPrice = capFloor.NPV();
        Real optionletPrice = std::max(0.0, capFloorPrice - previousCapFloorPrice);
        previousCapFloorPrice = capFloorPrice;

        DiscountFactor d = discountCurve->discount(optionletPaymentDates_[i]);
        DiscountFactor optionletAnnuity = optionletAccrualPeriods_[i] * d;
        try {
            if (volatilityType_ == ShiftedLognormal) {
                out[i] = blackFormulaImpliedStdDev(optionletType, strike, atmOptionletRate_[i], optionletPrice,
                                                   optionletAnnuity, displacement_, firstGuess, accuracy_, maxIter_);
            } else if (volatilityType_ == Normal) {
                out[i] = std::sqrt(optionletTimes_[i]) *
                         bachelierBlackFormulaImpliedVol(optionletType, strike, atmOptionletRate_[i],
                                                         optionletTimes_[i], optionletPrice, optionletAnnuity);
            } else {
                QL_FAIL("Unknown target volatility type: " << volatilityType_);
            }
        } catch (std::exception& /*e*/) {
            /*
            QL_FAIL("could not bootstrap optionlet:"
                << "\n type:    " << optionletType
                << "\n strike:  " << io::rate(strike)
                << "\n atm:     " << io::rate(atmOptionletRate_[i])
                << "\n price:   " << optionletPrice << "\n annuity: " << optionletAnnuity
                << "\n expiry:  " << optionletDates_[i] << "\n error:   " << e.what());
            */
            // No need to wipe the output (?)
            return false;
        }
    }
    return true;
}

const Matrix& OptionletStripper1::capletVols() const {
    calculate();
    return capletVols_;
}

const Matrix& OptionletStripper1::capFloorPrices() const {
    calculate();
    return capFloorPrices_;
}

const Matrix& OptionletStripper1::capFloorVolatilities() const {
    calculate();
    return capFloorVols_;
}

const Matrix& OptionletStripper1::optionletPrices() const {
    calculate();
    return optionletPrices_;
}

Rate OptionletStripper1::switchStrike() const {
    if (floatingSwitchStrike_)
        calculate();
    return switchStrike_;
}
} // namespace QuantExt
