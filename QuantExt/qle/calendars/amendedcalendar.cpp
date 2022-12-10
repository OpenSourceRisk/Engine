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
#include <qle/calendars/amendedcalendar.hpp>
#include <sstream>

using namespace QuantLib;

namespace QuantExt {

std::string AmendedCalendar::Impl::name() const { return name_; }

bool AmendedCalendar::Impl::isWeekend(Weekday w) const { return baseCalendar_.isWeekend(w); }

bool AmendedCalendar::Impl::isBusinessDay(const Date& date) const { return baseCalendar_.isBusinessDay(date); }

AmendedCalendar::Impl::Impl(const QuantLib::Calendar& calendar, const std::string& name)
    : name_(name), baseCalendar_(calendar) {}

AmendedCalendar::AmendedCalendar(const QuantLib::Calendar& calendar, const std::string& name) {
    impl_ = ext::shared_ptr<Calendar::Impl>(new AmendedCalendar::Impl(calendar, name));
}

} // namespace QuantExt
