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
    // Cross Currency Swap legs must be either Fixed, Floating or Cashflow and exactly two of Fixed and/or Floating
    vector<Size> legDataIdx;
    for (Size i = 0; i < legData_.size(); i++) {
        if (legData_[i].legType() == "Fixed" || legData_[i].legType() == "Floating")
            legDataIdx.push_back(i);
        else if (legData_[i].legType() == "Cashflow")
            continue;
        else
            QL_FAIL("CrossCurrencySwap leg #" << i + 1 << " must be Fixed, Floating or Cashflow");
    }
    QL_REQUIRE(legDataIdx.size() == 2,
               "A Cross Currency Swap must have 2 legs that are either Fixed or Floating: " + id());

    // Check leg currencies
    Currency legCcy0 = parseCurrencyWithMinors(legData_[legDataIdx[0]].currency());
    Currency legIndexCcy0;
    if (legData_[legDataIdx[0]].legType() == "Fixed") {
        legIndexCcy0 = legCcy0;
    } else if (legData_[legDataIdx[0]].legType() == "Floating") {
        auto floatingLeg = boost::dynamic_pointer_cast<FloatingLegData>(legData_[legDataIdx[0]].concreteLegData());
        legIndexCcy0 = parseIborIndex(floatingLeg->index())->currency();
    }

    Currency legCcy1;
    legCcy1 = parseCurrencyWithMinors(legData_[legDataIdx[1]].currency());
    // Require leg currencies to be different. If they are the same, we do a further check of FloatingLeg
    // underlying currencies, in which case we still call this a cross currency swap.
    if (legCcy0 == legCcy1) {
        Currency legIndexCcy1;
        if (legData_[legDataIdx[1]].legType() == "Fixed") {
            legIndexCcy1 = legCcy1;
        } else if (legData_[legDataIdx[1]].legType() == "Floating") {
            auto floatingLeg = boost::dynamic_pointer_cast<FloatingLegData>(legData_[legDataIdx[1]].concreteLegData());
            legIndexCcy1 = parseIborIndex(floatingLeg->index()) ->currency();
        }
        QL_REQUIRE(legIndexCcy0 != legIndexCcy1, "Cross currency swap legs must have different currencies.");
    }
}

void CrossCurrencySwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CrossCurrencySwap::build() called for " << id());

    Swap::build(engineFactory);

    checkCrossCurrencySwap();
}

} // namespace data
} // namespace ore
