/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/discountingequityforwardengine.hpp>

namespace QuantExt {

DiscountingEquityForwardEngine::DiscountingEquityForwardEngine(
    const Handle<YieldTermStructure>& equityInterestRateCurve, const Handle<YieldTermStructure>& dividendYieldCurve,
    const Handle<Quote>& equitySpot, const Handle<YieldTermStructure>& discountCurve,
    boost::optional<bool> includeSettlementDateFlows, const Date& settlementDate, const Date& npvDate)
    : equityRefRateCurve_(equityInterestRateCurve), divYieldCurve_(dividendYieldCurve), equitySpot_(equitySpot),
      discountCurve_(discountCurve), includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate), npvDate_(npvDate) {

    registerWith(equityRefRateCurve_);
    registerWith(divYieldCurve_);
    registerWith(equitySpot_);
    registerWith(discountCurve_);
}

void DiscountingEquityForwardEngine::calculate() const {

    Date npvDate = npvDate_;
    if (npvDate == Null<Date>()) {
        npvDate = divYieldCurve_->referenceDate();
    }
    Date settlementDate = settlementDate_;
    if (settlementDate == Null<Date>()) {
        settlementDate = npvDate;
    }

    results_.value = 0.0;

    if (!detail::simple_event(arguments_.payDate).hasOccurred(settlementDate, includeSettlementDateFlows_)) {
        Real lsInd = ((arguments_.longShort == Position::Long) ? 1.0 : -1.0);
        Real qty = arguments_.quantity;
        Date maturity = arguments_.maturityDate;
        Real strike = arguments_.strike;
        Real forwardPrice =
            equitySpot_->value() * divYieldCurve_->discount(maturity) / equityRefRateCurve_->discount(maturity);
        DiscountFactor df = discountCurve_->discount(arguments_.payDate);
        results_.value = (lsInd * qty) * (forwardPrice - strike) * df;
        Real fxRate = 1.0;
        if (arguments_.payCurrency != arguments_.currency) {
            QL_REQUIRE(arguments_.fxIndex != nullptr, "DiscountingEquityForwardEngine requires an FxIndex to convert "
                                                      "from underyling currency ("
                                                          << arguments_.currency << ") to payCurrency ("
                                                          << arguments_.payCurrency << ")");
            QL_REQUIRE(arguments_.fixingDate != Date(),
                       "DiscountingEquityForwardEngine: Payment and Underlying currency don't match, require an fx "
                       "fixing date for settlement conversion");
            auto fxRate = arguments_.fxIndex->fixing(arguments_.fixingDate);
            results_.value *= fxRate;
        }
        results_.additionalResults["forwardPrice"] = forwardPrice;
        results_.additionalResults["underlyingCcy"] = arguments_.currency;
        results_.additionalResults["currentNotional"] = qty * forwardPrice * fxRate;
        results_.additionalResults["currentNotionalCurrency"] = arguments_.payCurrency;
        results_.additionalResults["fxRate"] = fxRate;
        results_.additionalResults["fxFixingDate"] = arguments_.fixingDate;
        results_.additionalResults["payCcy"] = arguments_.payCurrency;
    }
} // calculate

} // namespace QuantExt
