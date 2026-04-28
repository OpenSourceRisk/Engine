/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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
#include <ql/exercise.hpp>
#include <ql/settings.hpp>
#include <qle/instruments/cashsettledamericanoption.hpp>

using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::Index;
using QuantLib::Natural;
using QuantLib::Null;
using QuantLib::Option;
using QuantLib::PlainVanillaPayoff;
using QuantLib::CashOrNothingPayoff;
using QuantLib::PricingEngine;
using QuantLib::Real;
using QuantLib::Settings;
using QuantLib::StrikedTypePayoff;
using QuantLib::VanillaOption;
using QuantLib::io::iso_date;

namespace {

// Shared check of arguments
void check(const Date& expiryDate, const Date& paymentDate, bool automaticExercise,
           const QuantLib::ext::shared_ptr<Index>& underlying, bool exercised, Real priceAtExercise) {

    using QuantLib::io::iso_date;

    QL_REQUIRE(paymentDate >= expiryDate, "Cash settled American option payment date ("
                                              << iso_date(paymentDate)
                                              << ") must be greater than or equal to the expiry date ("
                                              << iso_date(expiryDate) << ")");

    if (automaticExercise) {
        QL_REQUIRE(underlying, "Cash settled American option has automatic exercise so we need a valid underlying.");
    }

    if (exercised) {
        QL_REQUIRE(priceAtExercise != Null<Real>(), "Cash settled American option was exercised so we need "
                                                        << "a valid exercise price.");
    }
}

} // namespace

