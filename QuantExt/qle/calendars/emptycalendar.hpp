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

/*! \file emptycalendar.hpp
    \brief Empty calendar with the same structure as Null Calendar
*/

#ifndef quantext_empty_calendar_hpp
#define quantext_empty_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Empty calendar
/*! Like the Null Calendar, this calendar has no holidays.
    This allows us to explicitly specify a Null Calendar without
    coming across the default behaviour in some places which ignores
    the null calendar and subsequently uses the default currency
    calendar.

    \ingroup calendars
*/
class EmptyCalendar : public Calendar {
private:
    class Impl : public Calendar::Impl {
    public:
        std::string name() const { return "Empty"; }
        bool isWeekend(Weekday) const override { return false; }
        bool isBusinessDay(const Date&) const override { return true; }
    };

public:
    EmptyCalendar() {
        impl_ = ext::shared_ptr<Calendar::Impl>(new EmptyCalendar::Impl);
    }
};

} // namespace QuantExt

#endif
