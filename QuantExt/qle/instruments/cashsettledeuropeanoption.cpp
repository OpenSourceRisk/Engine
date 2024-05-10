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
#include <qle/instruments/cashsettledeuropeanoption.hpp>

using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::EuropeanExercise;
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

    QL_REQUIRE(paymentDate >= expiryDate, "Cash settled European option payment date ("
                                              << iso_date(paymentDate)
                                              << ") must be greater than or equal to the expiry date ("
                                              << iso_date(expiryDate) << ")");

    if (automaticExercise) {
        QL_REQUIRE(underlying, "Cash settled European option has automatic exercise so we need a valid underlying.");
    }

    if (exercised) {
        QL_REQUIRE(priceAtExercise != Null<Real>(), "Cash settled European option was exercised so we need "
                                                        << "a valid exercise price.");
    }
}

} // namespace

namespace QuantExt {

CashSettledEuropeanOption::CashSettledEuropeanOption(Option::Type type, Real strike, const Date& expiryDate,
                                                     const Date& paymentDate, bool automaticExercise,
                                                     const QuantLib::ext::shared_ptr<Index>& underlying, bool exercised,
                                                     Real priceAtExercise)
    : VanillaOption(QuantLib::ext::make_shared<PlainVanillaPayoff>(type, strike),
                    QuantLib::ext::make_shared<EuropeanExercise>(expiryDate)),
      paymentDate_(paymentDate), automaticExercise_(automaticExercise), underlying_(underlying), exercised_(false),
      priceAtExercise_(Null<Real>()) {

    init(exercised, priceAtExercise);

    check(exercise_->lastDate(), paymentDate_, automaticExercise_, underlying_, exercised_, priceAtExercise_);
}

CashSettledEuropeanOption::CashSettledEuropeanOption(Option::Type type, Real strike, const Date& expiryDate,
                                                     Natural paymentLag, const Calendar& paymentCalendar,
                                                     BusinessDayConvention paymentConvention, bool automaticExercise,
                                                     const QuantLib::ext::shared_ptr<Index>& underlying, bool exercised,
                                                     Real priceAtExercise)
    : VanillaOption(QuantLib::ext::make_shared<PlainVanillaPayoff>(type, strike),
                    QuantLib::ext::make_shared<EuropeanExercise>(expiryDate)),
      automaticExercise_(automaticExercise), underlying_(underlying), exercised_(false),
      priceAtExercise_(Null<Real>()) {

    init(exercised, priceAtExercise);

    // Derive payment date from exercise date using the lag, calendar and convention.
    paymentDate_ = paymentCalendar.advance(expiryDate, paymentLag * QuantLib::Days, paymentConvention);

    check(exercise_->lastDate(), paymentDate_, automaticExercise_, underlying_, exercised_, priceAtExercise_);
}

CashSettledEuropeanOption::CashSettledEuropeanOption(Option::Type type, Real strike, Real cashPayoff,
                                                     const Date& expiryDate, const Date& paymentDate,
                                                     bool automaticExercise, const QuantLib::ext::shared_ptr<Index>& underlying,
                                                     bool exercised, Real priceAtExercise)
    : VanillaOption(QuantLib::ext::make_shared<CashOrNothingPayoff>(type, strike, cashPayoff),
                    QuantLib::ext::make_shared<EuropeanExercise>(expiryDate)),
      paymentDate_(paymentDate), automaticExercise_(automaticExercise), underlying_(underlying), exercised_(false),
      priceAtExercise_(Null<Real>()) {

    init(exercised, priceAtExercise);

    check(exercise_->lastDate(), paymentDate_, automaticExercise_, underlying_, exercised_, priceAtExercise_);
}

CashSettledEuropeanOption::CashSettledEuropeanOption(Option::Type type, Real strike, Real cashPayoff,
                                                     const Date& expiryDate, Natural paymentLag,
                                                     const Calendar& paymentCalendar,
                                                     BusinessDayConvention paymentConvention, bool automaticExercise,
                                                     const QuantLib::ext::shared_ptr<Index>& underlying, bool exercised,
                                                     Real priceAtExercise)
    : VanillaOption(QuantLib::ext::make_shared<CashOrNothingPayoff>(type, strike, cashPayoff),
                    QuantLib::ext::make_shared<EuropeanExercise>(expiryDate)),
      automaticExercise_(automaticExercise), underlying_(underlying), exercised_(false),
      priceAtExercise_(Null<Real>()) {

    init(exercised, priceAtExercise);

    // Derive payment date from exercise date using the lag, calendar and convention.
    paymentDate_ = paymentCalendar.advance(expiryDate, paymentLag * QuantLib::Days, paymentConvention);

    check(exercise_->lastDate(), paymentDate_, automaticExercise_, underlying_, exercised_, priceAtExercise_);
}

void CashSettledEuropeanOption::init(bool exercised, Real priceAtExercise) {
    if (exercised)
        exercise(priceAtExercise);

    if (automaticExercise_ && underlying_)
        registerWith(underlying_);
}

bool CashSettledEuropeanOption::isExpired() const { return QuantLib::detail::simple_event(paymentDate_).hasOccurred(); }

void CashSettledEuropeanOption::setupArguments(PricingEngine::arguments* args) const {

    VanillaOption::setupArguments(args);

    CashSettledEuropeanOption::arguments* arguments = dynamic_cast<CashSettledEuropeanOption::arguments*>(args);

    // We have a VanillaOption engine that will ignore the deferred payment.
    if (!arguments)
        return;

    // Set up the arguments specific to cash settled european option.
    arguments->paymentDate = paymentDate_;
    arguments->automaticExercise = automaticExercise_;
    arguments->underlying = underlying_;
    arguments->exercised = exercised_;
    arguments->priceAtExercise = priceAtExercise_;
}

void CashSettledEuropeanOption::exercise(Real priceAtExercise) {
    QL_REQUIRE(priceAtExercise != Null<Real>(), "Cannot exercise with a null price.");
    QL_REQUIRE(Settings::instance().evaluationDate() >= exercise_->lastDate(),
               "European option cannot be "
                   << "exercised before expiry date. Valuation date " << iso_date(Settings::instance().evaluationDate())
                   << " is before expiry date " << iso_date(exercise_->lastDate()) << ".");
    exercised_ = true;
    priceAtExercise_ = priceAtExercise;
    update();
}

const Date& CashSettledEuropeanOption::paymentDate() const { return paymentDate_; }

bool CashSettledEuropeanOption::automaticExercise() const { return automaticExercise_; }

const QuantLib::ext::shared_ptr<Index>& CashSettledEuropeanOption::underlying() const { return underlying_; }

bool CashSettledEuropeanOption::exercised() const { return exercised_; }

Real CashSettledEuropeanOption::priceAtExercise() const { return priceAtExercise_; }

void CashSettledEuropeanOption::arguments::validate() const {
    QuantLib::VanillaOption::arguments::validate();
    check(exercise->lastDate(), paymentDate, automaticExercise, underlying, exercised, priceAtExercise);
}

} // namespace QuantExt
