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

using boost::shared_ptr;

namespace QuantExt {

OptionletStripper1::OptionletStripper1(const shared_ptr<QuantExt::CapFloorTermVolSurface>& termVolSurface,
                                       const shared_ptr<IborIndex>& index, Rate switchStrike, Real accuracy,
                                       Natural maxIter, const Handle<YieldTermStructure>& discount,
                                       const VolatilityType type, const Real displacement, bool dontThrow,
                                       const optional<VolatilityType> targetVolatilityType,
                                       const optional<Real> targetDisplacement, Real dontThrowMinVol)
    : OptionletStripper(termVolSurface, index, discount, targetVolatilityType ? *targetVolatilityType : type,
                        targetDisplacement ? *targetDisplacement : displacement),
      volQuotes_(nOptionletTenors_, std::vector<shared_ptr<SimpleQuote> >(nStrikes_)),
      floatingSwitchStrike_(switchStrike == Null<Rate>() ? true : false), capFlooMatrixNotInitialized_(true),
      switchStrike_(switchStrike), accuracy_(accuracy), maxIter_(maxIter), dontThrow_(dontThrow),
      dontThrowMinVol_(dontThrowMinVol), inputVolatilityType_(type), inputDisplacement_(displacement) {

    capFloorPrices_ = Matrix(nOptionletTenors_, nStrikes_);
    optionletPrices_ = Matrix(nOptionletTenors_, nStrikes_);
    capletVols_ = Matrix(nOptionletTenors_, nStrikes_);
    capFloorVols_ = Matrix(nOptionletTenors_, nStrikes_);

    Real firstGuess = 0.14; // guess is only used for shifted lognormal vols
    optionletStDevs_ = Matrix(nOptionletTenors_, nStrikes_, firstGuess);

    capFloors_ = CapFloorMatrix(nOptionletTenors_);
    capFloorEngines_ = std::vector<std::vector<boost::shared_ptr<PricingEngine> > >(nOptionletTenors_);
}

void OptionletStripper1::performCalculations() const {

    // update dates
    const Date& referenceDate = termVolSurface_->referenceDate();
    const DayCounter& dc = termVolSurface_->dayCounter();
    shared_ptr<BlackCapFloorEngine> dummy(new BlackCapFloorEngine( // discounting does not matter here
        iborIndex_->forwardingTermStructure(), 0.20, dc));
    for (Size i = 0; i < nOptionletTenors_; ++i) {
        CapFloor temp = MakeCapFloor(CapFloor::Cap, capFloorLengths_[i], iborIndex_,
                                     0.04, // dummy strike
                                     0 * Days)
                            .withPricingEngine(dummy);
        shared_ptr<FloatingRateCoupon> lFRC = temp.lastFloatingRateCoupon();
        optionletDates_[i] = lFRC->fixingDate();
        optionletPaymentDates_[i] = lFRC->date();
        optionletAccrualPeriods_[i] = lFRC->accrualPeriod();
        optionletTimes_[i] = dc.yearFraction(referenceDate, optionletDates_[i]);
        atmOptionletRate_[i] = lFRC->indexFixing();
    }

    if (floatingSwitchStrike_) {
        Rate averageAtmOptionletRate = 0.0;
        for (Size i = 0; i < nOptionletTenors_; ++i) {
            averageAtmOptionletRate += atmOptionletRate_[i];
        }
        switchStrike_ = averageAtmOptionletRate / nOptionletTenors_;
    }

    const Handle<YieldTermStructure>& discountCurve =
        discount_.empty() ? iborIndex_->forwardingTermStructure() : discount_;

    const std::vector<Rate>& strikes = termVolSurface_->strikes();
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
                    capFloorEngines_[i][j] = boost::make_shared<BlackCapFloorEngine>(
                        discountCurve, Handle<Quote>(volQuotes_[i][j]), dc, inputDisplacement_);
                } else if (inputVolatilityType_ == Normal) {
                    capFloorEngines_[i][j] =
                        boost::make_shared<BachelierCapFloorEngine>(discountCurve, Handle<Quote>(volQuotes_[i][j]), dc);
                } else {
                    QL_FAIL("unknown volatility type: " << volatilityType_);
                }
            }
        }
        capFlooMatrixNotInitialized_ = false;
    }

    for (Size j = 0; j < nStrikes_; ++j) {
        // using out-of-the-money options - but these are not always out of the money, for different tenors we may need to switch
        CapFloor::Type capFloorType = strikes[j] < switchStrike_ ? CapFloor::Floor : CapFloor::Cap;

        // we do this with the above to keep the variables capFloors_ etc consistant, but really its the optionletStdDevs_ below
        // that we want.
        Real previousCapFloorPrice = 0.0;
        for (Size i = 0; i < nOptionletTenors_; ++i) {

            capFloorVols_[i][j] = termVolSurface_->volatility(capFloorLengths_[i], strikes[j], true);
            volQuotes_[i][j]->setValue(capFloorVols_[i][j]);
            capFloors_[i][j] = MakeCapFloor(capFloorType, capFloorLengths_[i], iborIndex_, strikes[j], -0 * Days)
                                   .withPricingEngine(capFloorEngines_[i][j]);
            capFloorPrices_[i][j] = capFloors_[i][j]->NPV();
            optionletPrices_[i][j] = capFloorPrices_[i][j] - previousCapFloorPrice;
            previousCapFloorPrice = capFloorPrices_[i][j];
        }

	// now try to strip
        std::vector<Real> optionletStrip(nOptionletTenors_);
        bool ok = stripOptionlets(optionletStrip, capFloorType, j, discountCurve);
        if (!ok) {
            // try the reverse
            capFloorType = capFloorType == CapFloor::Cap ? CapFloor::Floor : CapFloor::Cap;
            ok = stripOptionlets(optionletStrip, capFloorType, j, discountCurve);
            QL_REQUIRE(ok, "Failed to strip Caplet vols");
        }
        // now copy
        for (Size i = 0; i < nOptionletTenors_; ++i) {
            optionletStDevs_[i][j] = optionletStrip[i];
            optionletVolatilities_[i][j] = optionletStDevs_[i][j] / std::sqrt(optionletTimes_[i]);
        }
    }
}


