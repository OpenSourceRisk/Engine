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

#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>
#include <qle/termstructures/swaptionvolcubewithatm.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/version.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

const Volatility SwaptionVolatilityConverter::minVol_ = 1.0e-7;
const Volatility SwaptionVolatilityConverter::maxVol_ = 10.0;
const Real SwaptionVolatilityConverter::minVega_ = 1.0e-6;

SwaptionVolatilityConverter::SwaptionVolatilityConverter(
    const Date& asof, const boost::shared_ptr<SwaptionVolatilityStructure>& svsIn,
    const Handle<YieldTermStructure>& discount, const Handle<YieldTermStructure>& shortDiscount,
    const boost::shared_ptr<SwapConventions>& conventions, const boost::shared_ptr<SwapConventions>& shortConventions,
    const Period& conventionsTenor, const Period& shortConventionsTenor, const VolatilityType targetType,
    const Matrix& targetShifts)
    : asof_(asof), svsIn_(svsIn), discount_(discount), shortDiscount_(shortDiscount), conventions_(conventions),
      shortConventions_(shortConventions), conventionsTenor_(conventionsTenor),
      shortConventionsTenor_(shortConventionsTenor), targetType_(targetType), targetShifts_(targetShifts),
      accuracy_(1.0e-5), maxEvaluations_(100) {

    // Some checks
    checkInputs();
}

