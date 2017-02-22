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

#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <ql/cashflows/cashflows.hpp>

using namespace QuantLib;

namespace QuantExt {

DiscountingRiskyBondEngine::DiscountingRiskyBondEngine(const Handle<YieldTermStructure>& discountCurve,
                                                       const Handle<DefaultProbabilityTermStructure>& defaultCurve,
                                                       const Handle<Quote> securitySpread,
                                                       boost::optional<bool> includeSettlementDateFlows)
    : discountCurve_(discountCurve), includeSettlementDateFlows_(icludeSettlementDateFlows) {
    registerWith(discountCurve_);
}

void DiscountingBondEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");

    results_.valuationDate = (*discountCurve_)->referenceDate();

    bool includeRefDateFlows =
        includeSettlementDateFlows_ ? *includeSettlementDateFlows_ : Settings::instance().includeReferenceDateEvents();

    results_.value = CashFlows::npv(arguments_.cashflows, **discountCurve_, includeRefDateFlows, results_.valuationDate,
                                    results_.valuationDate);

    // a bond's cashflow on settlement date is never taken into
    // account, so we might have to play it safe and recalculate
    if (!includeRefDateFlows && results_.valuationDate == arguments_.settlementDate) {
        // same parameters as above, we can avoid another call
        results_.settlementValue = results_.value;
    } else {
        // no such luck
        results_.settlementValue = CashFlows::npv(arguments_.cashflows, **discountCurve_, false,
                                                  arguments_.settlementDate, arguments_.settlementDate);
    }
}
}
