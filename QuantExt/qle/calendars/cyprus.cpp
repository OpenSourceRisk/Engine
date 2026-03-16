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
#include <qle/calendars/cyprus.hpp>
#include <qle/time/dateutilities.hpp>
#include <ql/errors.hpp>

namespace QuantExt {
    using namespace QuantLib;

    Cyprus::Cyprus() {
        // all calendar instances on the same market share the same
        // implementation instance
        impl_ = QuantLib::ext::make_shared<Cyprus::Impl>();
    }

    bool Cyprus::Impl::isBusinessDay(const Date& date) const {
        Weekday w = date.weekday();
        Day d = date.dayOfMonth(), dd = date.dayOfYear();
        Month m = date.month();
        Year y = date.year();
        Day em = easterMonday(y);

        if (isWeekend(w)
            // New Year's Day 
            || (d == 1 && m == January)
            // Ephiphany Day
            || (d == 6 && m == January)
            // Green Monday
            || (dd == em-49)
            // Good Friday
            || (dd == em-3)
            // Easter Monday
            || (dd == em)
            // Easter Tuesday
            || (dd == em+1)
            // Greek Independence Day
            || (m == Mar && d == 25)
            // National Day
            || (m == Apr && d == 1)
            // Labour Day
            || (m == May && d == 1)
            // Orthodox Pentecoast (Whit) Monday
            || (dd == em+49)
            // Assumption Day (Theotokos)
            || (m == August && d == 15)
            // Cyprus Independence Day
            || (m == October && d == 1)
            // Greek National Day
            || (m == October && d == 28)
            // Christmas Day 
            || (m == December && d == 25)
            // Boxing Day 
            || (m == December && d == 26))
            return false; // NOLINT(readability-simplify-boolean-expr)
        return true;
    }

}

