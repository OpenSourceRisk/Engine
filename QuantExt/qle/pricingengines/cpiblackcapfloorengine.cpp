/*
 Copyright (C) 2016,2021,2022 Quaternion Risk Management Ltd
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

CPICapFloorEngine::CPICapFloorEngine(const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                     const QuantLib::Handle<QuantLib::CPIVolatilitySurface>& surface,
                                     const bool ttmFromLastAvailableFixing)
    : discountCurve_(discountCurve), volatilitySurface_(surface),
      ttmFromLastAvailableFixing_(ttmFromLastAvailableFixing) {
    registerWith(discountCurve_);
    registerWith(volatilitySurface_);
}


void CPICapFloorEngine::setVolatility(const QuantLib::Handle<QuantLib::CPIVolatilitySurface>& surface) {
    if (!volatilitySurface_.empty())
        unregisterWith(volatilitySurface_);
    volatilitySurface_ = surface;
    registerWith(volatilitySurface_);
    update();
}

void CPICapFloorEngine::calculate() const { 
    Date maturity = arguments_.payDate;
    
    auto index = arguments_.index;
    DiscountFactor d = arguments_.nominal * discountCurve_->discount(maturity);

    bool isInterpolated = arguments_.observationInterpolation == CPI::Linear ||
                          (arguments_.observationInterpolation == CPI::AsIndex && index->interpolated());

    Date optionObservationDate = QuantExt::ZeroInflation::fixingDate(arguments_.payDate, arguments_.observationLag,
                                                                     index->frequency(), isInterpolated);

    Date optionBaseDate = QuantExt::ZeroInflation::fixingDate(arguments_.startDate, arguments_.observationLag,
                                                              index->frequency(), isInterpolated);
    
    Real optionBaseFixing = arguments_.baseCPI == Null<Real>()
                          ? ZeroInflation::cpiFixing(index, arguments_.startDate,
                                                     arguments_.observationLag, isInterpolated)
                          : arguments_.baseCPI;

    Real atmCPIFixing = ZeroInflation::cpiFixing(index, maturity,
                                                 arguments_.observationLag, isInterpolated);

    Time timeToMaturityFromInception =
        inflationYearFraction(index->frequency(), isInterpolated, index->zeroInflationTermStructure()->dayCounter(),
                              optionBaseDate, optionObservationDate);

    Real atmGrowth = atmCPIFixing / optionBaseFixing;
    Real strike = std::pow(1.0 + arguments_.strike, timeToMaturityFromInception); 

    auto lastKnownFixingDate = ZeroInflation::lastAvailableFixing(*index, volatilitySurface_->referenceDate());
    auto observationPeriod = inflationPeriod(optionObservationDate, index->frequency());
    auto requiredFixing = isInterpolated ? observationPeriod.first : observationPeriod.second + 1 * Days;
    

    // if time from base <= 0 the fixing is already known and stdDev is zero, return the intrinsic value
    Real stdDev = 0.0;
    Real vol = 0.0;
    Real strikeZeroRate = 0.0;
    Real ttmFromSurfaceBaseDate = 0.0;
    Real surfaceBaseFixing = 0.0;
    if (requiredFixing > lastKnownFixingDate) {
        // For reading volatility in the current market volatiltiy structure
        // baseFixingSwap(T0) * pow(1 + strikeRate(T0), T-T0) = StrikeIndex = baseFixing(t) * pow(1 + strikeRate(t), T-t),
        // solve for strikeRate(t):
        surfaceBaseFixing =
            ZeroInflation::cpiFixing(index, volatilitySurface_->baseDate(),
                                     0 * Days, volatilitySurface_->indexIsInterpolated());
        ttmFromSurfaceBaseDate =
            inflationYearFraction(volatilitySurface_->frequency(), volatilitySurface_->indexIsInterpolated(),
            index->zeroInflationTermStructure()->dayCounter(), volatilitySurface_->baseDate(), optionObservationDate);
        strikeZeroRate = pow(optionBaseFixing / surfaceBaseFixing * strike, 1.0 / ttmFromSurfaceBaseDate) - 1.0;
        vol = volatilitySurface_->volatility(optionObservationDate, strikeZeroRate, 0 * Days);
        if (ttmFromLastAvailableFixing_) {
            auto ttm =
                inflationYearFraction(volatilitySurface_->frequency(), volatilitySurface_->indexIsInterpolated(),
                                      volatilitySurface_->dayCounter(), lastKnownFixingDate, optionObservationDate);
            stdDev = std::sqrt(ttm * vol * vol);
        } else {
            stdDev = std::sqrt(volatilitySurface_->totalVariance(optionObservationDate, strikeZeroRate, 0 * Days));
        }
    }
    results_.value = optionPriceImpl(arguments_.type, strike, atmGrowth, stdDev, d);

    results_.additionalResults["npv"] = results_.value;
    results_.additionalResults["strike"] = strike;
    results_.additionalResults["forward"] = atmGrowth;
    results_.additionalResults["stdDev"] = stdDev;
    results_.additionalResults["discount"] = d;
    results_.additionalResults["vol"] = vol;
    results_.additionalResults["timeToExpiry"] = stdDev * stdDev / (vol * vol);
    results_.additionalResults["BaseDate_trade"] = optionBaseDate;        
    results_.additionalResults["BaseDate_today"] = volatilitySurface_->baseDate();
    results_.additionalResults["FixingDate"] = optionObservationDate;        
    results_.additionalResults["PaymentDate"] = maturity;
    results_.additionalResults["BaseCPI_trade"] = optionBaseFixing;
    results_.additionalResults["BaseCPI_today"] = surfaceBaseFixing;
    results_.additionalResults["ForwardCPI"] = atmCPIFixing;
    results_.additionalResults["strike_asof_trade"] = arguments_.strike;
    results_.additionalResults["strike_asof_today"] = strikeZeroRate;
    results_.additionalResults["timeToExpiry_from_trade_baseDate"] = timeToMaturityFromInception;
    results_.additionalResults["timeToExpiry_from_todays_baseDate"] = ttmFromSurfaceBaseDate;



         
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



double CPIBlackCapFloorEngine::optionPriceImpl(QuantLib::Option::Type type, double strike, double forward,
                                               double stdDev, double discount) const {
    return blackFormula(type, strike, forward, stdDev, discount);
}
} // namespace QuantExt
