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

#include <qle/calendars/israel.hpp>

using namespace QuantLib;

namespace QuantExt {

Israel::Israel(MarketExt market) {

    // all calendar instances share the same implementation instance
    static QuantLib::ext::shared_ptr<Calendar::Impl> telborImpl = QuantLib::ext::make_shared<TelborImpl>();

    // Update the impl_ if we have been passed Telbor
    if (market == Telbor) {
        impl_ = telborImpl;
    }
}

bool Israel::TelborImpl::isWeekend(Weekday w) const { return w == Saturday || w == Sunday; }

bool Israel::TelborImpl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    if (isWeekend(w)
        // New Year's Day
        || (d == 1 && m == January)
        // General Elections
        || (((d == 9 && m == April) || (d == 17 && m == September)) && y == 2019) ||
        (d == 2 && m == March && y == 2020)
        // Holiday abroad
        || (((d == 22 && m == April) || (d == 27 && m == May)) && y == 2019) ||
        ((((d == 10 || d == 13) && m == April) || ((d == 8 || d == 25) && m == May)) && y == 2020)
        // Purim
        || (d == 24 && m == February && y == 2013) || (d == 16 && m == March && y == 2014) ||
        (d == 05 && m == March && y == 2015) || (d == 24 && m == March && y == 2016) ||
        (d == 12 && m == March && y == 2017) || (d == 1 && m == March && y == 2018) ||
        ((d == 21 || d == 22) && m == March && y == 2019) || ((d == 10 || d == 11) && m == March && y == 2020) ||
        (d == 26 && m == February && y == 2021) || (d == 17 && m == March && y == 2022) ||
        (d == 7 && m == March && y == 2023) || (d == 24 && m == March && y == 2024) ||
        (d == 14 && m == March && y == 2025) || (d == 3 && m == March && y == 2026) ||
        (d == 23 && m == March && y == 2027) || (d == 12 && m == March && y == 2028) ||
        (d == 1 && m == March && y == 2029) || (d == 19 && m == March && y == 2030) ||
        (d == 9 && m == March && y == 2031) || (d == 26 && m == February && y == 2032) ||
        (d == 15 && m == March && y == 2033) || (d == 5 && m == March && y == 2034) ||
        (d == 25 && m == March && y == 2035) || (d == 13 && m == March && y == 2036) ||
        (d == 1 && m == March && y == 2037) || (d == 21 && m == March && y == 2038) ||
        (d == 10 && m == March && y == 2039) || (d == 28 && m == February && y == 2040) ||
        (d == 17 && m == March && y == 2041) || (d == 6 && m == March && y == 2042) ||
        (d == 26 && m == March && y == 2043) ||
        (d == 13 && m == March && y == 2044)
        // Passover I and Passover VII
        || ((((d == 25 || d == 26 || d == 31) && m == March) || (d == 1 && m == April)) && y == 2013) ||
        ((d == 14 || d == 15 || d == 20 || d == 21) && m == April && y == 2014) ||
        ((d == 3 || d == 4 || d == 9 || d == 10) && m == April && y == 2015) ||
        ((d == 22 || d == 23 || d == 28 || d == 29) && m == April && y == 2016) ||
        ((d == 10 || d == 11 || d == 16 || d == 17) && m == April && y == 2017) ||
        (((d == 31 && m == March) || ((d == 5 || d == 6) && m == April)) && y == 2018) ||
        ((d == 19 || d == 26) && m == April && y == 2019) ||
        ((d == 8 || d == 9 || d == 15) && m == April && y == 2020) ||
        (((d == 28 && m == March) || (d == 3 && m == April)) && y == 2021) ||
        ((d == 16 || d == 22) && m == April && y == 2022) || ((d == 6 || d == 12) && m == April && y == 2023) ||
        ((d == 23 || d == 29) && m == April && y == 2024) || ((d == 13 || d == 19) && m == April && y == 2025) ||
        ((d == 2 || d == 8) && m == April && y == 2026) || ((d == 22 || d == 28) && m == April && y == 2027) ||
        ((d == 11 || d == 17) && m == April && y == 2028) ||
        (((d == 31 && m == March) || (d == 6 && m == April)) && y == 2029) ||
        ((d == 18 || d == 24) && m == April && y == 2030) || ((d == 8 || d == 14) && m == April && y == 2031) ||
        (((d == 27 && m == March) || (d == 2 && m == April)) && y == 2032) ||
        ((d == 14 || d == 20) && m == April && y == 2033) || ((d == 4 || d == 10) && m == April && y == 2034) ||
        ((d == 24 || d == 30) && m == April && y == 2035) || ((d == 12 || d == 18) && m == April && y == 2036) ||
        (((d == 31 && m == March) || (d == 6 && m == April)) && y == 2037) ||
        ((d == 20 || d == 26) && m == April && y == 2038) || ((d == 9 || d == 15) && m == April && y == 2039) ||
        (((d == 29 && m == March) || (d == 4 && m == April)) && y == 2040) ||
        ((d == 16 || d == 22) && m == April && y == 2041) || ((d == 5 || d == 11) && m == April && y == 2042) ||
        (((d == 25 && m == April) || (d == 1 && m == May)) && y == 2043) ||
        ((d == 12 || d == 18) && m == April && y == 2044)
        // Israel Independence Day
        || (d == 9 && m == May && y == 2019) ||
        (d == 29 && m == April && y == 2020)
        // Feast of Shavout (Pentecost)
        || (d == 29 && m == May && y == 2020)
        // Fast of Ninth of Av
        || (d == 30 && m == July && y == 2020)
        // Jewish New Year (Rosh Hashanah eve)
        || ((d == 4 || d == 5 || d == 6) && m == September && y == 2013) ||
        ((d == 24 || d == 25 || d == 26) && m == September && y == 2014) ||
        ((d == 13 || d == 14 || d == 15) && m == September && y == 2015) ||
        ((d == 2 || d == 3 || d == 4) && m == October && y == 2016) ||
        ((d == 20 || d == 21 || d == 22) && m == September && y == 2017) ||
        ((d == 9 || d == 10 || d == 11) && m == September && y == 2018) ||
        (((d == 30 && m == September) || (d == 1 && m == October)) && y == 2019) ||
        (d == 18 && m == September && y == 2020) || ((d == 7 || d == 8) && m == September && y == 2021) ||
        ((d == 26 || d == 27) && m == September && y == 2022) ||
        ((d == 16 || d == 17) && m == September && y == 2023) || ((d == 3 || d == 4) && m == October && y == 2024) ||
        ((d == 23 || d == 24) && m == September && y == 2025) ||
        ((d == 12 || d == 13) && m == September && y == 2026) || ((d == 2 || d == 3) && m == October && y == 2027) ||
        ((d == 21 || d == 22) && m == September && y == 2028) ||
        ((d == 10 || d == 11) && m == September && y == 2029) ||
        ((d == 28 || d == 29) && m == September && y == 2030) ||
        ((d == 18 || d == 19) && m == September && y == 2031) || ((d == 6 || d == 7) && m == September && y == 2032) ||
        ((d == 24 || d == 25) && m == September && y == 2033) ||
        ((d == 14 || d == 15) && m == September && y == 2034) || ((d == 4 || d == 5) && m == October && y == 2035) ||
        ((d == 22 || d == 23) && m == September && y == 2036) ||
        ((d == 10 || d == 11) && m == September && y == 2037) ||
        (((d == 30 && m == September) || (d == 01 && m == October)) && y == 2038) ||
        ((d == 19 || d == 20) && m == September && y == 2039) || ((d == 8 || d == 9) && m == September && y == 2040) ||
        ((d == 26 || d == 27) && m == September && y == 2041) ||
        ((d == 15 || d == 16) && m == September && y == 2042) || ((d == 5 || d == 6) && m == October && y == 2043) ||
        ((d == 22 || d == 23) && m == September && y == 2044)
        // New Year's Day
        || (d == 1 && m == January)
        // Day of Atonement (Yom Kippur)
        || ((d == 8 || d == 9) && m == October && y == 2019) ||
        (d == 28 && m == September && y == 2020)
        // First Day of Sukkot (Tabernacles)
        || (d == 14 && m == October && y == 2019)
        // Rejoicing of the Law Festival (Simchat Torah)
        || (d == 21 && m == October && y == 2019)
        // last Monday of May (Spring Bank Holiday)
        || (d >= 25 && w == Monday && m == May && y != 2002 && y != 2012)
        // Christmas
        || (d == 25 && m == December)
        // Day of Goodwill
        || (d == 26 && m == December && (y >= 2000 && y != 2020)))
        return false;

    return true;
}

} // namespace QuantExt
