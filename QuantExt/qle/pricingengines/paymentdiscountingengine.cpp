/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ql/cashflows/cashflows.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

namespace QuantExt {

PaymentDiscountingEngine::PaymentDiscountingEngine(const Handle<YieldTermStructure>& discountCurve,
                                                   const Handle<Quote>& spotFX,
                                                   boost::optional<bool> includeSettlementDateFlows,
                                                   const Date& settlementDate, const Date& npvDate)
    : discountCurve_(discountCurve), spotFX_(spotFX), includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate), npvDate_(npvDate) {
    QL_REQUIRE(!discountCurve_.empty(), "empty discount curve");
    registerWith(discountCurve_);
    if (!spotFX.empty())
        registerWith(spotFX);
}

void PaymentDiscountingEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");

    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();

    Date refDate = discountCurve_->referenceDate();

    Date settlementDate = settlementDate_;
    if (settlementDate_ == Date()) {
        settlementDate = refDate;
    } else {
        QL_REQUIRE(settlementDate >= refDate, "settlement date (" << settlementDate
                                                                  << ") before discount curve reference date ("
                                                                  << refDate << ")");
    }

    Date valuationDate = npvDate_;
    if (npvDate_ == Date()) {
        valuationDate = refDate;
    } else {
        QL_REQUIRE(npvDate_ >= refDate,
                   "npv date (" << npvDate_ << ") before discount curve reference date (" << refDate << ")");
    }

    bool includeRefDateFlows =
        includeSettlementDateFlows_ ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();

    Real NPV = 0.0;
    if (!arguments_.cashflow->hasOccurred(settlementDate, includeRefDateFlows))
        NPV = arguments_.cashflow->amount() * discountCurve_->discount(arguments_.cashflow->date());

    if (!spotFX_.empty())
        NPV *= spotFX_->value();

    results_.value = NPV / discountCurve_->discount(valuationDate);

} // calculate
} // namespace QuantExt
