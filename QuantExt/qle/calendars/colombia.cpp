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

#include <qle/calendars/colombia.hpp>

namespace QuantLib {

Colombia::Colombia(Market market) {
    // all calendar instances share the same implementation instance
    static QuantLib::ext::shared_ptr<Calendar::Impl> impl(new Colombia::CseImpl);
    impl_ = impl;
}

bool Colombia::CseImpl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth(), dd = date.dayOfYear();
    Month m = date.month();
    Year y = date.year();
    Day em = easterMonday(y);
    if (isWeekend(w)
        // New Years Day
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Dia de los Reyes Magos
        || ((d >= 6 && d <= 12) && w == Monday && m == January)
        // St. Josephs Day
        || ((d >= 19 && d <= 25) && w == Monday && m == March)
        // Maundy Thursday
        || (dd == em - 4)
        // Good Friday
        || (dd == em - 3)
        // Labour Day
        || (d == 1 && m == May)
        // Ascension Day
        || (dd == em + 42)
        // Corpus Christi
        || (dd == em + 63)
        // Sacred Heart
        || (dd == em + 70)
        // Saint Peter and Saint Paul
        || (((d >= 29 && m == June) || (d <= 5 && m == July)) && w == Monday)
        // Declaration of Independence
        || (d == 20 && m == July)
        // Battle of Boyaca
        || (d == 7 && m == August)
        // Assumption
        || ((d >= 15 && d <= 21) && w == Monday && m == August)
        // Colombus Day
        || ((d >= 12 && d <= 18) && w == Monday && m == October)
        // All Saints Day
        || ((d >= 1 && d <= 7) && w == Monday && m == November)
        // Independence of Cartagena
        || ((d >= 12 && d <= 18) && w == Monday && m == November)
        // Immaculate Conception
        || (d == 8 && m == December)
        // Christmas
        || (d == 25 && m == December))
        return false;
    return true;
}

} // namespace QuantLib
