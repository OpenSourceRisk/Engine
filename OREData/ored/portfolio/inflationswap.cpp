/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/portfolio/inflationswap.hpp>

namespace ore {
namespace data {

InflationSwap::InflationSwap(const Envelope& env, const vector<LegData>& legData)
    : Swap(env, legData, "InflationSwap") {}

InflationSwap::InflationSwap(const Envelope& env, const LegData& leg0, const LegData& leg1)
    : Swap(env, leg0, leg1, "InflationSwap") {}

void InflationSwap::checkInflationSwap(const vector<LegData>& legData) {
    // An Inflation Swap must have at least one CPI or YY leg
    bool hasInflationLeg = false;
    for (Size i = 0; i < legData.size(); i++)
        if (legData[i].legType() == "CPI" || legData[i].legType() == "YY") {
            hasInflationLeg = true;
            break;
        }
    QL_REQUIRE(hasInflationLeg, "InflationSwap must have at least one inflation leg (e.g. CPI, YY)");
}

void InflationSwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("InflationSwap::build() called for " << id());

    checkInflationSwap(legData_);

    Swap::build(engineFactory);

    // ISDA taxonomy, override Swap settings Base Product and add Transaction here
    additionalData_["isdaBaseProduct"] = string("Inflation Swap");
    auto it1 = legTypeCount_.find("CPI");
    auto it2 = legTypeCount_.find("YY");
    if (it1 != legTypeCount_.end())
        additionalData_["isdaTransaction"] = string("Zero Coupon");  
    else if (it2 != legTypeCount_.end())
        additionalData_["isdaTransaction"] = string("Year on Year");  
}

} // namespace data
} // namespace ore
