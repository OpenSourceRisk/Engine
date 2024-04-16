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
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/settings.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

DateGrid::DateGrid()
    : dates_(1, Settings::instance().evaluationDate()), tenors_(1, 0 * Days), times_(1, 0.0),
      timeGrid_(times_.begin(), times_.end()), isValuationDate_(1, true), isCloseOutDate_(1, false) {}

DateGrid::DateGrid(const string& grid, const QuantLib::Calendar& gridCalendar, const QuantLib::DayCounter& dayCounter)
    : calendar_(gridCalendar), dayCounter_(dayCounter) {

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
                // we have a daily grid. Period and Calendar are not consistent with
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
    : calendar_(gridCalendar), dayCounter_(dayCounter), tenors_(tenors) {
    QL_REQUIRE(!tenors_.empty(), "DateGrid requires a non-empty vector of tenors");
    QL_REQUIRE(is_sorted(tenors_.begin(), tenors_.end()),
               "Construction of DateGrid requires a sorted vector of unique tenors");
    buildDates(gridCalendar, dayCounter);
}

DateGrid::DateGrid(const vector<Date>& dates, const QuantLib::Calendar& cal, const DayCounter& dayCounter)
    : calendar_(cal), dayCounter_(dayCounter), dates_(dates) {
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
    isValuationDate_ = std::vector<bool>(dates_.size(), true);
    isCloseOutDate_ = std::vector<bool>(dates_.size(), false);

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
            dates_[i] = cal.advance(today, tenors_[i], Following, false);
        if (i > 0) {
            QL_REQUIRE(dates_[i] >= dates_[i - 1], "DateGrid::buildDates(): tenors must be monotonic");
            if (dates_[i] == dates_[i - 1]) {
                dates_.erase(std::next(dates_.begin(), i));
                tenors_.erase(std::next(tenors_.begin(), i));
                --i;
            }
        }
    }

    // Build times
    times_.resize(dates_.size());
    for (Size i = 0; i < dates_.size(); i++)
        times_[i] = dc.yearFraction(today, dates_[i]);

    timeGrid_ = TimeGrid(times_.begin(), times_.end());
    isValuationDate_ = std::vector<bool>(dates_.size(), true);
    isCloseOutDate_ = std::vector<bool>(dates_.size(), false);

    // Log the date grid
    log();
}

void DateGrid::log() {
    DLOG("DateGrid constructed, size = " << size());
    for (Size i = 0; i < tenors_.size(); i++)
        DLOG("[" << setw(2) << i << "] Tenor:" << tenors_[i] << ", Date:" << io::iso_date(dates_[i])
                 << ", Valuation:" << isValuationDate_[i] << ", CloseOut:" << isCloseOutDate_[i]);
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
        isValuationDate_.resize(len);
        isCloseOutDate_.resize(len);
        DLOG("DateGrid size now " << dates_.size());
    }
}

void DateGrid::addCloseOutDates(const QuantLib::Period& p) {
    valuationCloseOutMap_.clear();
    if (p == QuantLib::Period(0, QuantLib::Days)) {
        for (Size i = 0; i < dates_.size(); ++i) {
            if (i == 0) {
                isCloseOutDate_.front() = false;
                isValuationDate_.front() = true;
            } else if (i == dates_.size() - 1) {
                isCloseOutDate_.back() = true;
                isValuationDate_.back() = false;
            } else {
                isCloseOutDate_[i] = true;
                isValuationDate_[i] = true;
            }
            if (isCloseOutDate_[i] && i > 0)
                valuationCloseOutMap_[dates_[i-1]] = dates_[i];
        }
    } else {
        std::set<QuantLib::Date> tmpCloseOutDates;
        std::set<QuantLib::Date> tmpDates;
        std::set<QuantLib::Date> tmpValueDates;
        for (Size i = 0; i < dates_.size(); ++i) {
            Date c;
            if (p.units() == Days)
                c = calendar_.adjust(dates_[i] + p);
            else
                c = calendar_.advance(dates_[i], p, Following, false);
            tmpCloseOutDates.insert(c);
            valuationCloseOutMap_[dates_[i]] = c;
            tmpDates.insert(dates_[i]);
            tmpDates.insert(c);
            tmpValueDates.insert(dates_[i]);
        }
        dates_.clear();
        dates_.assign(tmpDates.begin(), tmpDates.end());
        isCloseOutDate_ = std::vector<bool>(dates_.size(), false);
        isValuationDate_ = std::vector<bool>(dates_.size(), true);
        for(size_t i = 0; i < dates_.size(); ++i){
            Date d = dates_[i];
            if (tmpCloseOutDates.count(d) == 1) {
                isCloseOutDate_[i] = true;
            }
            if(tmpValueDates.count(d) == 0){
                isValuationDate_[i] = false;
            }
        }
        // FIXME ... (is that needed anywhere ?)
        tenors_ = std::vector<QuantLib::Period>(dates_.size(), 0 * Days);
        times_.resize(dates_.size());
        Date today = Settings::instance().evaluationDate();
        for (Size i = 0; i < dates_.size(); i++)
            times_[i] = dayCounter_.yearFraction(today, dates_[i]);
        timeGrid_ = TimeGrid(times_.begin(), times_.end());
    }
    // Log Grid
    DLOG("Added Close Out Dates to DateGrid , size = " << size());
    log();
}

