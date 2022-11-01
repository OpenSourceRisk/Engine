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

#include "unitedarabemirates.hpp"

namespace QuantExt {
UnitedArabEmirates::UnitedArabEmirates() {
    // all calendar instances share the same implementation instance
    static ext::shared_ptr<Calendar::Impl> impl(new UnitedArabEmirates::Impl);
    impl_ = impl;
}

bool UnitedArabEmirates::Impl::isWeekend(QuantLib::Weekday w) const {
    return w == QuantLib::Saturday || w == QuantLib::Sunday ;
}

bool UnitedArabEmirates::Impl::isBusinessDay(const QuantLib::Date& d) const {
    /*Introduction to public holidays in the UAE

    There are 14 official public holidays in the UAE.

    One of the biggest public holiday events is Eid al-Fitr, which marks the end of Ramadan.
    It begins the day after the sighting of the crescent moon, so the dates can only be estimated and may vary by a day or two. */

    const static std::vector<Date> publiHolidays{
        Date(1, Jan,2022), //New Year’s Day
        Date(30,Apr,2022), //Eid al-Fitr holiday
        Date(1, May,2022), //Eid al-Fitr holiday
        Date(2, May,2022), //Eid al-Fitr
        Date(3, May,2022), //Eid al-Fitr holiday
        Date(4, May,2022), //Eid al-Fitr holiday
        Date(8, Jul,2022), //Arafat (Hajj) Day
        Date(9, Jul,2022), //Eid al-Adha (Feast of Sacrifice)
        Date(10,Jul,2022), //Eid al-Adha holiday
        Date(11,Jul,2022), //Eid al-Adha holiday
        Date(30,Jul,2022), //Al-Hijra (Islamic New Year)
        Date(1, Dec,2022), //Commemoration Day
        Date(2, Dec,2022), //National Day
        Date(3, Dec,2022)  //National Day holiday
    };
    bool isPublicHoliday = std::count(publiHolidays.begin(), publiHolidays.end(), d)>0;
    return !isWeekend(d.weekday()) && !isPublicHoliday;
}
}