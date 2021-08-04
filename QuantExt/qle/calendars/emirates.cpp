/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/calendars/emirates.hpp>
#include <ql/errors.hpp>

namespace QuantExt {

    namespace {

        bool isUaeHoliday(Date d) {
            static std::vector<Date> holidays = {
                // 2021, according to https://wam.ae/en/details/1395302896477 and use Python hijri-converter package
                Date(1, January, 2021),   // New Year's Day
                Date(11, May, 2021),      // Eid Al Fitr
                Date(12, May, 2021),
                Date(13, May, 2021),
                Date(19, July, 2021),     // Arafat Day
                Date(20, July, 2021),     // Eid Al Adha
                Date(21, July, 2021),   
                Date(22, July, 2021),   
                Date(12, August, 2021),   // Islamic New Year
                Date(21, October, 2021),  // The Prophet's Birthday
                Date(1, December, 2021),  // Commemoration Day
                Date(2, December, 2021),  // UAE National Day
                
                // 2022 according to https://wam.ae/en/details/1395302896477
                Date(1, January, 2022),   // New Year's Day
                Date(1, May, 2022),       // Eid Al Fitr
                Date(2, May, 2022),
                Date(3, May, 2022),
                Date(4, May, 2022),
                Date(5, May, 2022),
                Date(9, July, 2022),      // Arafat Day
                Date(10, July, 2022),     // Eid Al Adha
                Date(11, July, 2022),   
                Date(12, July, 2022),   
                Date(30, July, 2022),     // Islamic New Year
                Date(8, October, 2022),   // The Prophet's Birthday
                Date(1, December, 2022),  // Marty's Day
                Date(2, December, 2022)   // UAE National Day
                
                // Add 2023 onwards as soon as published
            };

            for (auto& p : holidays) {
                if (d == p) {
                    return true;
                }
            }
            return false;
        }
    }

    UnitedArabEmirates::UnitedArabEmirates(Market market) {
        // all calendar instances share the same implementation instance
        static ext::shared_ptr<Calendar::Impl> uaeImpl(new UnitedArabEmirates::UaeImpl);
        switch (market) {
          case UAE:
            impl_ = uaeImpl;
            break;
          default:
            QL_FAIL("unknown market");
        }
    }

    bool UnitedArabEmirates::UaeImpl::isWeekend(Weekday w) const {
        return w == Friday || w == Saturday;
    }

    bool UnitedArabEmirates::UaeImpl::isBusinessDay(const Date& date) const {
        Day d = date.dayOfMonth();
        Month m = date.month();
        //Year y = date.year();
        Weekday w = date.weekday();
        
        if (isWeekend(w)
            || isUaeHoliday(date)
            // New Year's Day
            || (d == 1 && m == January)
            )
            return false; 
        return true;
    }

}