std::vector<QuantLib::Date> DateGrid::valuationDates() const {
    std::vector<Date> res;
    for (Size i = 0; i < dates_.size(); ++i) {
        if (isValuationDate_[i])
            res.push_back(dates_[i]);
    }
    return res;
}

std::vector<QuantLib::Date> DateGrid::closeOutDates() const {
    std::vector<Date> res;
    for (Size i = 0; i < dates_.size(); ++i) {
        if (isCloseOutDate_[i])
            res.push_back(dates_[i]);
    }
    return res;
}

QuantLib::TimeGrid DateGrid::valuationTimeGrid() const {
    std::vector<Real> times;
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < dates_.size(); ++i) {
        if (isValuationDate_[i])
            times.push_back(dayCounter_.yearFraction(today, dates_[i]));
    }
    return TimeGrid(times.begin(), times.end());
}

QuantLib::TimeGrid DateGrid::closeOutTimeGrid() const {
    std::vector<Real> times;
    Date today = Settings::instance().evaluationDate();
    for (Size i = 0; i < dates_.size(); ++i) {
        if (isCloseOutDate_[i])
            times.push_back(dayCounter_.yearFraction(today, dates_[i]));
    }
    return TimeGrid(times.begin(), times.end());
}

QuantLib::Date DateGrid::closeOutDateFromValuationDate(const QuantLib::Date& d) const {
    auto it = valuationCloseOutMap_.find(d);
    if(it == valuationCloseOutMap_.end()){
        return Date();
    } 
    return it->second;
}

QuantLib::ext::shared_ptr<DateGrid> generateShiftedDateGrid(const QuantLib::ext::shared_ptr<DateGrid>& dg,
                                                    const QuantLib::Period& shift) {
    DLOG("Building shifted date grid with shift of " << shift);
    vector<Date> defaultDates = dg->dates();
    vector<Date> closeOutDates;
    for (auto d : defaultDates) {
        Date closeOut = dg->calendar().adjust(d + shift);
        closeOutDates.push_back(closeOut);
    }
    QuantLib::ext::shared_ptr<DateGrid> newDg = QuantLib::ext::make_shared<DateGrid>(closeOutDates, dg->calendar(), dg->dayCounter());
    return newDg;
}

QuantLib::ext::shared_ptr<DateGrid> combineDateGrids(const QuantLib::ext::shared_ptr<DateGrid>& dg1,
                                             const QuantLib::ext::shared_ptr<DateGrid>& dg2) {
    DLOG("Combining date grids");
    vector<Date> combinedVec;
    vector<Date> dates1 = dg1->dates();
    vector<Date> dates2 = dg2->dates();
    combinedVec.reserve(dates1.size() + dates2.size());
    combinedVec.insert(combinedVec.end(), dates1.begin(), dates1.end());
    combinedVec.insert(combinedVec.end(), dates2.begin(), dates2.end());
    std::sort(combinedVec.begin(), combinedVec.end());
    auto last = std::unique(combinedVec.begin(), combinedVec.end());
    combinedVec.erase(last, combinedVec.end());
    // FIXME: Check that grid calendars and day counters match?
    QuantLib::ext::shared_ptr<DateGrid> newDg = QuantLib::ext::make_shared<DateGrid>(combinedVec, dg1->calendar(), dg1->dayCounter());
    return newDg;
}

} // namespace data
} // namespace ore
