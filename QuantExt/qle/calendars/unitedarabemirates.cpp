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
namespace {
bool isTrueWeekend(QuantLib::Date d) {
    // The UAE weekend was changed from 1st Jan 2022
    auto w = d.weekday();
    return (d < QuantLib::Date(1, QuantLib::Jan, 2022)) ?
          (w == QuantLib::Friday || w == QuantLib::Saturday) :
          (w == QuantLib::Saturday || w == QuantLib::Sunday);
}
}
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
    Day n = d.dayOfMonth();
    Month m = d.month();

    if (isTrueWeekend(d) ||
    (n==1 && m == Jan)   || // Gregorian new year
    (n==2 && m == Dec))     // National Day
        return false;
    else
        return true;
}
}