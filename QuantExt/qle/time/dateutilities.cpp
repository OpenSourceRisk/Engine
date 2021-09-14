/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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

#include <qle/time/dateutilities.hpp>

namespace QuantExt {
namespace DateUtilities {

using QuantLib::Date;
using QuantLib::Month;
using QuantLib::Weekday;
using QuantLib::Year;

Date lastWeekday(Weekday dayOfWeek, Month m, Year y) {
    Date endOfMonth = Date::endOfMonth(Date(1, m, y));
    Weekday lastWeekDayOfTheMonth = endOfMonth.weekday();
    auto lastDayOfMonth = endOfMonth.dayOfMonth();
    if (lastWeekDayOfTheMonth >= dayOfWeek) {
        return Date(lastDayOfMonth - (lastWeekDayOfTheMonth - dayOfWeek), m, y);
    } else {
        return Date(lastDayOfMonth - 7 + (dayOfWeek - lastWeekDayOfTheMonth), m, y);
    }
}

} // namespace DateUtilities
} // namespace QuantExt
