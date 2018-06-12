/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/algorithm/string.hpp>
#include <orea/simulation/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/settings.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace analytics {

DateGrid::DateGrid() : dates_(1, Settings::instance().evaluationDate()), 
    tenors_(1, 0 * Days), times_(1, 0.0), timeGrid_(times_.begin(), times_.end()) {}

DateGrid::DateGrid(const string& grid, const QuantLib::Calendar& gridCalendar, const QuantLib::DayCounter& dayCounter) {

    if (grid == "ALPHA") {
        // ALPHA is
        // quarterly up to 10Y,
        // annual up to 30Y,
        // quinquennial up to 100Y
        for (Size i = 1; i < 40; i++) { // 3M up to 39*3M = 117M = 9Y9M
            Period p(i * 3, Months);
            p.normalize();
            tenors_.push_back(p);
        }
        for (Size i = 10; i < 30; i++) // 10Y up to 29Y
            tenors_.push_back(Period(i, Years));
        for (Size i = 30; i < 105; i += 5) // 30Y up to 100Y
            tenors_.push_back(Period(i, Years));
    } else if (grid == "BETA") {
        // BETA is
        // monthly up to 10Y,
        // quarterly up to 20Y,
        // annually up to 50Y,
        // quinquennial up to 100Y
        for (Size i = 1; i < 119; i++) {
            Period p = i * Months;
            p.normalize();
            tenors_.push_back(p);
        }
        for (Size i = 40; i < 80; i++) {
            Period p = i * 3 * Months;
            p.normalize();
            tenors_.push_back(p);
        }
        for (Size i = 20; i < 50; i++)
            tenors_.push_back(i * Years);
        for (Size i = 50; i <= 100; i += 5)
            tenors_.push_back(i * Years);
    } else { // uniform grid of format "numPillars,spacing" (e.g. 40,1M)
        vector<string> tokens;
        boost::split(tokens, grid, boost::is_any_of(","));
        if (tokens.size() <= 2) {
            // uniform grid of format "numPillars,spacing" (e.g. 40,1M)
            Period gridTenor = 1 * Years; // default
            Size gridSize = atoi(tokens[0].c_str());
            QL_REQUIRE(gridSize > 0, "Invalid DateGrid string " << grid);
            if (tokens.size() == 2)
                gridTenor = data::parsePeriod(tokens[1]);
            if (gridTenor == Period(1, Days)) {
                // we have a daily grid. Period and Calendar are not consistant with
                // working & actual days, so we set the tenor grid
                Date today = Settings::instance().evaluationDate();
                Date d = today;
                for (Size i = 0; i < gridSize; i++) {
                    d = gridCalendar.advance(d, Period(1, Days), Following); // next working day
                    Size n = d - today;
                    tenors_.push_back(Period(n, Days));
                }
            } else {
                for (Size i = 0; i < gridSize; i++)
                    tenors_.push_back((i + 1) * gridTenor);
            }
        } else {
            // New style : 1D,2D,1W,2W,3Y,5Y,....
            for (Size i = 0; i < tokens.size(); i++)
                tenors_.push_back(data::parsePeriod(tokens[i]));
        }
    }
    buildDates(gridCalendar, dayCounter);
}

DateGrid::DateGrid(const vector<Period>& tenors, const QuantLib::Calendar& gridCalendar,
                   const QuantLib::DayCounter& dayCounter)
    : tenors_(tenors) {
    QL_REQUIRE(!tenors_.empty(), "DateGrid requires a non-empty vector of tenors");
    QL_REQUIRE(is_sorted(tenors_.begin(), tenors_.end()),
        "Construction of DateGrid requires a sorted vector of unique tenors");
    buildDates(gridCalendar, dayCounter);
}

DateGrid::DateGrid(const vector<Date>& dates, const DayCounter& dayCounter) : dates_(dates) {
    QL_REQUIRE(!dates_.empty(), "Construction of DateGrid requires a non-empty vector of dates");
    QL_REQUIRE(is_sorted(dates_.begin(), dates_.end()),
               "Construction of DateGrid requires a sorted vector of unique dates");
    Date today = Settings::instance().evaluationDate();
    QL_REQUIRE(today < dates_.front(),
               "Construction of DateGrid requires first element to be strictly greater than today");

    // Populate the tenors, times and timegrid
    tenors_.resize(dates_.size());
    times_.resize(dates_.size());
    for (Size i = 0; i < dates_.size(); i++) {
        tenors_[i] = (dates_[i] - today) * Days;
        times_[i] = dayCounter.yearFraction(today, dates_[i]);
    }
    timeGrid_ = TimeGrid(times_.begin(), times_.end());

    // Log the date grid
    log();
}

void DateGrid::buildDates(const QuantLib::Calendar& cal, const QuantLib::DayCounter& dc) {
    // build dates from tenors
    // this is called by both constructors
    dates_.resize(tenors_.size());
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < tenors_.size(); i++) {
        if (tenors_[i].units() == Days)
            dates_[i] = cal.adjust(today + tenors_[i]);
        else
            dates_[i] = cal.advance(today, tenors_[i], Following, true);
    }
    QL_REQUIRE(dates_.size() == tenors_.size(), "Date/Tenor mismatch");

    // Build times
    times_.resize(dates_.size());
    for (Size i = 0; i < dates_.size(); i++)
        times_[i] = dc.yearFraction(today, dates_[i]);

    timeGrid_ = TimeGrid(times_.begin(), times_.end());

    // Log the date grid
    log();
}

void DateGrid::log() {
    DLOG("DateGrid constructed, size = " << size());
    for (Size i = 0; i < tenors_.size(); i++)
        DLOG("[" << setw(2) << i << "] Tenor:" << tenors_[i] << ", Date:" << io::iso_date(dates_[i]));
}

void DateGrid::truncate(const Date& d, bool overrun) {
    if (d >= dates_.back())
        return; // no need for any truncation
    DLOG("Truncating DateGrid beyond " << QuantLib::io::iso_date(d));
    vector<Date>::iterator it = std::upper_bound(dates_.begin(), dates_.end(), d);
    if (overrun)
        ++it;
    dates_.erase(it, dates_.end());
    tenors_.resize(dates_.size());
    times_.resize(dates_.size());
    timeGrid_ = TimeGrid(times_.begin(), times_.end());
    DLOG("DateGrid size now " << dates_.size());
}

void DateGrid::truncate(Size len) {
    // Truncate grid up length len
    if (dates_.size() > len) {
        DLOG("Truncating DateGrid, removing elements " << dates_[len] << " to " << dates_.back());
        dates_.resize(len);
        tenors_.resize(len);
        times_.resize(len);
        timeGrid_ = TimeGrid(times_.begin(), times_.end());
        DLOG("DateGrid size now " << dates_.size());
    }
}
} // namespace analytics
} // namespace ore
