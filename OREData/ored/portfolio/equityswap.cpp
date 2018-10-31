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

#include <ored/portfolio/equityswap.hpp>

namespace ore {
namespace data {

EquitySwap::EquitySwap(const Envelope& env, const vector<LegData>& legData) :Swap(env, legData, "EquitySwap") {
    checkEquitySwap(legData);
}

EquitySwap::EquitySwap(const Envelope& env, const LegData& leg0, const LegData& leg1) : Swap(env, leg0, leg1, "EquitySwap") {
    vector<LegData> legData = { leg0, leg1 };
    checkEquitySwap(legData);
}

void EquitySwap::checkEquitySwap(const vector<LegData>& legData) {
    // An Equity Swap must have 2 legs - 1 an Equity Leg and the other an IR Leg either Fixed or Floating
    bool hasEquityLeg = false;
    bool hasIRLeg = false;
    for (auto ld : legData) {
        if (ld.legType() == "Equity") {
            hasEquityLeg = true;
        }
        else if (ld.legType() == "Fixed" || ld.legType() == "Floating") {
            hasIRLeg = true;
        }
    }
    QL_REQUIRE((legData.size() == 2) && hasEquityLeg && hasIRLeg,
        "An Equity Swap must have 2 legs, an Equity Leg and an IR Leg - Trade: " + id());
}

} // namespace data
} // namespace ore
