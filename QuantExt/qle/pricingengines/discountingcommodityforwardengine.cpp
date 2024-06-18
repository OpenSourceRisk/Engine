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
#include <qle/instruments/cashflowresults.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

DiscountingCommodityForwardEngine::DiscountingCommodityForwardEngine(const Handle<YieldTermStructure>& discountCurve,
                                                                     boost::optional<bool> includeSettlementDateFlows,
                                                                     const Date& npvDate)
    : discountCurve_(discountCurve), includeSettlementDateFlows_(includeSettlementDateFlows),
      npvDate_(npvDate) {

    registerWith(discountCurve_);
}

void DiscountingCommodityForwardEngine::calculate() const {

    const auto& index = arguments_.index;
    Date npvDate = npvDate_;
    if (npvDate == Null<Date>()) {
        const auto& priceCurve = index->priceCurve();
        QL_REQUIRE(!priceCurve.empty(), "DiscountingCommodityForwardEngine: need a non-empty price curve.");
        npvDate = priceCurve->referenceDate();
    }

    const Date& maturity = arguments_.maturityDate;
    Date paymentDate = maturity;
    if (!arguments_.physicallySettled && arguments_.paymentDate != Date()) {
        paymentDate = arguments_.paymentDate;
    }

    results_.value = 0.0;
    if (!detail::simple_event(paymentDate).hasOccurred(Date(), includeSettlementDateFlows_)) {

        Real buySell = arguments_.position == Position::Long ? 1.0 : -1.0;
        Real forwardPrice = index->fixing(maturity);
        Real paymentDateDiscountFactor = discountCurve_->discount(paymentDate);

        auto value = arguments_.quantity * buySell * (forwardPrice - arguments_.strike) * paymentDateDiscountFactor /
                     discountCurve_->discount(npvDate);
        Real fxRate = 1.0;
        if(arguments_.fxIndex && (arguments_.fixingDate!=Date()) && (arguments_.payCcy!=arguments_.currency)){ // NDF
            fxRate = arguments_.fxIndex->fixing(arguments_.fixingDate);
            value*=fxRate;
            results_.additionalResults["productCurrency"] = arguments_.currency;
            results_.additionalResults["settlementCurrency"] = arguments_.payCcy;
            results_.additionalResults["fxRate"] = fxRate;
        }
        results_.value = value;
        results_.additionalResults["forwardPrice"] = forwardPrice;
        results_.additionalResults["currentNotional"] = forwardPrice * arguments_.quantity;
        results_.additionalResults["paymentDateDiscountFactor"] = paymentDateDiscountFactor;

        // populate cashflow results
        std::vector<CashFlowResults> cashFlowResults;
        CashFlowResults cf1, cf2;
        cf1.payDate = cf2.payDate = arguments_.paymentDate;
        cf1.type = cf2.type = "Notional";
        cf1.discountFactor = cf2.discountFactor = paymentDateDiscountFactor / discountCurve_->discount(npvDate);
        cf1.legNumber = 0;
        cf2.legNumber = 1;
        if (!arguments_.physicallySettled) {
            cf1.fixingDate = maturity;
            cf1.fixingValue = forwardPrice;
            cf1.amount = arguments_.quantity * buySell * forwardPrice * fxRate;
            cf2.amount = arguments_.quantity * buySell * -arguments_.strike * fxRate;
            cf1.currency = cf2.currency =
                arguments_.payCcy.empty() ? arguments_.currency.code() : arguments_.payCcy.code();
        } else {
            cf1.amount = arguments_.quantity * buySell * forwardPrice;
            cf2.amount = arguments_.quantity * buySell * -arguments_.strike;
            cf1.currency = cf2.currency = arguments_.currency.code();
        }
        cashFlowResults.push_back(cf1);
        cashFlowResults.push_back(cf2);
        results_.additionalResults["cashFlowResults"] = cashFlowResults;
    }
}

} // namespace QuantExt
