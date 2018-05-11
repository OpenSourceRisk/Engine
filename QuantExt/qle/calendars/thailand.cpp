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

#include <qle/calendars/thailand.hpp>

namespace QuantLib {

    Thailand::Thailand(Market market) {
        // all calendar instances share the same implementation instance
        static boost::shared_ptr<Calendar::Impl> impl(new Thailand::SetImpl);
        impl_ = impl;
    }

    // Missing Lunar holidays
    bool Thailand::SetImpl::isBusinessDay(const Date& date) const {
        Weekday w = date.weekday();
        Day d = date.dayOfMonth();
        Month m = date.month();
        if (isWeekend(w)
            // New Years Day
            || ((d == 1 || ((d == 2 || d == 3) && w == Monday))
                && m == January)
            // Chakri Day
            || ((d == 6 || ((d == 7 || d == 8) && w == Monday)) &&
                m == April)
            // Songkran Festival
            || (d == 13 && m == April)
            || ((d == 14 || (d == 16 && w == Tuesday))
                && m == April)
            || ((d == 15 || (d == 16 && w == Monday))
                && m == April)
            // Labour Day
            || ((d == 1 || ((d == 2 || d == 3) && w == Monday)) &&
                m == May)
            // H. M. King's Birthday
            || ((d == 28 || ((d == 29 || d == 30) && w == Monday)) &&
                m == July)
            //Queens Birthday
            || ((d == 12 || ((d == 13 || d == 14) && w == Monday)) &&
                m == August)
            // Memorial Day for King
            || ((d == 13 || ((d == 14 || d == 15) && w == Monday)) &&
                m == October)
            // Chulalongkorn Day
            || ((d == 23 || ((d == 24 || d == 25) && w == Monday)) &&
                m == October)
            // King Bhumibol's Birthday
            || ((d == 5 || ((d == 6 || d == 7) && w == Monday)) &&
                m == December)
            // Constitution
            || ((d == 10 || ((d == 11 || d == 12) && w == Monday)) &&
                m == December)
            // New Years Eve
            || (d == 31 && m == December) || (( d == 2 || d ==3) && w == Tuesday && 
                m == January))
            return false;
        return true;
    }

}

