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

/*! \file qle/pricingengines/discountingcommoditycurrencyswapengine.hpp
    \brief discounting commodity cross-currency swap engine

        \ingroup engines
*/

#pragma once

#include <qle/pricingengines/discountingcurrencyswapengine.hpp>

namespace QuantExt {

//! Discounting engine for cross-currency commodity swaps
/*! Derives from DiscountingCurrencySwapEngine and adds currentNotional
    and notionalCurrency to the additional results.

    \ingroup engines
*/
class DiscountingCommodityCurrencySwapEngine : public DiscountingCurrencySwapEngine {
public:
    DiscountingCommodityCurrencySwapEngine(
        const std::vector<QuantLib::Handle<QuantLib::YieldTermStructure>>& discountCurves,
        const std::vector<QuantLib::Handle<QuantLib::Quote>>& fxQuotes,
        const std::vector<QuantLib::Currency>& currencies, const QuantLib::Currency& npvCurrency,
        QuantLib::ext::optional<bool> includeSettlementDateFlows = QuantLib::ext::nullopt,
        QuantLib::Date settlementDate = QuantLib::Date(), QuantLib::Date npvDate = QuantLib::Date(),
        const std::vector<QuantLib::Date>& spotFXSettleDateVec = std::vector<QuantLib::Date>());

    void calculate() const override;
};

} // namespace QuantExt
