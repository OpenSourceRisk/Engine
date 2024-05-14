/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/utilities/timeperiod.hpp>
#include <ored/utilities/parsers.hpp>

using std::vector;
using std::string;

namespace ore {
namespace data {
    
TimePeriod::TimePeriod(const std::vector<Date>& dates, Size mporDays, const QuantLib::Calendar& calendar) {
    QL_REQUIRE(dates.size() % 2 == 0, "TimePeriod: dates size must be an even number, got " << dates.size());
    for (Size i = 0; i < dates.size(); ++i) {
        if (i % 2 == 0) {
            Date sDate = dates[i];
            if (!calendar.empty() && mporDays != QuantLib::Null<Size>())
                sDate = calendar.advance(sDate, -mporDays * QuantLib::Days);
            startDates_.push_back(sDate);
        }
        else
            endDates_.push_back(dates[i]);
    }
}

bool TimePeriod::contains(const Date& d) const {
    for (Size i = 0; i < startDates_.size(); ++i) {
        if (d >= startDates_[i] && d <= endDates_[i])
            return true;
    }
    return false;
}

std::ostream& operator<<(std::ostream& out, const TimePeriod& t) {
    for (Size i = 0; i < t.numberOfContiguousParts(); ++i) {
        out << QuantLib::io::iso_date(t.startDates()[i]) << " to " << QuantLib::io::iso_date(t.endDates()[i]);
        if (i < t.numberOfContiguousParts() - 1)
            out << " + ";
    }
    return out;
}

TimePeriod totalTimePeriod(std::vector<std::string> timePeriods, Size mporDays, const QuantLib::Calendar& calendar) {
    vector<Date> allDates;
    for (const string& s : timePeriods) {
        auto dates = parseListOfValues<Date>(s, &parseDate);
        for (const auto& d : dates)
            allDates.push_back(d);
    }

    TimePeriod period(allDates);

    Date minDate = *std::min_element(period.startDates().begin(), period.startDates().end());
    Date maxDate = *std::max_element(period.endDates().begin(), period.endDates().end());

    vector<Date> finalDates;
    finalDates.push_back(minDate);
    finalDates.push_back(maxDate);

    TimePeriod totalPeriod(finalDates, mporDays, calendar);
    return totalPeriod;
}

} // namespace data
} // namespace ore