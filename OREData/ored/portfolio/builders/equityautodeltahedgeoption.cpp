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

#include <ored/portfolio/builders/equityautodeltahedgeoption.hpp>

namespace ore {
namespace data {

using namespace QuantExt;

string EquityAutoDeltaHedgedOptionEngineBuilder::keyImpl(const string& equityName, const Currency& ccy) {
    return equityName + "/" + ccy.code();
}

QuantLib::ext::shared_ptr<PricingEngine>
EquityAutoDeltaHedgedOptionEngineBuilder::engineImpl(const string& equityName, const Currency& ccy) {
    string config = configuration(MarketContext::pricing);
    Handle<Quote> spot = market_->equitySpot(equityName, config);
    Handle<YieldTermStructure> discountCurve = market_->discountCurve(ccy.code(), config);
    Handle<YieldTermStructure> divCurve = market_->equityDividendCurve(equityName, config);
    Handle<YieldTermStructure> fcstCurve = market_->equityForecastCurve(equityName, config);
    Handle<BlackVolTermStructure> marketVol = market_->equityVol(equityName, config);
    Handle<QuantExt::EquityIndex2> eqIndex = market_->equityCurve(equityName, config);
    return QuantLib::ext::make_shared<QuantExt::AnalyticEuropeanEngineAutoDeltaHedge>(
        spot, discountCurve, divCurve, fcstCurve, marketVol, eqIndex);
}

} // namespace data
} // namespace ore
