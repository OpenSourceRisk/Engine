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

#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/utilities/log.hpp>

#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<PricingEngine> CapFloorEngineBuilder::engineImpl(const std::string& index) {
    string ccyCode = parseIborIndex(index)->currency().code();
    Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
    Handle<OptionletVolatilityStructure> ovs = market_->capFloorVol(index, configuration(MarketContext::pricing));

    switch (ovs->volatilityType()) {
    case ShiftedLognormal:
        LOG("Build BlackCapFloorEngine for index " << index);
        return QuantLib::ext::make_shared<BlackCapFloorEngine>(yts, ovs, ovs->displacement());
        break;
    case Normal:
        LOG("Build BachelierCapFloorEngine for index " << index);
        return QuantLib::ext::make_shared<BachelierCapFloorEngine>(yts, ovs);
        break;
    default:
        QL_FAIL("Caplet volatility type, " << ovs->volatilityType() << ", not covered in EngineFactory");
        return nullptr; // avoid gcc warning
        break;
    }
}
} // namespace data
} // namespace ore
