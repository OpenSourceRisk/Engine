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

#include <ql/shared_ptr.hpp>
#include <qle/calendars/ice.hpp>

using namespace QuantLib;

namespace QuantExt {

ICE::ICE(ICE::Market market) {

    // All calendar instances on the same market share the same implementation instance
    static ext::shared_ptr<QuantLib::Calendar::Impl> futuresUSImpl(new ICE::FuturesUSImpl);
    static ext::shared_ptr<QuantLib::Calendar::Impl> futuresUSImpl_1(new ICE::FuturesUSImpl_1);
    static ext::shared_ptr<QuantLib::Calendar::Impl> futuresUSImpl_2(new ICE::FuturesUSImpl_2);
    static ext::shared_ptr<QuantLib::Calendar::Impl> futuresEUImpl(new ICE::FuturesEUImpl);
    static ext::shared_ptr<QuantLib::Calendar::Impl> futuresEUImpl_1(new ICE::FuturesEUImpl_1);
    static ext::shared_ptr<QuantLib::Calendar::Impl> endexEnergyImpl(new ICE::EndexEnergyImpl);
    static ext::shared_ptr<QuantLib::Calendar::Impl> endexEquitiesImpl(new ICE::EndexEquitiesImpl);
    static ext::shared_ptr<QuantLib::Calendar::Impl> swapTradeUSImpl(new ICE::SwapTradeUSImpl);
    static ext::shared_ptr<QuantLib::Calendar::Impl> swapTradeUKImpl(new ICE::SwapTradeUKImpl);
    static ext::shared_ptr<QuantLib::Calendar::Impl> futuresSingaporeImpl(new ICE::FuturesSingaporeImpl);

    switch (market) {
    case FuturesUS:
        impl_ = futuresUSImpl;
        break;
    case FuturesUS_1:
        impl_ = futuresUSImpl_1;
        break;
    case FuturesUS_2:
        impl_ = futuresUSImpl_2;
        break;
    case FuturesEU:
        impl_ = futuresEUImpl;
        break;
    case FuturesEU_1:
        impl_ = futuresEUImpl_1;
        break;
    case EndexEnergy:
        impl_ = endexEnergyImpl;
        break;
    case EndexEquities:
        impl_ = endexEquitiesImpl;
        break;
    case SwapTradeUS:
        impl_ = swapTradeUSImpl;
        break;
    case SwapTradeUK:
        impl_ = swapTradeUKImpl;
        break;
    case FuturesSingapore:
        impl_ = futuresSingaporeImpl;
        break;
    default:
        QL_FAIL("unknown market");
    }
}

bool ICE::FuturesUSImpl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    Day dd = date.dayOfYear();
    Day em = easterMonday(y);

    if (isWeekend(w)
        // New Year's Day (possibly moved to Monday if on Sunday)
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Good Friday
        || (dd == em - 3)
        // Christmas (Monday if Sunday or Friday if Saturday)
        || ((d == 25 || (d == 26 && w == Monday) || (d == 24 && w == Friday)) && m == December))
        return false;

    return true;
}

bool ICE::FuturesUSImpl_1::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    if (!FuturesUSImpl::isBusinessDay(date)
        // Martin Luther King's birthday (third Monday in January)
        || (y >= 1998 && (d >= 15 && d <= 21) && w == Monday && m == January)
        // Washington's birthday (third Monday in February)
        || ((d >= 15 && d <= 21) && w == Monday && m == February)
        // Memorial Day (last Monday in May)
        || (d >= 25 && w == Monday && m == May)
        // Independence Day (Monday if Sunday or Friday if Saturday)
        || ((d == 4 || (d == 5 && w == Monday) || (d == 3 && w == Friday)) && m == July)
        // Labor Day (first Monday in September)
        || (d <= 7 && w == Monday && m == September)
        // Thanksgiving Day (fourth Thursday in November)
        || ((d >= 22 && d <= 28) && w == Thursday && m == November))
        return false;

    return true;
}

bool ICE::FuturesUSImpl_2::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();

    if (!FuturesUSImpl::isBusinessDay(date)
        // Washington's birthday (third Monday in February)
        || ((d >= 15 && d <= 21) && w == Monday && m == February)
        // The Monday on or preceding 24 May (Victoria Day)
        || (d > 17 && d <= 24 && w == Monday && m == May)
        // July 1st, possibly moved to Monday (Canada Day)
        || ((d == 1 || ((d == 2 || d == 3) && w == Monday)) && m == July)
        // First Monday of August (Terry Fox Day)
        || (d <= 7 && w == Monday && m == August)
        // Labor Day (first Monday in September)
        || (d <= 7 && w == Monday && m == September)
        // Second Monday of October (Thanksgiving Day Canada)
        || (d > 7 && d <= 14 && w == Monday && m == October)
        // Boxing Day Canada (Monday if Sunday, Tuesday if Monday i.e. xmas day is Sunday)
        || ((d == 26 || (d == 27 && (w == Monday || w == Tuesday))) && m == December))
        return false;

    return true;
}

