/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <qle/termstructures/swaptionvolatilityconverter.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

const Volatility SwaptionVolatilityConverter::minVol_ = 1.0e-7;
const Volatility SwaptionVolatilityConverter::maxVol_ = 10.0;

SwaptionVolatilityConverter::SwaptionVolatilityConverter(const Date& asof,
                                                         const boost::shared_ptr<SwaptionVolatilityStructure>& svsIn,
                                                         const Handle<YieldTermStructure>& discount,
                                                         const boost::shared_ptr<SwapConventions>& conventions,
                                                         const VolatilityType targetType, const Matrix& targetShifts)
    : asof_(asof), svsIn_(svsIn), discount_(discount), conventions_(conventions), targetType_(targetType),
      targetShifts_(targetShifts), accuracy_(1.0e-5), maxEvaluations_(100) {

    // Some checks
    checkInputs();
}

SwaptionVolatilityConverter::SwaptionVolatilityConverter(const Date& asof,
                                                         const boost::shared_ptr<SwaptionVolatilityStructure>& svsIn,
                                                         const boost::shared_ptr<SwapIndex>& swapIndex,
                                                         const VolatilityType targetType, const Matrix& targetShifts)
    : asof_(asof), svsIn_(svsIn), discount_(swapIndex->discountingTermStructure()),
      conventions_(boost::make_shared<SwapConventions>(swapIndex->fixingDays(), swapIndex->fixedLegTenor(),
                                                       swapIndex->fixingCalendar(), swapIndex->fixedLegConvention(),
                                                       swapIndex->dayCounter(), swapIndex->iborIndex())),
      targetType_(targetType), targetShifts_(targetShifts), accuracy_(1.0e-5), maxEvaluations_(100) {

    // Some checks
    if (discount_.empty())
        discount_ = swapIndex->iborIndex()->forwardingTermStructure();
    checkInputs();
}

void SwaptionVolatilityConverter::checkInputs() const {
    QL_REQUIRE(svsIn_->referenceDate() == asof_,
               "SwaptionVolatilityConverter requires the asof date and reference date to align");
    QL_REQUIRE(!discount_.empty() && discount_->referenceDate() == asof_,
               "SwaptionVolatilityConverter requires a valid discount curve with reference date equal to asof date");
    Handle<YieldTermStructure> forwardCurve = conventions_->floatIndex()->forwardingTermStructure();
    QL_REQUIRE(!forwardCurve.empty() && forwardCurve->referenceDate() == asof_,
               "SwaptionVolatilityConverter requires a valid forward curve with reference date equal to asof date");
}

boost::shared_ptr<SwaptionVolatilityStructure> SwaptionVolatilityConverter::convert() const {
    // If SwaptionVolatilityMatrix passed in
    boost::shared_ptr<SwaptionVolatilityMatrix> svMatrix =
        boost::dynamic_pointer_cast<SwaptionVolatilityMatrix>(svsIn_);
    if (svMatrix)
        return convert(svMatrix);

    // If we get to here, then not supported
    QL_FAIL("SwaptionVolatilityConverter requires a SwaptionVolatilityMatrix as input");
}

