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
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
//#include <iostream>

using namespace QuantLib;

namespace QuantExt {

CPIBlackCapFloorEngine::CPIBlackCapFloorEngine(const Handle<YieldTermStructure>& discountCurve,
                                               const Handle<CPIVolatilitySurface>& volatilitySurface)
    : discountCurve_(discountCurve), volatilitySurface_(volatilitySurface) {
    registerWith(discountCurve_);
    registerWith(volatilitySurface_);
}

void CPIBlackCapFloorEngine::calculate() const {

    QL_REQUIRE(
        arguments_.observationInterpolation == QuantLib::CPI::AsIndex ||
            (arguments_.observationInterpolation == QuantLib::CPI::Flat && !arguments_.infIndex->interpolated()) ||
            (arguments_.observationInterpolation == QuantLib::CPI::Linear && arguments_.infIndex->interpolated()),
        "observation interpolation as index required");

    // Maturity adjustment for lag difference as in the QuantLib engine
    Period lagDiff = arguments_.observationLag - volatilitySurface_->observationLag();
    QL_REQUIRE(lagDiff >= Period(0, Months), "CPIBlackCapFloorEngine: "
                                             "lag difference must be non-negative: "
                                                 << lagDiff);
    Date maturity = arguments_.payDate;
    DiscountFactor d = arguments_.nominal * discountCurve_->discount(maturity);

    Date effectiveMaturity = arguments_.payDate - arguments_.observationLag - lagDiff;
    if (!arguments_.infIndex->interpolated()) {
        std::pair<Date, Date> ipm = inflationPeriod(effectiveMaturity, arguments_.infIndex->frequency());
        effectiveMaturity = ipm.first;
    }

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
    // baseFixing(T0) * pow(1 + strikeRate(T0), T-T0) = StrikeIndex = baseFixing(t) * pow(1 + strikeRate(t), T-t), solve
    // for strikeRate(t):
    Real strikeZeroRate =
        pow(arguments_.baseCPI / baseFixing * pow(1.0 + arguments_.strike, timeFromStart), 1.0 / timeFromBase) - 1.0;
    Real stdDev = std::sqrt(volatilitySurface_->totalVariance(maturity, strikeZeroRate));
    results_.value = blackFormula(arguments_.type, K, F, stdDev, d);

    // std::cout << "CPIBlackCapFloorEngine ==========" << std::endl
    // 	      << "startDate     = " << QuantLib::io::iso_date(arguments_.startDate) << std::endl
    // 	      << "maturityDate  = " << QuantLib::io::iso_date(maturity) << std::endl
    // 	      << "effStartDate  = " << QuantLib::io::iso_date(effectiveStart) << std::endl
    // 	      << "effEndDate    = " << QuantLib::io::iso_date(effectiveMaturity) << std::endl
    // 	      << "lagDiff       = " << lagDiff << std::endl
    // 	      << "timeFromStart = " << timeFromStart << std::endl
    // 	      << "timeFromBase  = " << timeFromBase << std::endl
    // 	      << "baseFixing    = " << baseFixing << std::endl
    // 	      << "baseCPI       = " << arguments_.baseCPI << std::endl
    // 	      << "K             = " << K << std::endl
    // 	      << "F             = " << F << std::endl
    // 	      << "stdDev        = " << stdDev << std::endl
    // 	      << "value         = " <<  results_.value << std::endl;
}

} // namespace QuantExt
