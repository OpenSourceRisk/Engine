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
#include <ored/portfolio/structuredtradewarning.hpp>
#include <ored/utilities/indexparser.hpp>


namespace ore {
namespace data {

CrossCurrencySwap::CrossCurrencySwap(const Envelope& env, const vector<LegData>& legData) : Swap(env, legData, "CrossCurrencySwap") {}

CrossCurrencySwap::CrossCurrencySwap(const Envelope& env, const LegData& leg0, const LegData& leg1)
    : Swap(env, leg0, leg1, "CrossCurrencySwap") {}

void CrossCurrencySwap::checkCrossCurrencySwap() {
    // This function will attempt to set legIndexCcy to the other ccy from the first Indexing index.
    auto getIndexingCurrency = [this](const LegData& legData, const Currency& legCcy, Currency& legIndexCcy) {
        vector<Indexing> indexings = legData.indexing();
        if (!indexings.empty() && indexings.front().hasData()) {
            Indexing indexing = indexings.front();
            if (!boost::starts_with(indexing.index(), "FX-")) {
                StructuredTradeWarningMessage(
                    tradeType(), id(), "Trade validation (checkCrossCurrencySwap)",
                    "Could not set fixed leg currency to Indexing currency for trade validation. Index (" +
                        indexing.index() + ") should start with 'FX-'")
                    .log();
                return;
            }

            auto index = parseFxIndex(indexing.index());
            Currency srcCurrency = index->sourceCurrency();
            Currency tgtCurrency = index->targetCurrency();

            if (legCcy != srcCurrency && legCcy != tgtCurrency) {
                StructuredTradeWarningMessage(tradeType(), id(), "Trade validation (checkCrossCurrencySwap)",
                                                   "Could not set fixed leg currency to Indexing currency for trade "
                                                   "validation. Expected the leg currency (" +
                                                       legCcy.code() +
                                                       ") be equal to either of the currencies in the index (" +
                                                  indexing.index() + ")")
                    .log();
                return;
            }

            legIndexCcy = legCcy == srcCurrency ? tgtCurrency : srcCurrency;
        }
    };

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

    const LegData& legData0 = legData_[legDataIdx[0]];
    const LegData& legData1 = legData_[legDataIdx[1]];

    // Check leg currencies
    const Currency legCcy0 = parseCurrencyWithMinors(legData0.currency());
    const Currency legCcy1 = parseCurrencyWithMinors(legData1.currency());

    // Require leg currencies to be different. If they are the same, we do a further check of the underlying currencies
    // (Indexings for Fixed leg; Floating leg index for Floating leg) and compare these.
    if (legCcy0 == legCcy1) {

        // Get relevant index currency for the first leg - defaults to the leg ccy
        Currency legIndexCcy0 = legCcy0;
        if (legData0.legType() == "Fixed") {
            getIndexingCurrency(legData0, legCcy0, legIndexCcy0);
        } else if (legData0.legType() == "Floating") {
            auto floatingLeg = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(legData0.concreteLegData());
            if (floatingLeg)
                legIndexCcy0 = parseIborIndex(floatingLeg->index())->currency();
        }

        // Get relevant index currency for the second leg - defaults to the leg ccy
        Currency legIndexCcy1 = legCcy1;
        if (legData1.legType() == "Fixed") {
            getIndexingCurrency(legData1, legCcy1, legIndexCcy1);
        } else if (legData1.legType() == "Floating") {
            auto floatingLeg = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(legData1.concreteLegData());
            if (floatingLeg)
                legIndexCcy1 = parseIborIndex(floatingLeg->index()) ->currency();
        }

        QL_REQUIRE(legIndexCcy0 != legIndexCcy1, "Cross currency swap legs must have different currencies.");
    }
}

void CrossCurrencySwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    DLOG("CrossCurrencySwap::build() called for " << id());

    Swap::build(engineFactory);

    checkCrossCurrencySwap();
}

} // namespace data
} // namespace ore
