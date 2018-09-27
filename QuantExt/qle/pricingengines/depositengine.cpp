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

#include <ql/cashflows/cashflows.hpp>
#include <qle/pricingengines/depositengine.hpp>

namespace QuantExt {

DepositEngine::DepositEngine(const Handle<YieldTermStructure>& discountCurve,
                             boost::optional<bool> includeSettlementDateFlows, Date settlementDate, Date npvDate)
    : discountCurve_(discountCurve), includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate), npvDate_(npvDate) {
    registerWith(discountCurve_);
}

void DepositEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");

    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();

    Date refDate = discountCurve_->referenceDate();

    Date settlementDate = settlementDate_;
    if (settlementDate_ == Date()) {
        settlementDate = refDate;
    } else {
        QL_REQUIRE(settlementDate >= refDate, "settlement date (" << settlementDate
                                                                  << ") before "
                                                                     "discount curve reference date ("
                                                                  << refDate << ")");
    }

    Date valuationDate = npvDate_;
    if (npvDate_ == Date()) {
        valuationDate = refDate;
    } else {
        QL_REQUIRE(npvDate_ >= refDate, "npv date (" << npvDate_
                                                     << ") before "
                                                        "discount curve reference date ("
                                                     << refDate << ")");
    }

    bool includeRefDateFlows =
        includeSettlementDateFlows_ ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();

    results_.value =
        CashFlows::npv(arguments_.leg, **discountCurve_, includeRefDateFlows, settlementDate, valuationDate);

    // calculate the fair rate of a hypothetical deposit instrument traded on the refDate with maturity as the original
    // instrument; this is only possible if the maturity date is later than the start date of that new deposit
    Date startDate = arguments_.index->valueDate(arguments_.index->fixingCalendar().adjust(refDate));
    if (arguments_.maturityDate > startDate)
        results_.fairRate =
            (discountCurve_->discount(startDate) / discountCurve_->discount(arguments_.maturityDate) - 1.0) /
            arguments_.index->dayCounter().yearFraction(startDate, arguments_.maturityDate);

} // calculate
} // namespace QuantExt