boost::shared_ptr<SwaptionVolatilityStructure>
SwaptionVolatilityConverter::convert(const boost::shared_ptr<SwaptionVolatilityMatrix>& svMatrix) const {

    // Some aspects of original volatility structure that we will need
    DayCounter dayCounter = svMatrix->dayCounter();
    bool extrapolation = svMatrix->allowsExtrapolation();
    Calendar calendar = svMatrix->calendar();
    BusinessDayConvention bdc = svMatrix->businessDayConvention();

    const vector<Date>& optionDates = svMatrix->optionDates();
    const vector<Period>& optionTenors = svMatrix->optionTenors();
    const vector<Period>& swapTenors = svMatrix->swapTenors();
    const vector<Time>& optionTimes = svMatrix->optionTimes();
    const vector<Time>& swapLengths = svMatrix->swapLengths();
    Size nOptionTimes = optionTimes.size();
    Size nSwapLengths = swapLengths.size();

    Rate dummyStrike = 0.0;
    Volatility inVolatility = 0.0;
    VolatilityType inType = svMatrix->volatilityType();
    Real inShift = 0.0;
    Real targetShift = 0.0;

    // If target type is ShiftedLognormal and shifts are provided, check size
    if (targetType_ == ShiftedLognormal && !targetShifts_.empty()) {
        QL_REQUIRE(targetShifts_.rows() == nOptionTimes,
                   "SwaptionVolatilityConverter: number of shift rows does not equal the number of option tenors");
        QL_REQUIRE(targetShifts_.columns() == nSwapLengths,
                   "SwaptionVolatilityConverter: number of shift columns does not equal the number of swap tenors");
    }

    // Calculate the converted volatilities
    Matrix volatilities(nOptionTimes, nSwapLengths);
    for (Size i = 0; i < nOptionTimes; ++i) {
        for (Size j = 0; j < nSwapLengths; ++j) {
            inVolatility = svMatrix->volatility(optionTimes[i], swapLengths[j], dummyStrike);
            inShift = svMatrix->shift(optionTimes[i], swapLengths[j]);
            if (!targetShifts_.empty())
                targetShift = targetShifts_[i][j];
            volatilities[i][j] = convert(optionDates[i], swapTenors[j], dayCounter, inVolatility, inType, targetType_,
                                         inShift, targetShift);
        }
    }

    // Return the new swaption volatility matrix
    if (calendar.empty() || optionTenors.empty()) {
        // Original matrix was created with fixed option dates
        return boost::make_shared<SwaptionVolatilityMatrix>(
            asof_, optionDates, swapTenors, volatilities, Actual365Fixed(), extrapolation, targetType_, targetShifts_);
    } else {
        return boost::shared_ptr<SwaptionVolatilityMatrix>(
            new SwaptionVolatilityMatrix(asof_, calendar, bdc, optionTenors, swapTenors, volatilities, Actual365Fixed(),
                                         extrapolation, targetType_, targetShifts_));
    }
}

Real SwaptionVolatilityConverter::convert(const Date& expiry, const Period& swapTenor, const DayCounter& volDayCounter,
                                          Real inVol, VolatilityType inType, VolatilityType outType, Real inShift,
                                          Real outShift) const {

    // Create the underlying swap with fixed rate = fair rate
    // We rely on the fact that MakeVanillaSwap sets the fixed rate to the fair rate if it is left null in the ctor
    Date effectiveDate = conventions_->fixedCalendar().advance(expiry, conventions_->settlementDays(), Days);
    boost::shared_ptr<PricingEngine> engine = boost::make_shared<DiscountingSwapEngine>(discount_);
    boost::shared_ptr<VanillaSwap> swap = MakeVanillaSwap(swapTenor, conventions_->floatIndex())
                                              .withEffectiveDate(effectiveDate)
                                              .withFixedLegTenor(conventions_->fixedTenor())
                                              .withFixedLegDayCount(conventions_->fixedDayCounter())
                                              .withFloatingLegSpread(0.0)
                                              .withPricingEngine(engine);
    Rate atmRate = swap->fairRate();

    // Create the swaption
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiry);
    boost::shared_ptr<Swaption> swaption = boost::make_shared<Swaption>(swap, exercise, Settlement::Physical);

    // Price the swaption with the input volatility
    boost::shared_ptr<PricingEngine> swaptionEngine;
    if (inType == ShiftedLognormal) {
        swaptionEngine = boost::make_shared<BlackSwaptionEngine>(discount_, inVol, volDayCounter, inShift);
    } else {
        swaptionEngine = boost::make_shared<BachelierSwaptionEngine>(discount_, inVol, volDayCounter);
    }
    swaption->setPricingEngine(swaptionEngine);

    Volatility impliedVol = 0.0;
    try {
        Real npv = swaption->NPV();

        // Calculate guess for implied volatility solver
        Real guess = 0.0;
        if (outType == ShiftedLognormal) {
            QL_REQUIRE(atmRate + outShift > 0.0, "SwaptionVolatilityConverter: ATM rate + shift must be > 0.0");
            if (inType == Normal)
                guess = inVol / (atmRate + outShift);
            else
                guess = inVol * (atmRate + inShift) / (atmRate + outShift);
        } else {
            if (inType == Normal)
                guess = inVol;
            else
                guess = inVol * (atmRate + inShift);
        }

        // Note: In implying the volatility the volatility day counter is hardcoded to Actual365Fixed
        impliedVol = swaption->impliedVolatility(npv, discount_, guess, accuracy_, maxEvaluations_, minVol_, maxVol_,
                                                 outShift, outType);
    } catch (std::exception& e) {
        // couldn't find implied volatility
        QL_FAIL("SwaptionVolatilityConverter: volatility conversion failed while trying to convert volatility"
                " for expiry "
                << expiry << " and swap tenor " << swapTenor << ". Error: " << e.what());
    }

    return impliedVol;
}
}
