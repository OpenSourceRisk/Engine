/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/calendars/philippines.hpp>

namespace QuantExt {

Philippines::Philippines(Market market) {
    // all calendar instances share the same implementation instance
    static QuantLib::ext::shared_ptr<Calendar::Impl> impl(new Philippines::PheImpl);
    impl_ = impl;
}

// Missing Eidul Fitr and Eidul Adha (Lunar)
bool Philippines::PheImpl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth(), dd = date.dayOfYear();
    Month m = date.month();
    Year y = date.year();
    Day em = easterMonday(y);
    if (isWeekend(w)
        // New Years Day
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Day of Valor
        || (d == 9 && m == April)
        // Maundy Thursday
        || (dd == em - 4)
        // Good Friday
        || (dd == em - 3)
        // Labor Day
        || (d == 1 && m == May)
        // Independence Day
        || (d == 12 && m == June)
        // Ninoy Aquino Day
        || (d == 21 && m == August)
        // National Heroes (last Mon of August)
        || (d >= 25 && w == Monday && m == August)
        // All Saints Day
        || (d == 1 && m == November)
        // Bonifacio Day
        || (d == 30 && m == November)
        // Christmas
        || ((d == 25 || (d == 27 && (w == Monday || w == Tuesday))) && m == December)
        // Rizal Day
        || ((d == 30 && m == December) || (d == 2 && m == January && w == Tuesday))
        // New Years Eve
        || (d == 31 && m == December))
        return false;
    return true;
}

} // namespace QuantExt
