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

#include <qle/calendars/malaysia.hpp>

namespace QuantLib {

Malaysia::Malaysia(Market market) {
    // all calendar instances share the same implementation instance
    static QuantLib::ext::shared_ptr<Calendar::Impl> impl(new Malaysia::MyxImpl);
    impl_ = impl;
}

// Missing about 8 lunar calendar holidays
bool Malaysia::MyxImpl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    if (isWeekend(w)
        // New Years
        || ((d == 1 && m == January) || (d == 31 && m == December && w == Friday) ||
            (d == 2 && m == January && w == Monday))
        // Federal Territory Day
        || ((d == 1 || ((d == 2 || d == 3) && w == Monday)) && m == February)
        // Labour Day
        || ((d == 1 || (d == 2 && w == Monday)) && m == May)
        // National Day
        || ((d == 31 && m == August) || (d == 1 && w == Monday && m == September))
        // Malaysia Day
        || ((d == 16 || (d == 17 && w == Monday)) && m == September)
        // Christmas
        || ((d == 25 || (d == 26 && w == Monday)) && m == December))
        return false;
    return true;
}

} // namespace QuantLib
