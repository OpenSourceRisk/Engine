/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file malaysia.hpp
    \brief Malaysian calendar
*/

#ifndef quantext_malaysian_calendar_hpp
#define quantext_malaysian_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantLib {

class Malaysia : public Calendar {
private:
    class MyxImpl : public Calendar::WesternImpl {
    public:
        std::string name() const override { return "Malaysia Stock Exchange"; }
        bool isBusinessDay(const Date&) const override;
    };

public:
    enum Market {
        MYX // Malaysia Stock Exchange
    };
    Malaysia(Market m = MYX);
};

} // namespace QuantLib

#endif
