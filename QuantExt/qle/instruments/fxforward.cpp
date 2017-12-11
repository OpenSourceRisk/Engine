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

#include <qle/instruments/fxforward.hpp>

using namespace QuantLib;

namespace QuantExt {

FxForward::FxForward(const Real& nominal1, const Currency& currency1, const Real& nominal2, const Currency& currency2,
                     const Date& maturityDate, const bool& payCurrency1)
    : nominal1_(nominal1), currency1_(currency1), nominal2_(nominal2), currency2_(currency2),
      maturityDate_(maturityDate), payCurrency1_(payCurrency1) {}

FxForward::FxForward(const Money& nominal1, const ExchangeRate& forwardRate, const Date& maturityDate,
                     bool sellingNominal)
    : nominal1_(nominal1.value()), currency1_(nominal1.currency()), maturityDate_(maturityDate),
      payCurrency1_(sellingNominal) {

    QL_REQUIRE(currency1_ == forwardRate.target(), "Currency of nominal1 does not match target (domestic) "
                                                   "currency in the exchange rate.");

    Money otherNominal = forwardRate.exchange(nominal1);
    nominal2_ = otherNominal.value();
    currency2_ = otherNominal.currency();
}

FxForward::FxForward(const Money& nominal1, const Handle<Quote>& fxForwardQuote, const Currency& currency2,
                     const Date& maturityDate, bool sellingNominal)
    : nominal1_(nominal1.value()), currency1_(nominal1.currency()), currency2_(currency2), maturityDate_(maturityDate),
      payCurrency1_(sellingNominal) {

    QL_REQUIRE(fxForwardQuote->isValid(), "The FX Forward quote is not valid.");

    nominal2_ = nominal1_ / fxForwardQuote->value();
}

bool FxForward::isExpired() const { return detail::simple_event(maturityDate_).hasOccurred(); }

void FxForward::setupExpired() const {
    Instrument::setupExpired();
    npv_ = Money(0.0, Currency());
    fairForwardRate_ = ExchangeRate();
}

void FxForward::setupArguments(PricingEngine::arguments* args) const {

    FxForward::arguments* arguments = dynamic_cast<FxForward::arguments*>(args);

    QL_REQUIRE(arguments, "wrong argument type in fxforward");

    arguments->nominal1 = nominal1_;
    arguments->currency1 = currency1_;
    arguments->nominal2 = nominal2_;
    arguments->currency2 = currency2_;
    arguments->maturityDate = maturityDate_;
    arguments->payCurrency1 = payCurrency1_;
}

void FxForward::fetchResults(const PricingEngine::results* r) const {

    Instrument::fetchResults(r);

    const FxForward::results* results = dynamic_cast<const FxForward::results*>(r);

    QL_REQUIRE(results, "wrong result type");

    npv_ = results->npv;
    fairForwardRate_ = results->fairForwardRate;
}

void FxForward::arguments::validate() const {
    QL_REQUIRE(nominal1 > 0.0, "nominal1  should be positive: " << nominal1);
    QL_REQUIRE(nominal2 > 0.0, "nominal2 should be positive: " << nominal2);
}

void FxForward::results::reset() {

    Instrument::results::reset();

    npv = Money(0.0, Currency());
    fairForwardRate = ExchangeRate();
}
} // namespace QuantExt
