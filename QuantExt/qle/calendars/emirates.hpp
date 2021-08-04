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

/*! \file emirates.hpp
    \brief United Arab Emirates calendar
*/

#ifndef quantext_emirates_calendar_hpp
#define quantext_emirates_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantExt {
using namespace QuantLib;

class UnitedArabEmirates : public Calendar {
private:
    class UaeImpl : public Calendar::Impl {
    public:
        std::string name() const override { return "UAE calendar"; }
        bool isWeekend(Weekday) const override;
        bool isBusinessDay(const Date&) const override;
    };

public:
    enum Market {
        UAE // UAE EIBOR fixings
    };
    UnitedArabEmirates(Market m = UAE);
};

} // namespace QuantExt

#endif
