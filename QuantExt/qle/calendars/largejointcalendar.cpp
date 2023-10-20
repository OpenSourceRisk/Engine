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

#include <ql/errors.hpp>
#include <qle/calendars/largejointcalendar.hpp>
#include <sstream>

using namespace QuantLib;

namespace QuantExt {

std::string LargeJointCalendar::Impl::name() const {
    std::ostringstream out;
    switch (rule_) {
    case JoinHolidays:
        out << "JoinHolidays(";
        break;
    case JoinBusinessDays:
        out << "JoinBusinessDays(";
        break;
    default:
        QL_FAIL("unknown joint calendar rule");
    }
    out << calendars_.front().name();
    std::vector<Calendar>::const_iterator i;
    for (i = calendars_.begin() + 1; i != calendars_.end(); ++i)
        out << ", " << i->name();
    out << ")";
    return out.str();
}

bool LargeJointCalendar::Impl::isWeekend(Weekday w) const {
    std::vector<Calendar>::const_iterator i;
    switch (rule_) {
    case JoinHolidays:
        for (i = calendars_.begin(); i != calendars_.end(); ++i) {
            if (i->isWeekend(w))
                return true;
        }
        return false;
    case JoinBusinessDays:
        for (i = calendars_.begin(); i != calendars_.end(); ++i) {
            if (!i->isWeekend(w))
                return false;
        }
        return true;
    default:
        QL_FAIL("unknown joint calendar rule");
    }
}

bool LargeJointCalendar::Impl::isBusinessDay(const Date& date) const {
    std::vector<Calendar>::const_iterator i;
    switch (rule_) {
    case JoinHolidays:
        for (i = calendars_.begin(); i != calendars_.end(); ++i) {
            if (i->isHoliday(date))
                return false;
        }
        return true;
    case JoinBusinessDays:
        for (i = calendars_.begin(); i != calendars_.end(); ++i) {
            if (i->isBusinessDay(date))
                return true;
        }
        return false;
    default:
        QL_FAIL("unknown joint calendar rule");
    }
}

LargeJointCalendar::Impl::Impl(const std::vector<Calendar>& calendars, JointCalendarRule r) : rule_(r) {
    for (auto c : calendars) {
        calendars_.push_back(c);
    }
}

LargeJointCalendar::LargeJointCalendar(const std::vector<Calendar>& calendars, JointCalendarRule r) {
    impl_ = ext::shared_ptr<Calendar::Impl>(new LargeJointCalendar::Impl(calendars, r));
}

} // namespace QuantExt
