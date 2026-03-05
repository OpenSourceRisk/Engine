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
    // Compute currentNotional: max current cashflow amount across legs, converted to notional currency
    Date asof = Settings::instance().evaluationDate();
    Real currentNotional = Null<Real>();

    for (Size i = 0; i < arguments_.legs.size(); ++i) {
        for (Size j = 0; j < arguments_.legs[i].size(); ++j) {
            QuantLib::ext::shared_ptr<CashFlow> flow = arguments_.legs[i][j];
            if (flow->date() > asof) {
                Real amount = flow->amount();
                // Convert to notional currency
                auto it = notionalFxQuotes_.find(arguments_.currency[i].code());
                QL_REQUIRE(it != notionalFxQuotes_.end(),
                           "No notional FX quote found for currency " << arguments_.currency[i].code());
                QL_REQUIRE(!it->second.empty(),
                           "Invalid notional FX quote for currency " << arguments_.currency[i].code());
                amount *= it->second->value();
                if (currentNotional == Null<Real>())
                    currentNotional = amount;
                else
                    currentNotional = std::max(currentNotional, amount);
                break; // move to next leg
            }
        }
    }

    if (currentNotional != Null<Real>()) {
        results_.additionalResults["currentNotional"] = currentNotional;
        results_.additionalResults["notionalCurrency"] = notionalCurrency_.code();
    }
}

} // namespace QuantExt
