/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/commodityoption.hpp>

#include <boost/make_shared.hpp>

#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>

#include <qle/termstructures/pricetermstructureadapter.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

boost::shared_ptr<QuantLib::PricingEngine> CommodityOptionEngineBuilder::engineImpl(
    const string& commodityName, const Currency& ccy) {

    // Create the commodity yield curve for the process
    Handle<Quote> commoditySpot = market_->commoditySpot(commodityName, configuration(MarketContext::pricing));
    Handle<PriceTermStructure> priceCurve = market_->commodityPriceCurve(commodityName, configuration(MarketContext::pricing));
    Handle<YieldTermStructure> discountCurve = market_->discountCurve(ccy.code(), configuration(MarketContext::pricing));

    Handle<YieldTermStructure> yield = Handle<YieldTermStructure>(boost::make_shared<PriceTermStructureAdapter>(
        *commoditySpot, *priceCurve, *discountCurve));

    // Create the option engine
    boost::shared_ptr<GeneralizedBlackScholesProcess> process = boost::make_shared<GeneralizedBlackScholesProcess>(
        commoditySpot, yield, discountCurve, market_->commodityVolatility(commodityName, configuration(MarketContext::pricing)));

    return boost::make_shared<QuantLib::AnalyticEuropeanEngine>(process, discountCurve);
}

}
}
