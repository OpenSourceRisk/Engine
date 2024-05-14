/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file timeperiod.hpp
    \brief non-contiguous time period handling
    \ingroup utilities
*/

#pragma once

#include <ql/errors.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>

#include <vector>

namespace ore {
namespace data {
using QuantLib::Date;
using QuantLib::Size;

//! Handles non-contiguous time period
/*!
\ingroup utilities
*/
class TimePeriod {
public:
    /* The given vector of dates defines the contiguous parts of the time period
       as start1, end1, start2, end2, ...
       The single parts may overlap.
       If mporDays, and a Calendar are provided, each startdate will be adjusted 
       backward to include the mpor period
    */
    TimePeriod(const std::vector<Date>& dates, Size mporDays = QuantLib::Null<Size>(),
               const QuantLib::Calendar& calendar = QuantLib::Calendar());
    Size numberOfContiguousParts() const { return startDates_.size(); }
    const std::vector<Date>& startDates() const { return startDates_; }
    const std::vector<Date>& endDates() const { return endDates_; }
    bool contains(const Date& d) const;

private:
    std::vector<Date> startDates_, endDates_;
};

TimePeriod totalTimePeriod(std::vector<std::string> timePeriods, Size mporDays = QuantLib::Null<Size>(),
                           const QuantLib::Calendar& calendar = QuantLib::Calendar());

std::ostream& operator<<(std::ostream& out, const TimePeriod& t);

} // namespace data
} // namespace ore
