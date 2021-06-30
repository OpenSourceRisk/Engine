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

using namespace QuantLib;

namespace QuantExt {

CPIBachelierCapFloorEngine::CPIBachelierCapFloorEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<CPIVolatilitySurface>& volatilitySurface)
    : discountCurve_(discountCurve), volatilitySurface_(volatilitySurface) {
    registerWith(discountCurve_);
    registerWith(volatilitySurface_);
}

void CPIBachelierCapFloorEngine::calculate() const {

    QL_REQUIRE(arguments_.observationInterpolation == QuantLib::CPI::AsIndex,
               "observation interpolation as index required");

    // Maturity adjustment for lag difference as in the QuantLib engine
    Period lagDiff = arguments_.observationLag - volatilitySurface_->observationLag();
    QL_REQUIRE(lagDiff >= Period(0, Months), "CPIBachelierCapFloorEngine: "
                                             "lag difference must be non-negative: "
                                                 << lagDiff);
    // Date effectiveMaturity = arguments_.payDate - lagDiff;

    Date maturity = arguments_.payDate;
    Date effectiveMaturity = arguments_.payDate - arguments_.observationLag;
    if (arguments_.observationInterpolation == QuantLib::CPI::AsIndex ||
        arguments_.observationInterpolation == QuantLib::CPI::Flat) {
        std::pair<Date, Date> ipm = inflationPeriod(effectiveMaturity, arguments_.infIndex->frequency());
        effectiveMaturity = ipm.first;
    }

    DiscountFactor d = arguments_.nominal * discountCurve_->discount(arguments_.payDate);

    Date baseDate = arguments_.infIndex->zeroInflationTermStructure()->baseDate();
    Real baseFixing = indexFixing(baseDate);

    Date effectiveStart = arguments_.startDate - arguments_.observationLag;
    if (arguments_.observationInterpolation == QuantLib::CPI::AsIndex ||
        arguments_.observationInterpolation == QuantLib::CPI::Flat) {
        std::pair<Date, Date> ips = inflationPeriod(effectiveStart, arguments_.infIndex->frequency());
        effectiveStart = ips.first;
    }

    Real timeFromStart =
        arguments_.infIndex->zeroInflationTermStructure()->dayCounter().yearFraction(effectiveStart, effectiveMaturity);
    Real timeFromBase =
        arguments_.infIndex->zeroInflationTermStructure()->dayCounter().yearFraction(baseDate, effectiveMaturity);
    Real K = pow(1.0 + arguments_.strike, timeFromStart);
    Real F = indexFixing(effectiveMaturity) / arguments_.baseCPI;

    // For reading volatility in the current market volatiltiy structure
    // baseFixing(T0) * pow(1 + strikeRate(T0), T-T0) = StrikeIndex = baseFixing(t) * pow(1 + strikeRate(t), T-t), solve
    // for strikeRate(t):
    Real strikeZeroRate =
        pow(arguments_.baseCPI / baseFixing * pow(1.0 + arguments_.strike, timeFromStart), 1.0 / timeFromBase) - 1.0;
    Period obsLag = Period(0, Days); // Should be zero here if we use the lag difference to adjust maturity above
    Real stdDev = std::sqrt(volatilitySurface_->totalVariance(maturity, strikeZeroRate, obsLag));

    results_.value = bachelierBlackFormula(arguments_.type, K, F, stdDev, d);
}

Rate CPIBachelierCapFloorEngine::indexFixing(const Date& d) const {
    // you may want to modify the interpolation of the index
    // this gives you the chance

    Rate I1;
    // what interpolation do we use? Index / flat / linear
    if (arguments_.observationInterpolation == CPI::AsIndex) {
        I1 = arguments_.infIndex->fixing(d);

    } else {
        // work out what it should be
        std::pair<Date, Date> dd = inflationPeriod(d, arguments_.infIndex->frequency());
        Real indexStart = arguments_.infIndex->fixing(dd.first);
        if (arguments_.observationInterpolation == CPI::Linear) {
            Real indexEnd = arguments_.infIndex->fixing(dd.second + Period(1, Days));
            // linear interpolation
            I1 = indexStart + (indexEnd - indexStart) * (d - dd.first) /
                                  (Real)((dd.second + Period(1, Days)) -
                                         dd.first); // can't get to next period's value within current period
        } else {
            // no interpolation, i.e. flat = constant, so use start-of-period value
            I1 = indexStart;
        }
    }
    return I1;
}

} // namespace QuantExt
