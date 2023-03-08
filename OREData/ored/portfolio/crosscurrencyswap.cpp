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

#include <ored/portfolio/crosscurrencyswap.hpp>

namespace ore {
namespace data {

CrossCurrencySwap::CrossCurrencySwap(const Envelope& env, const vector<LegData>& legData) : Swap(env, legData, "CrossCurrencySwap") {}

CrossCurrencySwap::CrossCurrencySwap(const Envelope& env, const LegData& leg0, const LegData& leg1)
    : Swap(env, leg0, leg1, "CrossCurrencySwap") {}

void CrossCurrencySwap::checkCrossCurrencySwap() {
    // A Cross Currency Swap must have 2 legs (either Fixed or Floating)
    QL_REQUIRE(legData_.size() >= 2, "A Cross Currency Swap must have at least 2 legs - Trade: " + id());
    for (Size i = 0; i < legData_.size(); i++)
        QL_REQUIRE(legData_[i].legType() == "Fixed" || legData_[i].legType() == "Floating",
                   "CrossCurrencySwap leg #" << i << " must be either Fixed or Floating");

    // Check leg currencies
    Currency legCcy0 = parseCurrencyWithMinors(legData_[0].currency());
    Currency legIndexCcy0;
    if (legData_[0].legType() == "Fixed") {
        legIndexCcy0 = legCcy0;
    } else if (legData_[0].legType() == "Floating") {
        auto floatingLeg = boost::dynamic_pointer_cast<FloatingLegData>(legData_[0].concreteLegData());
        legIndexCcy0 = parseIborIndex(floatingLeg->index())->currency();
    }

    Currency legCcy, legIndexCcy;
    for (Size i = 1; i < legData_.size(); i++) {
        legCcy = parseCurrencyWithMinors(legData_[i].currency());
        // Require leg currencies to be different. If they are the same, we do a further check of FloatingLeg
        // underlying currencies, in which case we still call this a cross currency swap.
        if (legCcy0 == legCcy) {
            if (legData_[i].legType() == "Fixed") {
                legIndexCcy = legCcy;
            } else if (legData_[i].legType() == "Floating") {
                auto floatingLeg = boost::dynamic_pointer_cast<FloatingLegData>(legData_[i].concreteLegData());
                legIndexCcy = parseIborIndex(floatingLeg->index()) ->currency();
            }
            QL_REQUIRE(legIndexCcy0 != legIndexCcy, "Cross currency swap legs must have different currencies.");
        }
    }
}

void CrossCurrencySwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CrossCurrencySwap::build() called for " << id());

    Swap::build(engineFactory);

    checkCrossCurrencySwap();
}

} // namespace data
} // namespace ore