SwaptionVolatilityConverter::SwaptionVolatilityConverter(const Date& asof,
                                                         const boost::shared_ptr<SwaptionVolatilityStructure>& svsIn,
                                                         const boost::shared_ptr<SwapIndex>& swapIndex,
                                                         const boost::shared_ptr<SwapIndex>& shortSwapIndex,
                                                         const VolatilityType targetType, const Matrix& targetShifts)
    : asof_(asof), svsIn_(svsIn), discount_(swapIndex->discountingTermStructure()),
      shortDiscount_(shortSwapIndex->discountingTermStructure()),
      conventions_(boost::make_shared<SwapConventions>(swapIndex->fixingDays(), swapIndex->fixedLegTenor(),
                                                       swapIndex->fixingCalendar(), swapIndex->fixedLegConvention(),
                                                       swapIndex->dayCounter(), swapIndex->iborIndex())),
      shortConventions_(boost::make_shared<SwapConventions>(
          shortSwapIndex->fixingDays(), shortSwapIndex->fixedLegTenor(), shortSwapIndex->fixingCalendar(),
          shortSwapIndex->fixedLegConvention(), shortSwapIndex->dayCounter(), shortSwapIndex->iborIndex())),
      conventionsTenor_(swapIndex->tenor()), shortConventionsTenor_(shortSwapIndex->tenor()), targetType_(targetType),
      targetShifts_(targetShifts), accuracy_(1.0e-5), maxEvaluations_(100) {

    // Some checks
    if (discount_.empty())
        discount_ = swapIndex->iborIndex()->forwardingTermStructure();
    if (shortDiscount_.empty())
        shortDiscount_ = shortSwapIndex->iborIndex()->forwardingTermStructure();
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

    boost::shared_ptr<SwaptionVolatilityDiscrete> svDisc;

    // We expect either the wrapper adding ATM to a cube or a swaption vol discrete
    // instance (like a matrix, a regular cube).

    vector<Real> strikeSpreads(1, 0.0);
    boost::shared_ptr<SwapIndex> swapIndexBase, shortSwapIndexBase;
    boost::shared_ptr<SwaptionVolatilityCube> cube;
    if (boost::dynamic_pointer_cast<SwaptionVolCubeWithATM>(svsIn_)) {
        cube = boost::static_pointer_cast<SwaptionVolCubeWithATM>(svsIn_)->cube();
    }
    if (boost::dynamic_pointer_cast<SwaptionVolatilityCube>(svsIn_)) {
        cube = boost::static_pointer_cast<SwaptionVolatilityCube>(svsIn_);
    }
    if (cube) {
        svDisc = cube;
        strikeSpreads = cube->strikeSpreads();
        swapIndexBase = cube->swapIndexBase();
        shortSwapIndexBase = cube->shortSwapIndexBase();
    } else if (boost::dynamic_pointer_cast<SwaptionVolatilityDiscrete>(svsIn_)) {
        svDisc = boost::static_pointer_cast<SwaptionVolatilityDiscrete>(svsIn_);
    } else {
        QL_FAIL("SwaptionVolatilityConverter: unknown input volatility structure");
    }

    // Some aspects of original volatility structure that we will need
    DayCounter dayCounter = svDisc->dayCounter();
    bool extrapolation = svDisc->allowsExtrapolation();
    Calendar calendar = svDisc->calendar();
    BusinessDayConvention bdc = svDisc->businessDayConvention();

    const vector<Date>& optionDates = svDisc->optionDates();
    const vector<Period>& optionTenors = svDisc->optionTenors();
    const vector<Period>& swapTenors = svDisc->swapTenors();
    const vector<Time>& optionTimes = svDisc->optionTimes();
    const vector<Time>& swapLengths = svDisc->swapLengths();
    Size nOptionTimes = optionTimes.size();
    Size nSwapLengths = swapLengths.size();
    Size nStrikeSpreads = strikeSpreads.size();

    Real targetShift = 0.0;

    // If target type is ShiftedLognormal and shifts are provided, check size
    if (targetType_ == ShiftedLognormal && !targetShifts_.empty()) {
        QL_REQUIRE(targetShifts_.rows() == nOptionTimes,
                   "SwaptionVolatilityConverter: number of shift rows does not equal the number of option tenors");
        QL_REQUIRE(targetShifts_.columns() == nSwapLengths,
                   "SwaptionVolatilityConverter: number of shift columns does not equal the number of swap tenors");
    }

    // Calculate the converted ATM volatilities
    Matrix volatilities(nOptionTimes, nSwapLengths);
    for (Size i = 0; i < nOptionTimes; ++i) {
        for (Size j = 0; j < nSwapLengths; ++j) {
            if (!targetShifts_.empty())
                targetShift = targetShifts_[i][j];
            volatilities[i][j] = convert(optionDates[i], swapTenors[j], 0.0, dayCounter, targetType_, targetShift);
        }
    }

    // Build ATM matrix
    Handle<SwaptionVolatilityStructure> atmStructure;
    if (calendar.empty() || optionTenors.empty()) {
        // Original matrix was created with fixed option dates
        atmStructure = Handle<SwaptionVolatilityStructure>(boost::make_shared<SwaptionVolatilityMatrix>(
            asof_, optionDates, swapTenors, volatilities, Actual365Fixed(), extrapolation, targetType_, targetShifts_));
    } else {
        atmStructure = Handle<SwaptionVolatilityStructure>(boost::shared_ptr<SwaptionVolatilityMatrix>(
            new SwaptionVolatilityMatrix(asof_, calendar, bdc, optionTenors, swapTenors, volatilities, Actual365Fixed(),
                                         extrapolation, targetType_, targetShifts_)));
    }

    // no cube input => we are done
    if (!cube)
        return *atmStructure;

    // convert non-ATM volatilities, note that we use the ATM option tenors and swap tenors here!

    std::vector<std::vector<Handle<Quote> > > volSpreads(nOptionTimes * nSwapLengths,
                                                         std::vector<Handle<Quote> >(nStrikeSpreads));
    for (Size k = 0; k < nStrikeSpreads; ++k) {
        for (Size i = 0; i < nOptionTimes; ++i) {
            for (Size j = 0; j < nSwapLengths; ++j) {
                if (!targetShifts_.empty())
                    targetShift = targetShifts_[i][j];
                Real outVol =
                    convert(optionDates[i], swapTenors[j], strikeSpreads[k], dayCounter, targetType_, targetShift);
                volSpreads[i * nSwapLengths + j][k] = Handle<Quote>(boost::make_shared<SimpleQuote>(outVol));
            }
        }
    }

    // Build and return cube, note that we hardcode flat extrapolation
    boost::shared_ptr<SwaptionVolatilityCube> cubeOut = boost::shared_ptr<QuantExt::SwaptionVolCube2>(
        new QuantExt::SwaptionVolCube2(atmStructure, optionTenors, swapTenors, strikeSpreads, volSpreads, swapIndexBase,
                                       shortSwapIndexBase, false, true, false));
    cubeOut->enableExtrapolation(cube->allowsExtrapolation());
    return boost::make_shared<SwaptionVolCubeWithATM>(cubeOut);

} // namespace QuantExt

