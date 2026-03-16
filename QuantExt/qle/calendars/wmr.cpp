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

#include <qle/calendars/wmr.hpp>

using namespace QuantLib;

namespace QuantExt {

Wmr::Wmr(Market market) {
    // all calendar instances share the same implementation instance
    static QuantLib::ext::shared_ptr<Calendar::Impl> impl(new Wmr::SetImpl);
    impl_ = impl;
}

bool Wmr::SetImpl::isBusinessDay(const Date& date) const {
    Weekday w = date.weekday();
    Day d = date.dayOfMonth(), dd = date.dayOfYear();
    Month m = date.month();
    Year y = date.year();
    Day em = easterMonday(y);
    if (isWeekend(w)
        // New Years Day
        || ((d == 1 || ((d == 2 || d == 3) && w == Monday)) && m == January)
        // Easter Monday
        || (dd == em)
        // Christmas Day
        || ((d == 25 || ((d == 26 || d == 27) && w == Monday)) && m == December))
        return false;
    return true;
}

} // namespace QuantExt
