/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/cashflows/nettedcommoditycashflow.hpp>
#include <ql/math/rounding.hpp>
#include <ql/errors.hpp>

using namespace QuantLib;

namespace QuantExt {

NettedCommodityCashFlow::NettedCommodityCashFlow(
    const std::vector<std::pair<ext::shared_ptr<CashFlow>, bool>>& underlyingCashflows,
    const Date& paymentDate,
    Natural nettingPrecision)
    : underlyingCashflows_(underlyingCashflows), paymentDate_(paymentDate),
      nettingPrecision_(nettingPrecision), commonQuantity_(0.0), cachedAmount_(0.0), calculated_(false) {

    QL_REQUIRE(!underlyingCashflows_.empty(), "NettedCommodityCashFlow: no underlying cashflows provided");

    validateAndSetCommonQuantity();

    // Register as observer of all underlying cashflows
    for (const auto& cfPair : underlyingCashflows_) {
        Observer::registerWith(cfPair.first);
    }
}

void NettedCommodityCashFlow::validateAndSetCommonQuantity() {
    Real quantity = Null<Real>();

    for (const auto& cfPair : underlyingCashflows_) {
        const auto& cf = cfPair.first;
        Real cfQuantity = Null<Real>();

        // Try to cast to different commodity cashflow types to get the quantity
        if (auto indexedCf = ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf)) {
            cfQuantity = indexedCf->periodQuantity();
        } else if (auto avgCf = ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf)) {
            cfQuantity = avgCf->periodQuantity();
        } else {
            QL_FAIL("NettedCommodityCashFlow: underlying cashflow is not a supported commodity cashflow type");
        }

        if (quantity == Null<Real>()) {
            quantity = cfQuantity;
        } else {
            QL_REQUIRE(std::abs(quantity - cfQuantity) < 1e-12,
                      "NettedCommodityCashFlow: all underlying cashflows must have the same periodQuantity(). "
                      << "Expected " << quantity << ", found " << cfQuantity);
        }
    }

    QL_REQUIRE(quantity != Null<Real>(), "NettedCommodityCashFlow: could not determine common quantity");
    commonQuantity_ = quantity;
}

Real NettedCommodityCashFlow::calculateTotalAverageFixing() const {
    Real totalFixing = 0.0;

    for (const auto& cfPair : underlyingCashflows_) {
        const auto& cf = cfPair.first;
        const bool isPayer = cfPair.second;
        Real fixing = 0.0;

        // Try to cast to different commodity cashflow types to get the fixing
        if (auto indexedCf = ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(cf)) {
            fixing = indexedCf->fixing();
        } else if (auto avgCf = ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(cf)) {
            fixing = avgCf->fixing();
        } else {
            QL_FAIL("NettedCommodityCashFlow: underlying cashflow is not a supported commodity cashflow type");
        }

        // Apply payer sign: payer legs subtract, receiver legs add
        totalFixing += (isPayer ? -1.0 : 1.0) * fixing;
    }

    return totalFixing;
}

Real NettedCommodityCashFlow::amount() const {
    if (!calculated_) {
        // Calculate the total average fixing
        Real totalAverageFixing = calculateTotalAverageFixing();

        // Round to specified precision if precision is specified
        Real roundedTotalFixing = totalAverageFixing;
        if (nettingPrecision_ != Null<Natural>()) {
            static const QuantLib::Natural preRoundPrecision = 8;
            QuantLib::ClosestRounding preRound(preRoundPrecision);
            totalAverageFixing = preRound(totalAverageFixing);
            ClosestRounding rounder(nettingPrecision_);
            roundedTotalFixing = rounder(totalAverageFixing);
        }

        // Calculate final amount: rounded total fixing * common quantity
        cachedAmount_ = roundedTotalFixing * commonQuantity_;
        calculated_ = true;
    }

    return cachedAmount_;
}

void NettedCommodityCashFlow::accept(AcyclicVisitor& v) {
    if (Visitor<NettedCommodityCashFlow>* v1 = dynamic_cast<Visitor<NettedCommodityCashFlow>*>(&v))
        v1->visit(*this);
    else
        CashFlow::accept(v);
}

} // namespace QuantExt
