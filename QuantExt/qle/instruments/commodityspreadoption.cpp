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

#include <ql/exercise.hpp>
#include <ql/shared_ptr.hpp>
#include <qle/cashflows/commodityindexedaveragecashflow.hpp>
#include <qle/cashflows/commodityindexedcashflow.hpp>
#include <qle/instruments/commodityspreadoption.hpp>

namespace QuantExt {

CommoditySpreadOption::CommoditySpreadOption(const QuantLib::ext::shared_ptr<CommodityCashFlow>& longAssetFlow,
                                             const QuantLib::ext::shared_ptr<CommodityCashFlow>& shortAssetFlow,
                                             const ext::shared_ptr<Exercise>& exercise, const Real quantity,
                                             const Real strikePrice, QuantLib::Option::Type type,
                                             const QuantLib::Date& paymentDate,
                                             const QuantLib::ext::shared_ptr<FxIndex>& longAssetFxIndex,
                                             const QuantLib::ext::shared_ptr<FxIndex>& shortAssetFxIndex,
                                             Settlement::Type delivery, Settlement::Method settlementMethod)
    : Option(ext::shared_ptr<Payoff>(), exercise), longAssetFlow_(longAssetFlow), shortAssetFlow_(shortAssetFlow),
      quantity_(quantity), strikePrice_(strikePrice), type_(type), paymentDate_(paymentDate),
      longAssetFxIndex_(longAssetFxIndex), shortAssetFxIndex_(shortAssetFxIndex), settlementType_(delivery),
      settlementMethod_(settlementMethod) {
    registerWith(longAssetFlow_);
    registerWith(shortAssetFlow_);
    QL_REQUIRE(ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(longAssetFlow_) ||
                   QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(longAssetFlow_),
               "Expect commodity floating cashflows");
    QL_REQUIRE(ext::dynamic_pointer_cast<CommodityIndexedCashFlow>(shortAssetFlow_) ||
                   QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(shortAssetFlow_),
               "Expect commodity floating cashflows");
    if (auto avgFlow = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(longAssetFlow_)) {
        QL_REQUIRE(exercise_->lastDate() >= avgFlow->indices().rbegin()->first,
                   "exercise Date hast to be after last observation date");
    }
    if (auto avgFlow = QuantLib::ext::dynamic_pointer_cast<CommodityIndexedAverageCashFlow>(shortAssetFlow_)) {
        QL_REQUIRE(exercise_->lastDate() >= avgFlow->indices().rbegin()->first,
                   "exercise Date hast to be after last observation date");
    }

    if (longAssetFxIndex_)
        registerWith(longAssetFxIndex_);
    if (shortAssetFxIndex_)
        registerWith(shortAssetFxIndex_);
    if (paymentDate_ == Date()) {
        paymentDate_ = std::max(longAssetFlow_->date(), shortAssetFlow_->date());
    }
}

bool CommoditySpreadOption::isExpired() const { return paymentDate_ < Settings::instance().evaluationDate(); }

Real CommoditySpreadOption::effectiveStrike() const {
    return (strikePrice_ - longAssetFlow_->spread() + shortAssetFlow_->spread());
}

void CommoditySpreadOption::setupArguments(PricingEngine::arguments* args) const {
    Option::setupArguments(args);

    CommoditySpreadOption::arguments* arguments = dynamic_cast<CommoditySpreadOption::arguments*>(args);

    QL_REQUIRE(arguments != 0, "wrong argument type");
    QL_REQUIRE(longAssetFlow_->gearing() > 0.0, "The gearing on an APO must be positive");

    arguments->quantity = quantity_;
    arguments->strikePrice = strikePrice_;
    arguments->effectiveStrike = effectiveStrike();
    arguments->type = type_;
    arguments->settlementType = settlementType_;
    arguments->settlementMethod = settlementMethod_;
    arguments->exercise = exercise_;
    arguments->longAssetFlow = underlyingLongAssetFlow();
    arguments->shortAssetFlow = underlyingShortAssetFlow();
    arguments->longAssetFxIndex = longAssetFxIndex();
    arguments->shortAssetFxIndex = shortAssetFxIndex();
    arguments->paymentDate = paymentDate_;
    arguments->longAssetLastPricingDate = longAssetFlow_->lastPricingDate();
    arguments->shortAssetLastPricingDate = shortAssetFlow_->lastPricingDate();
}

CommoditySpreadOption::arguments::arguments()
    : quantity(0.0), strikePrice(0.0), effectiveStrike(0.0), type(Option::Call), paymentDate(Date()),
      longAssetFxIndex(nullptr), shortAssetFxIndex(nullptr), settlementType(Settlement::Physical),
      settlementMethod(Settlement::PhysicalOTC) {}

void CommoditySpreadOption::arguments::validate() const {
    QL_REQUIRE(longAssetFlow, "underlying not set");
    QL_REQUIRE(shortAssetFlow, "underlying not set");
    QL_REQUIRE(exercise, "exercise not set");
    QuantLib::Settlement::checkTypeAndMethodConsistency(settlementType, settlementMethod);
}



} // namespace QuantExt
