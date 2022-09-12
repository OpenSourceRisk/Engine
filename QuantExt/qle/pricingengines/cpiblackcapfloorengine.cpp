/*
 Copyright (C) 2016,2021 Quaternion Risk Management Ltd
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
#include <qle/utilities/inflation.hpp>

using namespace QuantLib;

namespace QuantExt {

CPIBlackCapFloorEngine::CPIBlackCapFloorEngine(const Handle<YieldTermStructure>& discountCurve,
                                               const Handle<CPIVolatilitySurface>& volatilitySurface,
                                               const bool measureTimeToExpiryFromLastAvailableFixing)
    : discountCurve_(discountCurve), volatilitySurface_(volatilitySurface),
      measureTimeFromLastAvailableFixing_(measureTimeToExpiryFromLastAvailableFixing) {
    registerWith(discountCurve_);
    registerWith(volatilitySurface_);
}

void CPIBlackCapFloorEngine::calculate() const { 
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

    Time timeToMaturityFromInception =
        inflationYearFraction(index->frequency(), isInterpolated, index->zeroInflationTermStructure()->dayCounter(),
                              optionBaseDate, optionObservationDate);

    Real atmGrowth = atmCPIFixing / optionBaseFixing;
    Real strike = std::pow(1.0 + arguments_.strike, timeToMaturityFromInception); 

    auto lastKnownFixingDate = ZeroInflation::lastAvailableFixing(*index.currentLink(), volatilitySurface_->referenceDate());
    auto observationPeriod = inflationPeriod(optionObservationDate, index->frequency());
    auto requiredFixing = isInterpolated ? observationPeriod.first : observationPeriod.second + 1 * Days;
    

    // if time from base <= 0 the fixing is already known and stdDev is zero, return the intrinsic value
    Real stdDev = 0.0;
    if (requiredFixing > lastKnownFixingDate) {
        // For reading volatility in the current market volatiltiy structure
        // baseFixingSwap(T0) * pow(1 + strikeRate(T0), T-T0) = StrikeIndex = baseFixing(t) * pow(1 + strikeRate(t), T-t),
        // solve for strikeRate(t):
        auto surfaceBaseFixing =
            ZeroInflation::cpiFixing(*index.currentLink(), volatilitySurface_->baseDate(),
                                     0 * Days, volatilitySurface_->indexIsInterpolated());
        auto ttmFromSurfaceBaseDate =
            inflationYearFraction(volatilitySurface_->frequency(), volatilitySurface_->indexIsInterpolated(),
            index->zeroInflationTermStructure()->dayCounter(), volatilitySurface_->baseDate(), optionObservationDate);
        Real strikeZeroRate = pow(optionBaseFixing / surfaceBaseFixing * strike, 1.0 / ttmFromSurfaceBaseDate) - 1.0;
        if (measureTimeFromLastAvailableFixing_) {
            auto vol = volatilitySurface_->volatility(optionObservationDate, strikeZeroRate, 0 * Days);
            auto ttm =
                inflationYearFraction(volatilitySurface_->frequency(), volatilitySurface_->indexIsInterpolated(),
                                      volatilitySurface_->dayCounter(), lastKnownFixingDate, optionObservationDate);
            stdDev = std::sqrt(ttm * vol * vol);
        } else {
            stdDev = std::sqrt(volatilitySurface_->totalVariance(optionObservationDate, strikeZeroRate, 0 * Days));
        }
    }
    results_.value = blackFormula(arguments_.type, strike, atmGrowth, stdDev, d);

    // std::cout << "CPIBlackCapFloorEngine ==========" << std::endl
    // 	      << "startDate     = " << QuantLib::io::iso_date(arguments_.startDate) << std::endl
    // 	      << "maturityDate  = " << QuantLib::io::iso_date(maturity) << std::endl
    // 	      << "effStartDate  = " << QuantLib::io::iso_date(effectiveStart) << std::endl
    // 	      << "effEndDate    = " << QuantLib::io::iso_date(effectiveMaturity) << std::endl
    // 	      << "lagDiff       = " << lagDiff << std::endl
    // 	      << "timeFromStart = " << timeFromStart << std::endl
    // 	      << "timeFromBase  = " << timeFromBase << std::endl
    // 	      << "baseFixing    = " << baseFixing << std::endl
    // 	      << "baseCPI       = " << baseCPI << std::endl
    // 	      << "K             = " << K << std::endl
    // 	      << "F             = " << F << std::endl
    // 	      << "stdDev        = " << stdDev << std::endl
    // 	      << "value         = " <<  results_.value << std::endl;
}

} // namespace QuantExt
