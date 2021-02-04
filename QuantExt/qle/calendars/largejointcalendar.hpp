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

/*! \file largejointcalendar.hpp
    \brief Joint calendar
*/

#ifndef quantext_large_joint_calendar_h
#define quantext_large_joint_calendar_h

#include <ql/time/calendar.hpp>
#include <ql/time/calendars/jointcalendar.hpp>

namespace QuantExt {

//! Large Joint calendar
/*! Similar to QuantLib's "JointCalendar" but allows a larger number of
    underlying calendars.

    \ingroup calendars

    \test the correctness of the returned results is tested by
          reproducing the calculations.
*/
class LargeJointCalendar : public QuantLib::Calendar {
private:
    class Impl : public Calendar::Impl {
    public:
        Impl(const std::vector<QuantLib::Calendar>& calendar, QuantLib::JointCalendarRule rule);
        std::string name() const;
        bool isWeekend(QuantLib::Weekday) const;
        bool isBusinessDay(const QuantLib::Date&) const;

    private:
        QuantLib::JointCalendarRule rule_;
        std::vector<QuantLib::Calendar> calendars_;
    };

public:
    explicit LargeJointCalendar(const std::vector<QuantLib::Calendar>&,
                                QuantLib::JointCalendarRule = QuantLib::JoinHolidays);
};

} // namespace QuantExt

#endif
