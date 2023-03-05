/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/calendars/mauritius.hpp>

namespace QuantExt {
Mauritius::Mauritius(Market m) {
    // all calendar instances share the same implementation instance
    static ext::shared_ptr<Calendar::Impl> impl(new Mauritius::SemImpl);
    impl_ = impl;
}

bool Mauritius::SemImpl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    if(isWeekend(w)
        // New year's day
        || (d == 1 && m == January)
        // Abolition of slavery
        || (d == 1 && m == February )
        // Independence and Republic day
        || (d == 12 && m == March )
        // Labour day
        || (d == 1 && m == May)
        // Arrival of indentured laborers
        || (d == 2 && m == November)
        // Christmas
        || (d == 25 && m == December )
        )
        return false;
    if(y == 2022){
        if( // New year holiday
            (d == 3 && m == January)
            // Thaipoosam Cavadee
            || (d == 18 && m == January)
            // Chinese Spring Festival
            || (d == 1 && m == February )
            // Maha Shivaratree
            || (d == 1 && m == March )
            // Eid-Ul-Fitr
            || (d == 3 && m == May )
            // Assumption of the blessed Virgin Mary
            || (d == 15 && m == August)
            // Ganesh Chartuthi
            || (d == 1 && m == September)
            // Divali
            || (d == 24 && m == October)
            )
            return false;
    }
    else if (y == 2023){
        if( // New year holiday
            (d == 2 && m == January)
            // New year holiday
            || (d == 3 && m == January)
            // Ugaadi
            || (d == 22 && m == March )
            // Labour day
            || (d == 1 && m == May )
            // Ganesh Chartuthi
            || (d == 20 && m == September)
            //All Saints Day
            || (d == 1 && m == November)
        )
            return false;
    }
    return true;
}
}