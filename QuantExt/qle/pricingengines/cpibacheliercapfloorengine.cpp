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

/*!
 \file cpibacheliercapfloorengines.cpp
 \brief Engines for CPI options
 \ingroup PricingEngines
 */

#include <ql/time/daycounters/actualactual.hpp>

#include <ql/pricingengines/blackformula.hpp>
#include <qle/pricingengines/cpibacheliercapfloorengine.hpp>
#include <qle/utilities/inflation.hpp>

using namespace QuantLib;

namespace QuantExt {

CPIBachelierCapFloorEngine::CPIBachelierCapFloorEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<CPIVolatilitySurface>& volatilitySurface)
    : discountCurve_(discountCurve), volatilitySurface_(volatilitySurface) {
    registerWith(discountCurve_);
    registerWith(volatilitySurface_);
}

void CPIBachelierCapFloorEngine::calculate() const {

    Date maturity = arguments_.payDate;

    auto index = arguments_.infIndex;
    DiscountFactor d = arguments_.nominal * discountCurve_->discount(maturity);

    bool isInterpolated = arguments_.observationInterpolation == CPI::Linear ||
                          (arguments_.observationInterpolation == CPI::AsIndex && arguments_.infIndex->interpolated());

    Date optionObservationDate = arguments_.payDate - arguments_.observationLag;

    Date optionBaseDate = arguments_.startDate - arguments_.observationLag;

    Real optionBaseFixing = arguments_.baseCPI == Null<Real>()
                                ? ZeroInflation::cpiFixing(*arguments_.infIndex.currentLink(), arguments_.startDate,
                                                           arguments_.observationLag, isInterpolated)
                                : arguments_.baseCPI;

    Real atmCPIFixing = ZeroInflation::cpiFixing(*arguments_.infIndex.currentLink(), maturity,
                                                 arguments_.observationLag, isInterpolated);

    Time ttm =
        inflationYearFraction(index->frequency(), isInterpolated, index->zeroInflationTermStructure()->dayCounter(),
                              optionBaseDate, optionObservationDate);

    Real atmGrowth = atmCPIFixing / optionBaseFixing;
    Real strike = std::pow(1.0 + arguments_.strike, ttm);

    auto lastKnownFixing =
        ZeroInflation::lastAvailableFixing(*index.currentLink(), volatilitySurface_->referenceDate());
    auto observationPeriod = inflationPeriod(optionObservationDate, index->frequency());
    auto requiredFixing = isInterpolated ? observationPeriod.first : observationPeriod.second + 1 * Days;

    // if time from base <= 0 the fixing is already known and stdDev is zero, return the intrinsic value
    Real stdDev = 0.0;
    if (requiredFixing < lastKnownFixing) {
        // For reading volatility in the current market volatiltiy structure
        // baseFixingSwap(T0) * pow(1 + strikeRate(T0), T-T0) = StrikeIndex = baseFixing(t) * pow(1 + strikeRate(t),
        // T-t), solve for strikeRate(t):
        auto surfaceBaseDate = ZeroInflation::effectiveObservationDate(
            volatilitySurface_->referenceDate(), volatilitySurface_->observationLag(), volatilitySurface_->frequency(),
            volatilitySurface_->indexIsInterpolated());
        auto surfaceBaseFixing =
            ZeroInflation::cpiFixing(*index.currentLink(), volatilitySurface_->referenceDate(),
                                     volatilitySurface_->observationLag(), volatilitySurface_->indexIsInterpolated());
        auto ttmSurface =
            inflationYearFraction(volatilitySurface_->frequency(), volatilitySurface_->indexIsInterpolated(),
                                  volatilitySurface_->dayCounter(), surfaceBaseDate, optionObservationDate);
        Real strikeZeroRate = pow(optionBaseFixing / surfaceBaseFixing * strike, 1.0 / ttmSurface) - 1.0;
        stdDev = std::sqrt(volatilitySurface_->totalVariance(optionObservationDate, strikeZeroRate, 0 * Days));
    }
    results_.value = bachelierBlackFormula(arguments_.type, strike, atmGrowth, stdDev, d);
}

Rate CPIBachelierCapFloorEngine::indexFixing(const Date& observationDate, const Date& payDate) const {
    // you may want to modify the interpolation of the index
    // this gives you the chance
    Rate I1;
    // what interpolation do we use? Index / flat / linear
    if (arguments_.observationInterpolation == CPI::AsIndex) {
        I1 = arguments_.infIndex->fixing(observationDate);

    } else {
        // work out what it should be

        std::pair<Date, Date> dd = inflationPeriod(observationDate, arguments_.infIndex->frequency());
        Real indexStart = arguments_.infIndex->fixing(dd.first);
        if (arguments_.observationInterpolation == CPI::Linear) {
            std::pair<Date, Date> cpnInflationPeriod = inflationPeriod(payDate, arguments_.infIndex->frequency());
            // linear interpolation
            Real indexEnd = arguments_.infIndex->fixing(dd.second + Period(1, Days));
            I1 = indexStart +
                 (indexEnd - indexStart) * (payDate - cpnInflationPeriod.first) /
                     (Real)((cpnInflationPeriod.second + Period(1, Days)) -
                            cpnInflationPeriod.first); // can't get to next period's value within current period
        } else {
            // no interpolation, i.e. flat = constant, so use start-of-period value
            I1 = indexStart;
        }
    }
    return I1;
}

} // namespace QuantExt
