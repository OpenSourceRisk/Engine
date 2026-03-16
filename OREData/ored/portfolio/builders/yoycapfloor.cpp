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

#include <qle/pricingengines/inflationcapfloorengines.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<PricingEngine> YoYCapFloorEngineBuilder::engineImpl(const string& indexName) {
    Handle<YoYInflationIndex> yoyTs = market_->yoyInflationIndex(indexName, configuration(MarketContext::pricing));
    Handle<YieldTermStructure> discount =
        market_->discountCurve(yoyTs->currency().code(), configuration(MarketContext::pricing));
    Handle<QuantExt::YoYOptionletVolatilitySurface> ovs =
        market_->yoyCapFloorVol(indexName, configuration(MarketContext::pricing));
    if (ovs.empty())
        return QuantLib::ext::make_shared<QuantExt::YoYInflationBlackCapFloorEngine>(
            *yoyTs, Handle<QuantLib::YoYOptionletVolatilitySurface>(), discount);
    switch (ovs->volatilityType()) {
    case ShiftedLognormal:
        if (ovs->displacement() == 0.0) {
            LOG("Build YoYInflationBlackCapFloorEngine for inflation index " << indexName);
            return QuantLib::ext::make_shared<QuantExt::YoYInflationBlackCapFloorEngine>(
                *yoyTs, Handle<QuantLib::YoYOptionletVolatilitySurface>(ovs), discount);
            break;
        } else {
            LOG("Build YoYInflationUnitDisplacedBlackCapFloorEngine for inflation index " << indexName);
            return QuantLib::ext::make_shared<QuantExt::YoYInflationUnitDisplacedBlackCapFloorEngine>(
                *yoyTs, Handle<QuantLib::YoYOptionletVolatilitySurface>(ovs), discount);
            break;
        }
    case Normal:
        LOG("Build YoYInflationBachelierCapFloorEngine for inflation index " << indexName);
        return QuantLib::ext::make_shared<QuantExt::YoYInflationBachelierCapFloorEngine>(
            *yoyTs, Handle<QuantLib::YoYOptionletVolatilitySurface>(ovs), discount);
        break;
    default:
        QL_FAIL("Caplet volatility type, " << ovs->volatilityType() << ", not covered in EngineFactory");
        return nullptr; // avoid gcc warning
        break;
    }
}
} // namespace data
} // namespace ore
