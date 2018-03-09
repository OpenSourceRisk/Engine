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

#include <qle/instruments/payment.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

Payment::Payment(const Real amount, const Currency& currency, const Date& date) : currency_(currency) {
    cashflow_ = boost::shared_ptr<SimpleCashFlow>(new SimpleCashFlow(amount, date));
}

bool Payment::isExpired() const { return cashflow_->hasOccurred(); }

void Payment::setupExpired() const { Instrument::setupExpired(); }

void Payment::setupArguments(PricingEngine::arguments* args) const {
    Payment::arguments* arguments = dynamic_cast<Payment::arguments*>(args);
    QL_REQUIRE(arguments, "wrong argument type in deposit");
    arguments->currency = currency_;
    arguments->cashflow = cashflow_;
}

void Payment::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
    const Payment::results* results = dynamic_cast<const Payment::results*>(r);
    QL_REQUIRE(results, "wrong result type");
}

void Payment::arguments::validate() const {
    // always valid
}

void Payment::results::reset() { Instrument::results::reset(); }
} // namespace QuantExt
