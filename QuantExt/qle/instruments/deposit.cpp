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

#include <qle/instruments/deposit.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

Deposit::Deposit(const Real nominal, const Rate rate, const Period& tenor, const Natural fixingDays,
                 const Calendar& calendar, const BusinessDayConvention convention, const bool endOfMonth,
                 const DayCounter& dayCounter, const Date& tradeDate, const bool isLong, const Period forwardStart) {

    leg_.resize(3);
    index_ = boost::make_shared<IborIndex>("despoit-helper-index", tenor, fixingDays, Currency(), calendar, convention,
                                           endOfMonth, dayCounter);
    // move to next good day
    Date referenceDate = calendar.adjust(tradeDate);
    startDate_ = index_->valueDate(referenceDate);
    fixingDate_ = index_->fixingDate(startDate_);
    maturityDate_ = index_->maturityDate(startDate_);
    Real w = isLong ? 1.0 : -1.0;
    leg_[0] = boost::make_shared<Redemption>(-w * nominal, startDate_);
    leg_[1] =
        boost::make_shared<FixedRateCoupon>(maturityDate_, w * nominal, rate, dayCounter, startDate_, maturityDate_);
    leg_[2] = boost::make_shared<Redemption>(w * nominal, maturityDate_);
}

bool Deposit::isExpired() const { return detail::simple_event(maturityDate_).hasOccurred(); }

void Deposit::setupExpired() const {
    Instrument::setupExpired();
    fairRate_ = Null<Real>();
}

void Deposit::setupArguments(PricingEngine::arguments* args) const {

    Deposit::arguments* arguments = dynamic_cast<Deposit::arguments*>(args);
    QL_REQUIRE(arguments, "wrong argument type in deposit");
    arguments->leg = leg_;
    arguments->index = index_;
}

void Deposit::fetchResults(const PricingEngine::results* r) const {

    Instrument::fetchResults(r);
    const Deposit::results* results = dynamic_cast<const Deposit::results*>(r);
    QL_REQUIRE(results, "wrong result type");
    fairRate_ = results->fairRate;
}

void Deposit::arguments::validate() const {
    QL_REQUIRE(leg.size() == 3,
               "deposit arugments: unexpected number of cash flows (" << leg.size() << "), should be 3");
}

void Deposit::results::reset() {

    Instrument::results::reset();
    fairRate = Null<Real>();
}
} // namespace QuantExt
