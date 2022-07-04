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

#include <qle/calendars/cme.hpp>

using namespace QuantLib;

namespace QuantExt {

CME::CME() {

    // All calendar instances share the same implementation instance
    static ext::shared_ptr<QuantLib::Calendar::Impl> impl(new Impl);
    impl_ = impl;
}

bool CME::Impl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    Day dd = date.dayOfYear();
    Day em = easterMonday(y);

    if (isWeekend(w)
        // New Year's Day (possibly moved to Monday if on Sunday)
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Martin Luther King's birthday (third Monday in January)
        || (y >= 1998 && (d >= 15 && d <= 21) && w == Monday && m == January)
        // Washington's birthday (third Monday in February)
        || ((d >= 15 && d <= 21) && w == Monday && m == February)
        // Good Friday
        || (dd == em - 3)
        // Memorial Day (last Monday in May)
        || (d >= 25 && w == Monday && m == May)
        // Independence Day (Monday if Sunday or Friday if Saturday)
        || ((d == 4 || (d == 5 && w == Monday) || (d == 3 && w == Friday)) && m == July)
        // Labor Day (first Monday in September)
        || (d <= 7 && w == Monday && m == September)
        // Thanksgiving Day (fourth Thursday in November)
        || ((d >= 22 && d <= 28) && w == Thursday && m == November)
        // Christmas (Monday if Sunday or Friday if Saturday)
        || ((d == 25 || (d == 26 && w == Monday) || (d == 24 && w == Friday)) && m == December))
        return false;

    return true;
}

} // namespace QuantExt
