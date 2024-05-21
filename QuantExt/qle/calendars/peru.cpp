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

#include <qle/calendars/peru.hpp>

namespace QuantExt {

Peru::Peru(Market market) {
    // all calendar instances share the same implementation instance
    static QuantLib::ext::shared_ptr<Calendar::Impl> impl(new Peru::LseImpl);
    impl_ = impl;
}

bool Peru::LseImpl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth(), dd = date.dayOfYear();
    Month m = date.month();
    Year y = date.year();
    Day em = easterMonday(y);
    if (isWeekend(w)
        // New Years
        || (d == 1 && m == January)
        // if New Years on Thurs, get Fri off too
        || (d == 2 && w == Friday && m == January)
        // Maundy Thursday
        || (dd == em - 4)
        // Good Friday
        || (dd == em - 3)
        // Labour Day
        || (d == 1 && m == May)
        // Saint Peter and Saint Paul
        || (d == 29 && m == June)
        // Independence Day
        || ((d == 28 || (d == 27 && w == Friday) || (d == 29 && w == Monday)) && m == July)
        // if Ind Day on Thurs, get Fri off too
        || (d == 29 && w == Friday && m == July)
        // Santa Rosa de Lima
        || (d == 30 && m == August)
        // if SR on Thurs, get Fri off too
        || (d == 31 && w == Friday && m == August)
        // Battle of Angamos
        || (d == 8 && m == October)
        // All Saints Day
        || (d == 1 && m == November)
        // if Saints Day on Thurs, get Fri off too
        || (d == 2 && w == Friday && m == November)
        // Immaculate Conception
        || (d == 8 && m == December)
        // Christmas
        || (d == 25 && m == December))
        return false;
    return true;
}

} // namespace QuantExt
