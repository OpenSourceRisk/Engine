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

#include <qle/calendars/switzerland.hpp>

namespace QuantExt {
using namespace QuantLib;

Switzerland::Switzerland(Market market) {
    // all calendar instances share the same implementation instance
    static ext::shared_ptr<Calendar::Impl> settlementImpl(new Switzerland::SettlementImpl);
    static ext::shared_ptr<Calendar::Impl> sixImpl(new Switzerland::SixImpl);
    switch (market) {
    case Settlement:
        impl_ = settlementImpl;
        break;
    case SIX:
        impl_ = sixImpl;
        break;
    default:
        QL_FAIL("unknown market");
    }
}

bool Switzerland::SettlementImpl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth(), dd = date.dayOfYear();
    Month m = date.month();
    Year y = date.year();
    Day em = easterMonday(y);
    if (isWeekend(w)
        // New Year's Day
        || (d == 1 && m == January)
        // Berchtoldstag
        || (d == 2 && m == January)
        // Good Friday
        || (dd == em - 3)
        // Easter Monday
        || (dd == em)
        // Ascension Day
        || (dd == em + 38)
        // Whit Monday
        || (dd == em + 49)
        // Labour Day
        || (d == 1 && m == May)
        // National Day
        || (d == 1 && m == August)
        // Christmas
        || (d == 25 && m == December)
        // St. Stephen's Day
        || (d == 26 && m == December))
        return false;
    return true;
}

bool Switzerland::SixImpl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth(), dd = date.dayOfYear();
    Month m = date.month();
    Year y = date.year();
    Day em = easterMonday(y);
    if (isWeekend(w)
        // New Year's Day
        || (d == 1 && m == January)
        // Berchtoldstag
        || (d == 2 && m == January)
        // Good Friday
        || (dd == em - 3)
        // Easter Monday
        || (dd == em)
        // Ascension Day
        || (dd == em + 38)
        // Whit Monday
        || (dd == em + 49)
        // Labour Day
        || (d == 1 && m == May)
        // National Day
        || (d == 1 && m == August)
        // Christmas Eve
        || (d == 24 && m == December)
        // Christmas
        || (d == 25 && m == December)
        // St. Stephen's Day
        || (d == 26 && m == December)
        // New Year Eve
        || (d == 31 && m == December))
        return false;
    return true;
}

} // namespace QuantExt
