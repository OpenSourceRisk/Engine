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

#include <qle/instruments/cashflowresults.hpp>
#include <qle/pricingengines/discountingbondfutureengine.hpp>
#include <qle/pricingengines/forwardenabledbondengine.hpp>

#include <ql/event.hpp>

#include <boost/date_time.hpp>

namespace QuantExt {

DiscountingBondFutureEngine::DiscountingBondFutureEngine(const Handle<YieldTermStructure>& discountCurve,
                                                         const Handle<Quote>& conversionFactor)
    : discountCurve_(discountCurve), conversionFactor_(conversionFactor) {}

void DiscountingBondFutureEngine::calculate() const {

    Date today = Settings::instance().evaluationDate();
    Real strike = arguments_.index->fixing(std::min(arguments_.index->futureExpiryDate(), today));
    Real forwardBondPrice = arguments_.index->fixing(arguments_.index->futureExpiryDate());

    results_.value = discountCurve_->discount(arguments_.futureSettlement) *
                     (forwardBondPrice * conversionFactor_->value() - strike) * (arguments_.isLong ? 1.0 : -1.0) *
                     arguments_.contractNotional;

    std::vector<CashFlowResults> cashFlowResults;

    CashFlowResults strikeFlow;
    strikeFlow.payDate = arguments_.futureSettlement;
    strikeFlow.amount = strike * (arguments_.isLong ? -1.0 : 1.0) * arguments_.contractNotional;
    strikeFlow.type = "StrikeFlow";
    cashFlowResults.push_back(strikeFlow);

    CashFlowResults bondFlow;
    bondFlow.payDate = arguments_.futureSettlement;
    bondFlow.amount =
        forwardBondPrice * conversionFactor_->value() * (arguments_.isLong ? 1.0 : -1.0) * arguments_.contractNotional;
    bondFlow.type = "BondValueFlow";
    bondFlow.fixingDate = arguments_.index->futureExpiryDate();
    bondFlow.fixingValue = forwardBondPrice;
    cashFlowResults.push_back(bondFlow);
}

} // namespace QuantExt
