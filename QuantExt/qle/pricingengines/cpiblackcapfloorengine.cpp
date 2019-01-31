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

#include <iomanip>
#include <iostream>
#include <ql/pricingengines/blackformula.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>

using namespace QuantLib;

namespace QuantExt {

CPIBlackCapFloorEngine::CPIBlackCapFloorEngine(const Handle<YieldTermStructure>& discountCurve,
                                               const Handle<CPIVolatilitySurface>& volatilitySurface)
    : discountCurve_(discountCurve), volatilitySurface_(volatilitySurface) {
    registerWith(discountCurve_);
    registerWith(volatilitySurface_);
}

void CPIBlackCapFloorEngine::calculate() const {

    QL_REQUIRE(arguments_.observationInterpolation == QuantLib::CPI::AsIndex,
               "observation interpolation as index required");

    // Maturity adjustment for lag difference as in the QuantLib engine
    Period lagDiff = arguments_.observationLag - volatilitySurface_->observationLag();
    QL_REQUIRE(lagDiff >= Period(0, Months), "CPIBlackCapFloorEngine: "
                                             "lag difference must be non-negative: "
                                                 << lagDiff);
    // Date effectiveMaturity = arguments_.payDate - lagDiff;

    Date maturity = arguments_.payDate;
    Date effectiveMaturity = arguments_.payDate - arguments_.observationLag;
    if (!arguments_.infIndex->interpolated()) {
        std::pair<Date, Date> ipm = inflationPeriod(effectiveMaturity, arguments_.infIndex->frequency());
        effectiveMaturity = ipm.first;
    }

    DiscountFactor d = arguments_.nominal * discountCurve_->discount(arguments_.payDate);

    Date baseDate = arguments_.infIndex->zeroInflationTermStructure()->baseDate();
    Real baseFixing = arguments_.infIndex->fixing(baseDate);

    Date effectiveStart = arguments_.startDate - arguments_.observationLag;
    if (!arguments_.infIndex->interpolated()) {
        std::pair<Date, Date> ips = inflationPeriod(effectiveStart, arguments_.infIndex->frequency());
        effectiveStart = ips.first; 
    }

    Real timeFromStart =
        arguments_.infIndex->zeroInflationTermStructure()->dayCounter().yearFraction(effectiveStart, effectiveMaturity);
    Real timeFromBase =
        arguments_.infIndex->zeroInflationTermStructure()->dayCounter().yearFraction(baseDate, effectiveMaturity);
    Real K = pow(1.0 + arguments_.strike, timeFromStart);
    Real F = arguments_.infIndex->fixing(effectiveMaturity) / arguments_.baseCPI;

    // For reading volatility in the current market volatiltiy structure
    // baseFixing(T0) * pow(1 + strikeRate(T0), T-T0) = StrikeIndex = baseFixing(t) * pow(1 + strikeRate(t), T-t), solve for strikeRate(t):
    Real strikeZeroRate =
        pow(arguments_.baseCPI / baseFixing * pow(1.0 + arguments_.strike, timeFromStart), 1.0 / timeFromBase) - 1.0;
    Period obsLag = Period(0, Days); // Should be zero here if we use the lag difference to adjust maturity above
    Real stdDev = std::sqrt(volatilitySurface_->totalVariance(maturity, strikeZeroRate, obsLag));
    results_.value = blackFormula(arguments_.type, K, F, stdDev, d);

    // std::cout << std::setprecision(8) << "npv=" << results_.value << " TB=" << timeFromBase << " TS=" << timeFromStart
    //           << " K=" << K << " F=" << F << " SD=" << stdDev << " d=" << d << " " << baseFixing << " "
    //           << arguments_.baseCPI << std::endl;
}

} // namespace QuantExt