// Ignore "warning C4996: 'Quantlib::Swaption::impliedVolatility': was declared deprecated"
#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
Real SwaptionVolatilityConverter::convert(const Date& expiry, const Period& swapTenor, Real strikeSpread,
                                          const DayCounter& volDayCounter, VolatilityType outType,
                                          Real outShift) const {

    const boost::shared_ptr<SwapConventions> tmpConv =
        swapTenor <= shortConventionsTenor_ ? shortConventions_ : conventions_;
    Handle<YieldTermStructure> tmpDiscount = swapTenor <= shortConventionsTenor_ ? shortDiscount_ : discount_;

    // Create the underlying swap with fixed rate = fair rate
    // We rely on the fact that MakeVanillaSwap sets the fixed rate to the fair rate if it is left null in the ctor
    Date effectiveDate = tmpConv->fixedCalendar().advance(expiry, tmpConv->settlementDays(), Days);
    boost::shared_ptr<PricingEngine> engine = boost::make_shared<DiscountingSwapEngine>(tmpDiscount);
    boost::shared_ptr<VanillaSwap> swap = MakeVanillaSwap(swapTenor, tmpConv->floatIndex())
                                              .withType(strikeSpread < 0.0 ? VanillaSwap::Receiver : VanillaSwap::Payer)
                                              .withEffectiveDate(effectiveDate)
                                              .withFixedLegCalendar(tmpConv->fixedCalendar())
                                              .withFixedLegDayCount(tmpConv->fixedDayCounter())
                                              .withFixedLegTenor(tmpConv->fixedTenor())
                                              .withFixedLegConvention(tmpConv->fixedConvention())
                                              .withFixedLegTerminationDateConvention(tmpConv->fixedConvention())
                                              .withFloatingLegSpread(0.0)
                                              .withPricingEngine(engine);
    // we need this also for non-atm swaps
    Rate atmRate = swap->fairRate(), strike;
    if (!close_enough(strikeSpread, 0.0)) {
        strike = atmRate + strikeSpread;
        swap = MakeVanillaSwap(swapTenor, tmpConv->floatIndex(), strike)
                   .withEffectiveDate(effectiveDate)
                   .withFixedLegTenor(tmpConv->fixedTenor())
                   .withFixedLegDayCount(tmpConv->fixedDayCounter())
                   .withFloatingLegSpread(0.0)
                   .withPricingEngine(engine);
    } else {
        strike = atmRate;
    }

    Real inShift = svsIn_->shift(expiry, swapTenor);
    VolatilityType inType = svsIn_->volatilityType();

    // if strike is invalid w.r.t. the given input or output vol types, return zero vol
    // (for a lognormal cube e.g. it is common that some effective strikes are negative)
    Real inMinStrike = inType == ShiftedLognormal ? -inShift : -QL_MAX_REAL;
    Real outMinStrike = outType == ShiftedLognormal ? -outShift : -QL_MAX_REAL;
    if (strike < inMinStrike || strike < outMinStrike)
        return 0.0;

    Real inVol = svsIn_->volatility(expiry, swapTenor, strike);

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

    // zero might be the actual implied vol (if we e.g. convert from LN to N with a strike near zero),
    // but the Swaption::impliedVolatility function will not find this, so we check for this case
    // explicitly here
    Real vega = swaption->result<Real>("vega");
    if (vega < minVega_)
        return 0.0;

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
#if QL_HEX_VERSION < 0x011000f0
        impliedVol = swaption->impliedVolatility(npv, discount_, guess, accuracy_, maxEvaluations_, minVol_, maxVol_,
                                                 outShift, outType);
#else
        impliedVol = swaption->impliedVolatility(npv, discount_, guess, accuracy_, maxEvaluations_, minVol_, maxVol_,
                                                 outType, outShift);
#endif

    } catch (std::exception& e) {
        // couldn't find implied volatility
        QL_FAIL("SwaptionVolatilityConverter: volatility conversion failed while trying to convert volatility"
                " for expiry "
                << expiry << " and swap tenor " << swapTenor << ". Error: " << e.what());
    }

    return impliedVol;
}
#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
} // namespace QuantExt