bool ICE::FuturesEUImpl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    Day dd = date.dayOfYear();
    Day em = easterMonday(y);

    if (isWeekend(w)
        // New Year's Day (Monday if on Sunday)
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Good Friday
        || (dd == em - 3)
        // Christmas (Monday if Sunday)
        || ((d == 25 || (d == 26 && w == Monday)) && m == December))
        return false;

    return true;
}

bool ICE::FuturesEUImpl_1::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();

    if (!FuturesEUImpl::isBusinessDay(date)
        // 26 Dec (Monday off if Sunday or Saturday is 26 Dec, Tuesday off if Monday is 26 Dec)
        || ((d == 26 || ((d == 27 || d == 28) && w == Monday) || (d == 27 && w == Tuesday)) && m == December))
        return false;

    return true;
}

bool ICE::EndexEnergyImpl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    Day dd = date.dayOfYear();
    Day em = easterMonday(y);

    if (isWeekend(w)
        // New Year's Day (Monday if on Sunday)
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Good Friday
        || (dd == em - 3)
        // Easter Monday
        || dd == em
        // Christmas (Monday if Sunday)
        || ((d == 25 || (d == 26 && w == Monday)) && m == December)
        // 26 Dec (Monday if Sunday)
        || ((d == 26 || (d == 27 && w == Monday)) && m == December))
        return false;

    return true;
}

bool ICE::EndexEquitiesImpl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    Day dd = date.dayOfYear();
    Day em = easterMonday(y);

    if (isWeekend(w)
        // New Year's Day (Monday if on Sunday)
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Good Friday
        || (dd == em - 3)
        // Easter Monday
        || dd == em
        // Labour Day
        || (d == 1 && m == May)
        // Christmas (Monday if Sunday)
        || ((d == 25 || (d == 26 && w == Monday)) && m == December)
        // 26 Dec (Monday if Sunday)
        || ((d == 26 || (d == 27 && w == Monday)) && m == December))
        return false;

    return true;
}

bool ICE::SwapTradeUSImpl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    Day dd = date.dayOfYear();
    Day em = easterMonday(y);

    if (isWeekend(w)
        // New Year's Day (possibly moved to Monday if on Sunday)
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Good Friday
        || (dd == em - 3)
        // Martin Luther King's birthday (third Monday in January)
        || (y >= 1998 && (d >= 15 && d <= 21) && w == Monday && m == January)
        // Washington's birthday (third Monday in February)
        || ((d >= 15 && d <= 21) && w == Monday && m == February)
        // Memorial Day (last Monday in May)
        || (d >= 25 && w == Monday && m == May)
        // Independence Day (Monday if Sunday or Friday if Saturday)
        || ((d == 4 || (d == 5 && w == Monday) || (d == 3 && w == Friday)) && m == July)
        // Labor Day (first Monday in September)
        || (d <= 7 && w == Monday && m == September)
        // Columbus Day
        || ((d >= 8 && d <= 14) && w == Monday && m == October && y >= 1971)
        // Veteran's Day: November 11th, adjusted
        || ((d == 11 || (d == 12 && w == Monday) || (d == 10 && w == Friday)) && m == November)
        // Thanksgiving Day (fourth Thursday in November)
        || ((d >= 22 && d <= 28) && w == Thursday && m == November)
        // Christmas (Monday if Sunday)
        || ((d == 25 || (d == 26 && w == Monday)) && m == December))
        return false;

    return true;
}

bool ICE::SwapTradeUKImpl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    Day dd = date.dayOfYear();
    Day em = easterMonday(y);

    if (isWeekend(w)
        // New Year's Day (possibly moved to Monday if on Sunday)
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Good Friday
        || (dd == em - 3)
        // Easter Monday
        || dd == em
        // first Monday of May (Early May Bank Holiday)
        || (d <= 7 && w == Monday && m == May)
        // last Monday of August (Summer Bank Holiday)
        || (d >= 25 && w == Monday && m == August)
        // Christmas (Monday if Sunday)
        || ((d == 25 || (d == 26 && w == Monday)) && m == December)
        // 26 Dec (Monday if Sunday)
        || ((d == 26 || (d == 27 && w == Monday)) && m == December))
        return false;

    return true;
}

bool ICE::FuturesSingaporeImpl::isBusinessDay(const Date& date) const {

    Weekday w = date.weekday();
    Day d = date.dayOfMonth();
    Month m = date.month();
    Year y = date.year();

    Day dd = date.dayOfYear();
    Day em = easterMonday(y);

    if (isWeekend(w)
        // New Year's Day (Monday if Sunday)
        || ((d == 1 || (d == 2 && w == Monday)) && m == January)
        // Good Friday
        || (dd == em - 3)
        // Christmas (Monday if Sunday)
        || ((d == 25 || (d == 26 && w == Monday)) && m == December))
        return false;

    return true;
}

} // namespace QuantExt
