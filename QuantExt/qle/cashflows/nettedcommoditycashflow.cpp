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
    const std::vector<std::pair<ext::shared_ptr<CommodityCashFlow>, bool>>& underlyingCashflows,
    Natural nettingPrecision)
    : CommodityCashFlow(0.0, 0.0, 1.0, false, nullptr, nullptr),
      underlyingCashflows_(underlyingCashflows), nettingPrecision_(nettingPrecision),
      commonQuantity_(0.0), lastPricingDate_(Date()), indicesCalculated_(false) {

    QL_REQUIRE(!underlyingCashflows_.empty(), "NettedCommodityCashFlow: no underlying cashflows provided");

    validateAndSetCommonQuantityAndDate();

    // Register as observer of all underlying cashflows
    for (const auto& cfPair : underlyingCashflows_) {
        Observer::registerWith(cfPair.first);
    }
}

void NettedCommodityCashFlow::validateAndSetCommonQuantityAndDate() {
    Real quantity = Null<Real>();
    Date paymentDate = Date();

    for (size_t i = 0; i < underlyingCashflows_.size(); ++i) {
        const auto& cf = underlyingCashflows_[i].first;

        // Get quantity from the CommodityCashFlow base class
        Real cfQuantity = cf->periodQuantity();
        Date cfPaymentDate = cf->date();

        if (i == 0) {
            quantity = cfQuantity;
            paymentDate = cfPaymentDate;
        } else {
            QL_REQUIRE(QuantLib::close_enough(quantity, cfQuantity),
                      "NettedCommodityCashFlow: all underlying cashflows must have the same periodQuantity(). "
                      << "Expected " << quantity << ", found " << cfQuantity);
            QL_REQUIRE(paymentDate == cfPaymentDate,
                      "NettedCommodityCashFlow: all underlying cashflows must have the same payment date. "
                      << "Expected " << paymentDate << ", found " << cfPaymentDate);
        }
    }

    QL_REQUIRE(quantity != Null<Real>(), "NettedCommodityCashFlow: could not determine common quantity");
    QL_REQUIRE(paymentDate != Date(), "NettedCommodityCashFlow: could not determine common payment date");
    commonQuantity_ = quantity;
}

Real NettedCommodityCashFlow::calculateTotalAverageFixing() const {
    Real totalFixing = 0.0;

    for (const auto& cfPair : underlyingCashflows_) {
        const auto& cf = cfPair.first;
        const bool isPayer = cfPair.second;

        // Use CommodityCashFlow methods directly - implement the formula: sign_pay * cf.gearing() * cf.fixing() + cf.spread()
        Real sign_pay = isPayer ? -1.0 : 1.0;
        Real fixing = sign_pay * cf->gearing() * cf->fixing() + cf->spread();
        totalFixing += fixing;
    }

    return totalFixing;
}

Real NettedCommodityCashFlow::amount() const {
    // Calculate and return the amount directly - no need for caching since
    // underlying commodity cashflows are already lazy
    Real roundedTotalFixing = roundedFixing();
    return roundedTotalFixing * commonQuantity_;
}

Date NettedCommodityCashFlow::date() const {
    QL_REQUIRE(!underlyingCashflows_.empty(), "NettedCommodityCashFlow: no underlying cashflows");
    return underlyingCashflows_[0].first->date();
}

const std::vector<std::pair<Date, ext::shared_ptr<CommodityIndex>>>& NettedCommodityCashFlow::indices() const {
    if (!indicesCalculated_) {
        cachedIndices_.clear();

        // Collect all indices from underlying cashflows
        for (const auto& cfPair : underlyingCashflows_) {
            const auto& cf = cfPair.first;
            const auto& cfIndices = cf->indices();

            // Add all indices from this cashflow
            for (const auto& indexPair : cfIndices) {
                cachedIndices_.push_back(indexPair);
            }
        }

        indicesCalculated_ = true;
    }

    return cachedIndices_;
}

Date NettedCommodityCashFlow::lastPricingDate() const {
    if (lastPricingDate_ == Date()) {
        Date maxDate = Date();

        // Find the latest pricing date across all underlying cashflows
        for (const auto& cfPair : underlyingCashflows_) {
            const auto& cf = cfPair.first;
            Date cfLastDate = cf->lastPricingDate();

            if (maxDate == Date() || cfLastDate > maxDate) {
                maxDate = cfLastDate;
            }
        }

        lastPricingDate_ = maxDate;
    }

    return lastPricingDate_;
}

Real NettedCommodityCashFlow::periodQuantity() const {
    return commonQuantity_;
}

Real NettedCommodityCashFlow::roundedFixing() const {
    // Calculate the total average fixing
    Real totalAverageFixing = calculateTotalAverageFixing();

    // Round to specified precision if precision is specified
    if (nettingPrecision_ != Null<Natural>()) {
        static const QuantLib::Natural preRoundPrecision = 8;
        QuantLib::ClosestRounding preRound(preRoundPrecision);
        totalAverageFixing = preRound(totalAverageFixing);
        ClosestRounding rounder(nettingPrecision_);
        totalAverageFixing = rounder(totalAverageFixing);
    }

    return totalAverageFixing;
}

Real NettedCommodityCashFlow::fixing() const {
    return roundedFixing();
}

void NettedCommodityCashFlow::accept(AcyclicVisitor& v) {
    if (Visitor<NettedCommodityCashFlow>* v1 = dynamic_cast<Visitor<NettedCommodityCashFlow>*>(&v))
        v1->visit(*this);
    else
        CommodityCashFlow::accept(v);
}

} // namespace QuantExt