bool OptionletStripper1::stripOptionlets(std::vector<Real>& out, CapFloor::Type capFloorType, Size j, const Handle<YieldTermStructure>& discountCurve) const {

    bool ok = true;
    Real strike = termVolSurface_->strikes()[j];

    // floor is put, cap is call
    Option::Type optionletType = capFloorType == CapFloor::Floor ? Option::Put : Option::Call;

    Real previousCapFloorPrice = 0.0;
    for (Size i = 0; i < nOptionletTenors_; ++i) {

	// we have capFloorVols_[i][j] & volQuotes_[i][j]
	CapFloor capFloor = MakeCapFloor(capFloorType, capFloorLengths_[i], iborIndex_, strike, -0 * Days)
			       .withPricingEngine(capFloorEngines_[i][j]);
	Real capFloorPrice = capFloor.NPV();
	Real optionletPrice = capFloorPrice - previousCapFloorPrice;
	previousCapFloorPrice = capFloorPrice;

	DiscountFactor d = discountCurve->discount(optionletPaymentDates_[i]);
	DiscountFactor optionletAnnuity = optionletAccrualPeriods_[i] * d;
	try {
	    if (volatilityType_ == ShiftedLognormal) {
		out[i] = blackFormulaImpliedStdDev(
		    optionletType, strike, atmOptionletRate_[i], optionletPrice, optionletAnnuity,
		    displacement_, 0.0 /*optionletStDevs_[i][j]*/, accuracy_, maxIter_);
	    } else if (volatilityType_ == Normal) {
		out[i] = std::sqrt(optionletTimes_[i]) *
		    bachelierBlackFormulaImpliedVol(optionletType, strike, atmOptionletRate_[i],
						    optionletTimes_[i], optionletPrice, optionletAnnuity);
	    } else {
		QL_FAIL("Unknown target volatility type: " << volatilityType_);
	    }
	} catch (std::exception& e) {
            ok = false;
/*
   	    QL_FAIL("could not bootstrap optionlet:"
			"\n type:    "
			<< optionletType << "\n strike:  " << io::rate(strikes[j])
			<< "\n atm:     " << io::rate(atmOptionletRate_[i])
			<< "\n price:   " << optionletPrices_[i][j] << "\n annuity: " << optionletAnnuity
			<< "\n expiry:  " << optionletDates_[i] << "\n error:   " << e.what());
*/
	}
    }

    return ok;
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
