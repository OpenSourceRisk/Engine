/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/pricingengines/cpibacheliercapfloorengine.hpp>
#include <qle/utilities/inflation.hpp>
#include <boost/make_shared.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<PricingEngine> CpiCapFloorEngineBuilder::engineImpl(const string& indexName) {
    Handle<ZeroInflationIndex> cpiIndex = market_->zeroInflationIndex(indexName, configuration(MarketContext::pricing));
    // QL_REQUIRE(!cpiIndex.empty(), "engineFactory error, cpi index " << indexName << " not found");
    std::string ccyCode = cpiIndex->currency().code();
    Handle<YieldTermStructure> discountCurve = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
    Handle<QuantLib::CPIVolatilitySurface> ovs =
        market_->cpiInflationCapFloorVolatilitySurface(indexName, configuration(MarketContext::pricing));
    // QL_REQUIRE(!ovs.empty(),
    //            "engineFactory error: cpi cap/floor vol surface for index " << indexName << " not found");
    bool useLastFixingDate =
        parseBool(engineParameter("useLastFixingDate", std::vector<std::string>(), false, "false"));

    bool isLogNormal = QuantExt::ZeroInflation::isCPIVolSurfaceLogNormal(ovs.currentLink());

    if (isLogNormal) {
        return QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(discountCurve, ovs, useLastFixingDate);
    } else {
        return QuantLib::ext::make_shared<QuantExt::CPIBachelierCapFloorEngine>(discountCurve, ovs, useLastFixingDate);
    }
    
}
} // namespace data
} // namespace ore
