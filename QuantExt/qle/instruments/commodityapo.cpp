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
#include <qle/instruments/commodityapo.hpp>

namespace QuantExt {

CommodityAveragePriceOption::CommodityAveragePriceOption(const QuantLib::ext::shared_ptr<CommodityIndexedAverageCashFlow>& flow,
                                                         const ext::shared_ptr<Exercise>& exercise, const Real quantity,
                                                         const Real strikePrice, QuantLib::Option::Type type,
                                                         QuantLib::Settlement::Type delivery,
                                                         QuantLib::Settlement::Method settlementMethod,
                                                         const Real barrierLevel, Barrier::Type barrierType,
                                                         Exercise::Type barrierStyle,
                                                         const QuantLib::ext::shared_ptr<FxIndex>& fxIndex)
    : Option(ext::shared_ptr<Payoff>(), exercise), flow_(flow), quantity_(quantity), strikePrice_(strikePrice),
      type_(type), settlementType_(delivery), settlementMethod_(settlementMethod), fxIndex_(fxIndex),
      barrierLevel_(barrierLevel), barrierType_(barrierType), barrierStyle_(barrierStyle) {
    flow_->alwaysForwardNotifications();
    registerWith(flow_);
    if (fxIndex_)
        registerWith(fxIndex_);
}

bool CommodityAveragePriceOption::isExpired() const { return detail::simple_event(flow_->date()).hasOccurred(); }

Real CommodityAveragePriceOption::effectiveStrike() const {
    return (strikePrice_ - flow_->spread()) / flow_->gearing();
}

Real CommodityAveragePriceOption::accrued(const Date& refDate) const {
    Real tmp = 0.0;

    // If all of the pricing dates are greater than today => no accrued
    if (refDate >= flow_->indices().begin()->first) {
        for (const auto& kv : flow_->indices()) {
            // Break on the first pricing date that is greater than today
            if (refDate < kv.first) {
                break;
            }
            // Update accrued where pricing date is on or before today
            Real fxRate = (fxIndex_)?fxIndex_->fixing(kv.first):1.;
            tmp += fxRate*kv.second->fixing(kv.first);
        }
        // We should have pricing dates in the period but check.
        auto n = flow_->indices().size();
        QL_REQUIRE(n > 0, "APO coupon accrued calculation has a degenerate coupon.");
        tmp /= n;
    }

    return tmp;
}

void CommodityAveragePriceOption::setupArguments(PricingEngine::arguments* args) const {
    Option::setupArguments(args);

    CommodityAveragePriceOption::arguments* arguments = dynamic_cast<CommodityAveragePriceOption::arguments*>(args);

    QL_REQUIRE(arguments != 0, "wrong argument type");
    QL_REQUIRE(flow_->gearing() > 0.0, "The gearing on an APO must be positive");

    Date today = Settings::instance().evaluationDate();

    arguments->quantity = quantity_;
    arguments->strikePrice = strikePrice_;
    arguments->effectiveStrike = effectiveStrike();
    arguments->accrued = accrued(today);
    arguments->type = type_;
    arguments->settlementType = settlementType_;
    arguments->settlementMethod = settlementMethod_;
    arguments->barrierLevel = barrierLevel_;
    arguments->barrierType = barrierType_;
    arguments->barrierStyle = barrierStyle_;
    arguments->exercise = exercise_;
    arguments->flow = flow_;
    arguments->fxIndex = fxIndex_;
}

CommodityAveragePriceOption::arguments::arguments()
    : quantity(0.0), strikePrice(0.0), effectiveStrike(0.0), type(Option::Call), fxIndex(nullptr),
      settlementType(Settlement::Physical), settlementMethod(Settlement::PhysicalOTC), barrierLevel(Null<Real>()),
      barrierType(Barrier::DownIn), barrierStyle(Exercise::American) {}

void CommodityAveragePriceOption::arguments::validate() const {
    QL_REQUIRE(flow, "underlying not set");
    QL_REQUIRE(exercise, "exercise not set");
    QuantLib::Settlement::checkTypeAndMethodConsistency(settlementType, settlementMethod);
}

} // namespace QuantExt
