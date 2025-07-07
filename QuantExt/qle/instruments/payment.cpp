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

Payment::Payment(const Real amount, const Currency& currency, const Date& date)
    : Payment(amount, currency, date, std::nullopt, std::nullopt, std::nullopt) {}

Payment::Payment(const Real amount, const Currency& currency, const Date& date,
                 const std::optional<Currency>& payCurrency,
                 const std::optional<QuantLib::ext::shared_ptr<FxIndex>>& fxIndex,
                 const std::optional<QuantLib::Date>& fixingDate)
    : currency_(currency), payCurrency_(payCurrency), fxIndex_(fxIndex), fixingDate_(fixingDate) {
    cashflow_ = QuantLib::ext::make_shared<SimpleCashFlow>(amount, date);
    QL_REQUIRE(!fxIndex_.has_value() || payCurrency_.has_value(),
               "Payment: pay currency must be set if fx index is set");
    QL_REQUIRE(!fxIndex_.has_value() || (fxIndex_.value()->sourceCurrency().code() == currency.code() &&
                                         fxIndex_.value()->targetCurrency().code() == payCurrency_->code()),
               "Payment: fx index currency must match pay and premium currency, got index "
                   << fxIndex_.value()->name() << " with source " << fxIndex_.value()->sourceCurrency().code()
                   << " and target " << fxIndex_.value()->targetCurrency().code() << ", pay currency "
                   << payCurrency_->code() << " and premium currency " << currency.code());
}

bool Payment::isExpired() const { return cashflow_->hasOccurred(); }

void Payment::setupExpired() const { Instrument::setupExpired(); }

void Payment::setupArguments(PricingEngine::arguments* args) const {
    Payment::arguments* arguments = dynamic_cast<Payment::arguments*>(args);
    QL_REQUIRE(arguments, "wrong argument type in deposit");
    arguments->cashflow = cashflow_;
    arguments->fxIndex = fxIndex_;
    arguments->fixingDate = fixingDate_;
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
