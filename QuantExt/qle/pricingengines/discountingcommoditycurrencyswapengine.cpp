/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

#include <qle/pricingengines/discountingcommoditycurrencyswapengine.hpp>

#include <ql/cashflows/cashflows.hpp>

namespace QuantExt {

DiscountingCommodityCurrencySwapEngine::DiscountingCommodityCurrencySwapEngine(
    const std::vector<Handle<YieldTermStructure>>& discountCurves, const std::vector<Handle<Quote>>& fxQuotes,
    const std::vector<Currency>& currencies, const Currency& npvCurrency, const Currency& notionalCurrency,
    const std::map<std::string, Handle<Quote>>& notionalFxQuotes, QuantLib::ext::optional<bool> includeSettlementDateFlows,
    Date settlementDate, Date npvDate, const std::vector<Date>& spotFXSettleDateVec)
    : DiscountingCurrencySwapEngine(discountCurves, fxQuotes, currencies, npvCurrency, includeSettlementDateFlows,
                                    settlementDate, npvDate, spotFXSettleDateVec),
      notionalCurrency_(notionalCurrency), notionalFxQuotes_(notionalFxQuotes) {
    for (const auto& ccy : currencies)
        QL_REQUIRE(notionalFxQuotes_.find(ccy.code()) != notionalFxQuotes_.end(),
                   "No notional FX quote provided for currency " << ccy.code());
    for (const auto& [ccy, quote] : notionalFxQuotes_)
        registerWith(quote);
}

void DiscountingCommodityCurrencySwapEngine::calculate() const {
    DiscountingCurrencySwapEngine::calculate();
    // Compute currentNotional: max first future cashflow amount across legs, converted to notional currency
    // Compute aggregatedNotional: max total future cashflow amount across legs, converted to notional currency
    Date asof = Settings::instance().evaluationDate();
    Real currentNotional = 0.0;
    Real aggregatedNotional = 0.0;
    bool found = false;
    for (Size i = 0; i < arguments_.legs.size(); ++i) {
        auto it = notionalFxQuotes_.find(arguments_.currency[i].code());
        QL_REQUIRE(it != notionalFxQuotes_.end(),
                   "No notional FX quote found for currency " << arguments_.currency[i].code());
        QL_REQUIRE(!it->second.empty(),
                   "Invalid notional FX quote for currency " << arguments_.currency[i].code());
        Real fx = it->second->value();
        // Current notional: first future cashflow
        auto cf = CashFlows::nextCashFlow(arguments_.legs[i], false, asof);
        if (cf != arguments_.legs[i].end()) {
            Real amount = (*cf)->amount() * fx;
            currentNotional = std::max(currentNotional, amount);
            found = true;
        }
        // Aggregated notional: sum of all future cashflows
        Real legAggregatedNotional = 0.0;
        for (auto j = cf; j != arguments_.legs[i].end(); ++j) {
            legAggregatedNotional += (*j)->amount() * fx;
        }
        aggregatedNotional = std::max(aggregatedNotional, legAggregatedNotional);
    }

    if (found) {
        results_.additionalResults["currentNotional"] = currentNotional;
        results_.additionalResults["notionalCurrency"] = notionalCurrency_.code();
        results_.additionalResults["aggregatedNotional"] = aggregatedNotional;
    }
}

} // namespace QuantExt
