/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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
#include <ql/errors.hpp>
#include <qle/calendars/greece.hpp>
#include <qle/time/dateutilities.hpp>

namespace QuantExt {
using namespace QuantLib;

Greece::Greece() {
    // all calendar instances on the same market share the same
    // implementation instance
    impl_ = QuantLib::ext::make_shared<Greece::Impl>();
}

bool Greece::Impl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth(), dd = date.dayOfYear();
    Month m = date.month();
    Year y = date.year();
    Day em = easterMonday(y);
    Day laborDay = Date(1, May, y).dayOfYear();

    if (isWeekend(w)
        // New Year's Day
        || (d == 1 && m == January)
        // Ephiphany Day
        || (d == 6 && m == January)
        // Clean Monday
        || (dd == em - 49)
        // Good Friday
        || (dd == em - 3)
        // Easter Monday
        || (dd == em)
        // Greek Independence Day
        || (m == Mar && d == 25)
        // Labour Day
        || (m == May && ((d == 1) || ((w==Tuesday) && (d<=5) && (em-3 <= laborDay && laborDay <= em))))
        // Orthodox Pentecoast (Whit) Monday
        || (dd == em + 49)
        // Assumption Day
        || (m == August && d == 15)
        // Greek National Day
        || (m == October && d == 28)
        // Christmas Day
        || (m == December && d == 25)
        // Christmas Day
        || (m == December && d == 26))
        return false; // NOLINT(readability-simplify-boolean-expr)
    return true;
}

} // namespace QuantExt
