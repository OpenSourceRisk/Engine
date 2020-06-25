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

#include <ql/event.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/instruments/forwardbond.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>

namespace QuantExt {

ForwardBond::ForwardBond(const boost::shared_ptr<Bond>& underlying, const boost::shared_ptr<Payoff>& payoff,
                         const Date& fwdMaturityDate, const bool settlementDirty, const Real compensationPayment,
                         const Date compensationPaymentDate, const Real bondNotional)
    : underlying_(underlying), payoff_(payoff), fwdMaturityDate_(fwdMaturityDate), settlementDirty_(settlementDirty),
      compensationPayment_(compensationPayment), compensationPaymentDate_(compensationPaymentDate),
      bondNotional_(bondNotional) {}

bool ForwardBond::isExpired() const { return detail::simple_event(fwdMaturityDate_).hasOccurred(); }

void ForwardBond::setupArguments(PricingEngine::arguments* args) const {
    ForwardBond::arguments* arguments = dynamic_cast<ForwardBond::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type in forward bond");
    arguments->underlying = underlying_;
    arguments->payoff = payoff_;
    arguments->fwdMaturityDate = fwdMaturityDate_;
    arguments->settlementDirty = settlementDirty_;
    arguments->compensationPayment = compensationPayment_;
    arguments->compensationPaymentDate = compensationPaymentDate_;
    arguments->bondNotional = bondNotional_;
}

void ForwardBond::results::reset() {
    Instrument::results::reset();
    forwardValue = Null<Real>();
    underlyingSpotValue = Null<Real>();
    underlyingIncome = Null<Real>();
}

void ForwardBond::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
    const ForwardBond::results* results = dynamic_cast<const ForwardBond::results*>(r);
    QL_REQUIRE(results, "wrong result type");
    forwardValue_ = results->forwardValue;
    underlyingSpotValue_ = results->underlyingSpotValue;
    underlyingIncome_ = results->underlyingIncome;
}

void ForwardBond::arguments::validate() const { QL_REQUIRE(underlying, "bond pointer is null"); }

} // namespace QuantExt
