/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <ql/errors.hpp>
#include <ql/math/rounding.hpp>
#include <qle/cashflows/nettedcommoditycashflow.hpp>

using namespace QuantLib;

namespace QuantExt {

Real roundWithPrecision(Real value, Natural precision, Natural preRoundPrecision) {
    if (precision == Null<Natural>()) {
        return value;
    }

    // First, round to pre-round precision to avoid floating point problems
    QuantLib::ClosestRounding preRound(preRoundPrecision);
    value = preRound(value);

    // Then round to final precision
    QuantLib::ClosestRounding finalRound(precision);
    return finalRound(value);
}

NettedCommodityCashFlow::NettedCommodityCashFlow(
    const std::vector<ext::shared_ptr<CommodityCashFlow>>& underlyingCashflows, const std::vector<bool>& isPayer,
    Natural nettingPrecision)
    : CommodityCashFlow(0.0, 0.0, 1.0, false, nullptr, nullptr), underlyingCashflows_(underlyingCashflows),
      isPayer_(isPayer), nettingPrecision_(nettingPrecision) {

    QL_REQUIRE(!underlyingCashflows_.empty(), "NettedCommodityCashFlow: no underlying cashflows provided");
    QL_REQUIRE(underlyingCashflows_.size() == isPayer_.size(),
               "NettedCommodityCashFlow: underlyingCashflows and isPayer vectors must have the same size");
    for (size_t i = 1; i < underlyingCashflows_.size(); ++i) {
        QL_REQUIRE(underlyingCashflows_[i] != nullptr,
                   "NettedCommodityCashFlow: null underlying cashflow at index " << i);
        QL_REQUIRE(underlyingCashflows_[i]->date() == underlyingCashflows_[0]->date(),
                   "NettedCommodityCashFlow: all underlying cashflows must have the same payment date. "
                       << "Expected " << underlyingCashflows_[0]->date() << ", found "
                       << underlyingCashflows_[i]->date());
        QL_REQUIRE(underlyingCashflows_[i]->periodQuantity() == underlyingCashflows_[0]->periodQuantity(),
                   "NettedCommodityCashFlow: all underlying cashflows must have the same periodQuantity(). "
                       << "Expected " << underlyingCashflows_[0]->periodQuantity() << ", found "
                       << underlyingCashflows_[i]->periodQuantity());
    }

    for (const auto& cashflow : underlyingCashflows_) {
        registerWith(cashflow);
    }
}

Real NettedCommodityCashFlow::fixing() const {
    Real nettedFixing = 0.0;
    for (size_t i = 0; i < underlyingCashflows_.size(); ++i) {
        const auto& cf = underlyingCashflows_[i];
        double multiplier = isPayer_[i] ? -1.0 : 1.0;
        Real fixing = multiplier * cf->gearing() * cf->fixing() + cf->spread();
        nettedFixing += fixing;
    }
    return roundWithPrecision(nettedFixing, nettingPrecision_);
}

Real NettedCommodityCashFlow::amount() const { // Calculate and return the amount directly
    return fixing() * periodQuantity();
}

const std::vector<std::pair<Date, ext::shared_ptr<CommodityIndex>>>& NettedCommodityCashFlow::indices() const {
    static std::vector<std::pair<Date, ext::shared_ptr<CommodityIndex>>> allIndices;
    allIndices.clear();

    for (const auto& cf : underlyingCashflows_) {
        const auto& cfIndices = cf->indices();
        for (const auto& indexPair : cfIndices) {
            allIndices.push_back(indexPair);
        }
    }

    return allIndices;
}

Date NettedCommodityCashFlow::lastPricingDate() const {
    Date maxDate;
    // Find the latest pricing date across all underlying cashflows
    for (const auto& cf : underlyingCashflows_) {
        Date cfLastDate = cf->lastPricingDate();
        if (maxDate == Date() || cfLastDate > maxDate) {
            maxDate = cfLastDate;
        }
    }
    return maxDate;
}

Real NettedCommodityCashFlow::periodQuantity() const { return underlyingCashflows_.front()->periodQuantity(); }

void NettedCommodityCashFlow::accept(AcyclicVisitor& v) {
    if (Visitor<NettedCommodityCashFlow>* v1 = dynamic_cast<Visitor<NettedCommodityCashFlow>*>(&v))
        v1->visit(*this);
    else
        CommodityCashFlow::accept(v);
}

} // namespace QuantExt
