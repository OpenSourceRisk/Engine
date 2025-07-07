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

#include <boost/make_shared.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/utilities/fxindex.hpp>

using namespace QuantLib;

namespace QuantExt{

Payment::Payment(const Real amount, const Currency& currency, const Date& date)
    : Payment(amount, currency, date, currency, nullptr, std::nullopt) {}

Payment::Payment(const Real amount, const Currency& currency, const Date& date, const Currency& payCurrency,
                 const QuantLib::ext::shared_ptr<FxIndex>& fxIndex, const std::optional<QuantLib::Date>& fixingDate)
    : currency_(currency), payCurrency_(payCurrency), fxIndex_(fxIndex), fixingDate_(fixingDate) {
    cashflow_ = QuantLib::ext::make_shared<SimpleCashFlow>(amount, date);
    QL_REQUIRE(payCurrency_ == currency_ || validFxIndex(fxIndex_, currency_, payCurrency_),
               "Payment: pay currency must be the same as premium currency or an FX index must be provided, got pay "
                   << payCurrency_.code() << " and premium currency " << currency_.code());
    if (payCurrency_ != currency_) {
        registerWith(fxIndex_);
    }
}

bool Payment::isExpired() const { return cashflow_->hasOccurred(); }

void Payment::setupExpired() const { Instrument::setupExpired(); }

void Payment::setupArguments(PricingEngine::arguments* args) const {
    Payment::arguments* arguments = dynamic_cast<Payment::arguments*>(args);
    QL_REQUIRE(arguments, "wrong argument type in deposit");
    arguments->cashflow = cashflow_;
    arguments->fxIndex = payCurrency_ != currency_ ? fxIndex_ : nullptr;
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
