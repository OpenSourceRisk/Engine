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

#include <ored/portfolio/builders/cpicapfloor.hpp>
#include <ored/utilities/log.hpp>

//#include <ql/experimental/inflation/cpicapfloorengines.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/pricingengines/cpicapfloorengines.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

boost::shared_ptr<PricingEngine> CPICapFloorEngineBuilder::engineImpl(const string& indexName, const string& capFloor) {
    Handle<QuantLib::CPICapFloorTermPriceSurface> ps =
        market_->cpiInflationCapFloorPriceSurface(indexName, configuration(MarketContext::pricing));
    QL_REQUIRE(!ps.empty(), "engineFactory error: cap/floor price surface not found for index " << indexName);

    return boost::make_shared<QuantExt::InterpolatingCPICapFloorEngine>(ps);
}

boost::shared_ptr<PricingEngine> CPIBlackCapFloorEngineBuilder::engineImpl(const string& indexName,
                                                                           const string& capFloor) {
    Handle<ZeroInflationIndex> index = market_->zeroInflationIndex(indexName, configuration(MarketContext::pricing));
    QL_REQUIRE(!index.empty(), "zero inflation index is empry");
    Handle<YieldTermStructure> nominalTS = index->zeroInflationTermStructure()->nominalTermStructure();
    QL_REQUIRE(!nominalTS.empty(), "nominal term structure is empry");

    Handle<QuantLib::CPIVolatilitySurface> ps;
    // Choose vol surface depending on product type
    if (capFloor == "Cap")
        ps = market_->cpiInflationCapVolatilitySurface(indexName, configuration(MarketContext::pricing));
    else if (capFloor == "Floor")
        ps = market_->cpiInflationFloorVolatilitySurface(indexName, configuration(MarketContext::pricing));
    else
        QL_FAIL("instrument type " << capFloor << " not covered");

    QL_REQUIRE(!ps.empty(), "engineFactory error: cap/floor volatility surface not found for index " << indexName);

    return boost::make_shared<QuantExt::CPIBlackCapFloorEngine>(nominalTS, ps);
}

} // namespace data
} // namespace ore
