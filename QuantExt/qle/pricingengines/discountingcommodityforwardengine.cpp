/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ql/event.hpp>

#include <qle/pricingengines/discountingcommodityforwardengine.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

DiscountingCommodityForwardEngine::DiscountingCommodityForwardEngine(const Handle<PriceTermStructure>& priceCurve,
    const Handle<YieldTermStructure>& discountCurve, boost::optional<bool> includeSettlementDateFlows,
    const Date& npvDate)
    : priceCurve_(priceCurve), discountCurve_(discountCurve), 
      includeSettlementDateFlows_(includeSettlementDateFlows), npvDate_(npvDate) {

    registerWith(priceCurve_);
    registerWith(discountCurve_);
}

void DiscountingCommodityForwardEngine::calculate() const {

    Date npvDate = npvDate_ == Null<Date>() ? priceCurve_->referenceDate() : npvDate_;

    results_.value = 0.0;
    if (!detail::simple_event(arguments_.maturityDate).hasOccurred(
        Date(), includeSettlementDateFlows_)) {

        Real buySellIndicator = arguments_.position == Position::Long ? 1.0 : -1.0;

        Real forwardPrice = priceCurve_->price(arguments_.maturityDate);
        DiscountFactor discount = discountCurve_->discount(arguments_.maturityDate);
        discount /= discountCurve_->discount(npvDate);

        results_.value = (forwardPrice - arguments_.strike) * discount;
        results_.value *= arguments_.quantity * buySellIndicator;
    }
}

}
