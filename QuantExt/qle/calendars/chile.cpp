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

#include <qle/calendars/chile.hpp>

namespace QuantLib {

    Chile::Chile(Market market) {
        // all calendar instances share the same implementation instance
        static boost::shared_ptr<Calendar::Impl> impl(new Chile::SseImpl);
        impl_ = impl;
    }

    // Missing Peter & Paul, Independence Day, Armed Forces Day and Reformation
    // Day
    bool Chile::SseImpl::isBusinessDay(const Date& date) const {
        Weekday w = date.weekday();
        Day d = date.dayOfMonth(), dd = date.dayOfYear();
        Month m = date.month();
        Year y = date.year();
        Day em = easterMonday(y);
        if (isWeekend(w)
            // New Years Day
            || ((d == 1 || (d == 2 && w == Monday)) && 
                m == January)
            // Good Friday
            || (dd == em-3)
            // Labour Day
            || (d == 1 && m == May)
            // Battle of Iquique
            || (d == 21 && m == May)
            // Virgin del Carmen
            || (d == 16 && m == July)
            // Assumption Day
            || (d == 15 && m == August)
            // All Saints
            || (d == 1 && m == November)
            // Immaculate Conception
            || (d == 8 && m == December)
            // Christmas
            || (d == 25 && m == December))
            return false;
        return true;
    }

}

