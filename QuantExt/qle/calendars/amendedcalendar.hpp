/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file amendedcalendar.hpp
    \brief Amended calendar
*/

#ifndef quantext_amended_calendar_h
#define quantext_amended_calendar_h

#include <ql/time/calendar.hpp>
#include <ql/time/calendars/jointcalendar.hpp>

namespace QuantExt {

//! Amended calendar
/*! Any added or removed dates do not affect other implementations of this calendar.
    \ingroup calendars

*/
class AmendedCalendar : public QuantLib::Calendar {
private:
    class Impl : public Calendar::Impl {
    public:
        Impl(const QuantLib::Calendar& calendar, const std::string& name);
        std::string name() const override;
        bool isWeekend(QuantLib::Weekday) const override;
        bool isBusinessDay(const QuantLib::Date&) const override;

    private:
        std::string name_;
        QuantLib::Calendar baseCalendar_;
    };

public:
    AmendedCalendar(const QuantLib::Calendar&, const std::string& name);
};

} // namespace QuantExt

#endif
