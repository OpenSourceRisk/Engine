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

#include <ored/portfolio/builders/yoycapfloor.hpp>
#include <ored/utilities/log.hpp>

#include <ql/pricingengines/inflation/inflationcapfloorengines.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

boost::shared_ptr<PricingEngine> YoYCapFloorEngineBuilder::engineImpl(const string& indexName) {
    boost::shared_ptr<YoYInflationIndex> yoyTs = market_->yoyInflationIndex(indexName, configuration(MarketContext::pricing)).currentLink();
    QL_REQUIRE(yoyTs, "engineFactory error: yield term structure not found for currency " << indexName);
    boost::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> ovs = market_->yoyCapFloorVol(indexName, configuration(MarketContext::pricing)).currentLink();
    QL_REQUIRE(ovs, "engineFactory error: caplet volatility structure not found for currency " << indexName);
    Handle<QuantLib::YoYOptionletVolatilitySurface> qovs(boost::dynamic_pointer_cast<QuantLib::YoYOptionletVolatilitySurface>(ovs));

    switch (ovs->volatilityType()) {
    case ShiftedLognormal:
        //Should have a displacement
        LOG("Build YoYInflationBlackCapFloorEngine for inflation index " << indexName);
        //return boost::make_shared<YoYInflationBlackCapFloorEngine>(yts, ovs, ovs->displacement());
        return boost::make_shared<YoYInflationBlackCapFloorEngine>(yoyTs, qovs);
        break;
    case Normal:
        LOG("Build YoYInflationBachelierCapFloorEngine for inflation index " << indexName);
        return boost::make_shared<YoYInflationBachelierCapFloorEngine>(yoyTs, qovs);
        break;
    default:
        QL_FAIL("Caplet volatility type, " << ovs->volatilityType() << ", not covered in EngineFactory");
        break;
    }
}
} // namespace data
} // namespace ore