namespace QuantExt {

CashSettledAmericanOption::CashSettledAmericanOption(Option::Type type, Real strike, const Date& expiryDate,
                                                     const Date& paymentDate, bool automaticExercise,
                                                     const QuantLib::ext::shared_ptr<Index>& underlying, bool exercised,
                                                     Real priceAtExercise,
                                                     const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex,
                                                     const std::optional<QuantLib::Date>& cashSettlementFxFixingDate)
    : VanillaOption(QuantLib::ext::make_shared<PlainVanillaPayoff>(type, strike),
                    QuantLib::ext::make_shared<AmericanExercise>(expiryDate)),
      paymentDate_(paymentDate), automaticExercise_(automaticExercise), underlying_(underlying), exercised_(false),
      priceAtExercise_(Null<Real>()), fxIndex_(fxIndex), cashSettlementFxFixingDate_(cashSettlementFxFixingDate) {

    init(exercised, priceAtExercise);

    check(exercise_->lastDate(), paymentDate_, automaticExercise_, underlying_, exercised_, priceAtExercise_);
}

CashSettledAmericanOption::CashSettledAmericanOption(Option::Type type, Real strike, const Date& expiryDate,
                                                     Natural paymentLag, const Calendar& paymentCalendar,
                                                     BusinessDayConvention paymentConvention, bool automaticExercise,
                                                     const QuantLib::ext::shared_ptr<Index>& underlying, bool exercised,
                                                     Real priceAtExercise,
                                                     const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex,
                                                     const std::optional<QuantLib::Date>& cashSettlementFxFixingDate)
    : VanillaOption(QuantLib::ext::make_shared<PlainVanillaPayoff>(type, strike),
                    QuantLib::ext::make_shared<AmericanExercise>(expiryDate)),
      automaticExercise_(automaticExercise), underlying_(underlying), exercised_(false),
      priceAtExercise_(Null<Real>()), fxIndex_(fxIndex), cashSettlementFxFixingDate_(cashSettlementFxFixingDate) { 

    init(exercised, priceAtExercise);

    // Derive payment date from exercise date using the lag, calendar and convention.
    paymentDate_ = paymentCalendar.advance(expiryDate, paymentLag * QuantLib::Days, paymentConvention);

    check(exercise_->lastDate(), paymentDate_, automaticExercise_, underlying_, exercised_, priceAtExercise_);
}

CashSettledAmericanOption::CashSettledAmericanOption(Option::Type type, Real strike, Real cashPayoff,
                                                     const Date& expiryDate, const Date& paymentDate,
                                                     bool automaticExercise, const QuantLib::ext::shared_ptr<Index>& underlying,
                                                     bool exercised, Real priceAtExercise,
                                                     const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex,
                                                     const std::optional<QuantLib::Date>& cashSettlementFxFixingDate)
    : VanillaOption(QuantLib::ext::make_shared<CashOrNothingPayoff>(type, strike, cashPayoff),
                    QuantLib::ext::make_shared<AmericanExercise>(expiryDate)),
      paymentDate_(paymentDate), automaticExercise_(automaticExercise), underlying_(underlying), exercised_(false),
      priceAtExercise_(Null<Real>()), fxIndex_(fxIndex), cashSettlementFxFixingDate_(cashSettlementFxFixingDate) {

    init(exercised, priceAtExercise);

    check(exercise_->lastDate(), paymentDate_, automaticExercise_, underlying_, exercised_, priceAtExercise_);
}

CashSettledAmericanOption::CashSettledAmericanOption(Option::Type type, Real strike, Real cashPayoff,
                                                     const Date& expiryDate, Natural paymentLag,
                                                     const Calendar& paymentCalendar,
                                                     BusinessDayConvention paymentConvention, bool automaticExercise,
                                                     const QuantLib::ext::shared_ptr<Index>& underlying, bool exercised,
                                                     Real priceAtExercise,
                                                     const QuantLib::ext::shared_ptr<QuantExt::FxIndex>& fxIndex,
                                                     const std::optional<QuantLib::Date>& cashSettlementFxFixingDate)
    : VanillaOption(QuantLib::ext::make_shared<CashOrNothingPayoff>(type, strike, cashPayoff),
                    QuantLib::ext::make_shared<AmericanExercise>(expiryDate)),
      automaticExercise_(automaticExercise), underlying_(underlying), exercised_(false),
      priceAtExercise_(Null<Real>()), fxIndex_(fxIndex), cashSettlementFxFixingDate_(cashSettlementFxFixingDate) {

    init(exercised, priceAtExercise);

    // Derive payment date from exercise date using the lag, calendar and convention.
    paymentDate_ = paymentCalendar.advance(expiryDate, paymentLag * QuantLib::Days, paymentConvention);

    check(exercise_->lastDate(), paymentDate_, automaticExercise_, underlying_, exercised_, priceAtExercise_);
}

void CashSettledAmericanOption::init(bool exercised, Real priceAtExercise) {
    if (exercised)
        exercise(priceAtExercise);

    registerWith(underlying_);
    registerWith(fxIndex_);
}

bool CashSettledAmericanOption::isExpired() const { return QuantLib::detail::simple_event(paymentDate_).hasOccurred(); }

void CashSettledAmericanOption::setupArguments(PricingEngine::arguments* args) const {

    VanillaOption::setupArguments(args);

    CashSettledAmericanOption::arguments* arguments = dynamic_cast<CashSettledAmericanOption::arguments*>(args);

    // We have a VanillaOption engine that will ignore the deferred payment.
    if (!arguments)
        return;

    // Set up the arguments specific to cash settled american option.
    arguments->paymentDate = paymentDate_;
    arguments->automaticExercise = automaticExercise_;
    arguments->underlying = underlying_;
    arguments->exercised = exercised_;
    arguments->priceAtExercise = priceAtExercise_;
    arguments->fxIndex = fxIndex_;
    arguments->cashSettlementFxFixingDate = cashSettlementFxFixingDate_;
}

void CashSettledAmericanOption::exercise(Real priceAtExercise) {
    QL_REQUIRE(priceAtExercise != Null<Real>(), "Cannot exercise with a null price.");
    exercised_ = true;
    priceAtExercise_ = priceAtExercise;
    update();
}

const Date& CashSettledAmericanOption::paymentDate() const { return paymentDate_; }

bool CashSettledAmericanOption::automaticExercise() const { return automaticExercise_; }

const QuantLib::ext::shared_ptr<Index>& CashSettledAmericanOption::underlying() const { return underlying_; }

bool CashSettledAmericanOption::exercised() const { return exercised_; }

Real CashSettledAmericanOption::priceAtExercise() const { return priceAtExercise_; }

void CashSettledAmericanOption::arguments::validate() const {
    QuantLib::VanillaOption::arguments::validate();
    check(exercise->lastDate(), paymentDate, automaticExercise, underlying, exercised, priceAtExercise);
}

} // namespace QuantExt
