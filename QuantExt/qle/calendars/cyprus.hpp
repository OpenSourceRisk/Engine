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

/*! \file qle/calendars/cyprus.hpp
    \brief Cyprus Calendar
*/

#pragma once

#include <ql/time/calendar.hpp>

namespace QuantExt {

//! Cyprus Calendar
/*! Public holidays (data from https://www.centralbank.cy/en/the-bank/working-hours-bank-holidays):
    
    Fixed Dates
    <ul>
    <li>Saturdays</li>
    <li>Sundays</li>
    <li>New Year's Day, January 1st</li>
    <li>Ephiphany Day, 6th January</li>
    <li>Greek Independence Day, 25th March</li>
    <li>National Day, 1st April</li>
    <li>Labour Day, 1st May</li>
    <li>Assumption (Theotokos) Day, 15st August</li>
    <li>Cyprus Independence Day, 1st October</li>
    <li>Greece National Day, 28th October</li>
    <li>Christmas Day, December 25th</li>
    <li>Boxing Day, December 26th</li>
    </ul>

    Variable days
    <ul>
    <li>Green Monday</li>
    <li>Good Friday</li>
    <li>Easter Monday</li>
    <li>Easter Tuesday/li>
    <li>Orthodox Pentecost (Whit) Monday</li>
    </ul>

    \ingroup calendars

    \test the correctness of the returned results is tested
          against a list of known holidays.
*/
class Cyprus : public QuantLib::Calendar {
private:
    class Impl : public QuantLib::Calendar::OrthodoxImpl {
    public:
        std::string name() const override { return "Cyprus"; }
        bool isBusinessDay(const QuantLib::Date&) const override;
    };

public:
    Cyprus();
};
} // namespace QuantExt