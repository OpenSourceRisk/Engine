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
    : today_(Settings::instance().evaluationDate()), dates_(1, today_), valuationDates_(1, today_),
      tenors_(1, 0 * Days), times_(1, 0.0), timeGrid_(times_.begin(), times_.end()), isValuationDate_(1, true),
      isCloseOutDate_(1, false) {}

DateGrid::DateGrid(const string& grid, const QuantLib::Calendar& gridCalendar, const QuantLib::DayCounter& dayCounter)
    : calendar_(gridCalendar), dayCounter_(dayCounter) {

    today_ = Settings::instance().evaluationDate();

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
        QL_REQUIRE(!tokens.empty(), "DateGrid(): no tokens in grid spec '" << grid << "'");
        QuantLib::Integer gridSize;
        if (tokens.size() <= 2 &&
            tryParse(tokens[0], gridSize, std::function<QuantLib::Integer(const std::string& s)>(parseInteger))) {
            // uniform grid of format "numPillars,spacing" (e.g. 40,1M)
            Period gridTenor = 1 * Years; // default
            QL_REQUIRE(gridSize > 0, "DateGrid(): gridSize is zero, spec is '" << grid << "'");
            if (tokens.size() == 2)
                gridTenor = data::parsePeriod(tokens[1]);
            if (gridTenor == Period(1, Days)) {
                // we have a daily grid. Period and Calendar are not consistent with
                // working & actual days, so we set the tenor grid
                Date d = today_;
                for (Size i = 0; i < gridSize; i++) {
                    d = gridCalendar.advance(d, Period(1, Days), Following); // next working day
                    Size n = d - today_;
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
    today_ = Settings::instance().evaluationDate();
    buildDates(gridCalendar, dayCounter);
}

DateGrid::DateGrid(const vector<Date>& dates, const QuantLib::Calendar& cal, const DayCounter& dayCounter)
    : calendar_(cal), dayCounter_(dayCounter), dates_(dates) {
    QL_REQUIRE(!dates_.empty(), "Construction of DateGrid requires a non-empty vector of dates");
    QL_REQUIRE(is_sorted(dates_.begin(), dates_.end()),
               "Construction of DateGrid requires a sorted vector of unique dates");
    today_ = Settings::instance().evaluationDate();
    QL_REQUIRE(today_ < dates_.front(),
               "Construction of DateGrid requires first element to be strictly greater than today");

    // Populate the tenors, times and timegrid
    tenors_.resize(dates_.size());
    times_.resize(dates_.size());
    for (Size i = 0; i < dates_.size(); i++) {
        tenors_[i] = (dates_[i] - today_) * Days;
        times_[i] = dayCounter.yearFraction(today_, dates_[i]);
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

    dates_.clear();
    Date today = Settings::instance().evaluationDate();
    std::vector<Period> tenorsTmp;
    for(auto const& ten: tenors_) {
        Date d = ten.units() == Days ? cal.adjust(today + ten) : cal.advance(today, ten, Following, false);
        QL_REQUIRE(dates_.empty() || d >= dates_.back(), "DateGrid::buildDates(): tenors must be monotonic");
        if(dates_.empty() || d != dates_.back()) {
            dates_.push_back(d);
            tenorsTmp.push_back(ten);
        }
    }

    tenors_.swap(tenorsTmp);

    times_.resize(dates_.size());
    for (Size i = 0; i < dates_.size(); i++)
        times_[i] = dc.yearFraction(today, dates_[i]);

    timeGrid_ = TimeGrid(times_.begin(), times_.end());
    isValuationDate_ = std::vector<bool>(dates_.size(), true);
    isCloseOutDate_ = std::vector<bool>(dates_.size(), false);
    valuationDates_ = dates_;

    // Log the date grid
    log();
}

void DateGrid::log() {
    DLOG("DateGrid constructed, size = " << size());
    for (Size i = 0; i < tenors_.size(); i++)
        DLOG("[" << setw(2) << i << "] Tenor:" << tenors_[i] << ", Date:" << io::iso_date(dates_[i])
                 << ", Valuation:" << isValuationDate_[i] << ", CloseOut:" << isCloseOutDate_[i]);
}

void DateGrid::addCloseOutDates(const QuantLib::Period& p) {
    QL_REQUIRE(closeOutDates_.empty(),
               "DateGrid::addCloseOutDates(): close-out dates were already added, this can not be done twice.");
    valuationDates_.clear();
    if (p == QuantLib::Period(0, QuantLib::Days)) {
        for (Size i = 0; i < dates_.size(); ++i) {
            if (i == 0) {
                isCloseOutDate_.front() = false;
                isValuationDate_.front() = true;
                valuationDates_.push_back(dates_[i]);
            } else if (i == dates_.size() - 1) {
                isCloseOutDate_.back() = true;
                isValuationDate_.back() = false;
                closeOutDates_.push_back(dates_[i]);
            } else {
                isCloseOutDate_[i] = true;
                isValuationDate_[i] = true;
                valuationDates_.push_back(dates_[i]);
                closeOutDates_.push_back(dates_[i]);
            }
        }
    } else {
        valuationDates_ = dates_;
        for (auto const& d : dates_) {
            closeOutDates_.push_back(p.units() == Days ? calendar_.adjust(d + p)
                                                       : calendar_.advance(d, p, Following, false));
            QL_REQUIRE(
                closeOutDates_.size() == 1 || closeOutDates_.back() >= closeOutDates_[closeOutDates_.size() - 2],
                "DateGrid::addCloseOutDates(): internal error, added close-out date is earlier than the one before.");
        }
        std::set<Date> tmp;
        tmp.insert(valuationDates_.begin(), valuationDates_.end());
        tmp.insert(closeOutDates_.begin(), closeOutDates_.end());
        dates_.assign(tmp.begin(), tmp.end());
        isCloseOutDate_ = std::vector<bool>(dates_.size(), false);
        isValuationDate_ = std::vector<bool>(dates_.size(), true);
        for(Size i = 0; i < dates_.size(); ++i){
            isCloseOutDate_[i] = std::binary_search(closeOutDates_.begin(), closeOutDates_.end(), dates_[i]);
            isValuationDate_[i] = std::binary_search(valuationDates_.begin(), valuationDates_.end(), dates_[i]);
        }
        tenors_.resize(dates_.size());
        times_.resize(dates_.size());
        for(Size i=0;i<dates_.size();++i) {
            tenors_[i] = (dates_[i] - today_) * Days;
            times_[i] = dayCounter_.yearFraction(today_, dates_[i]);
        }
        timeGrid_ = TimeGrid(times_.begin(), times_.end());
    }
    // Log updated grid
    DLOG("Added Close Out Dates to DateGrid , size = " << size());
    log();
}

QuantLib::Date DateGrid::closeOutDateFromValuationDate(const QuantLib::Date& d) const {
    auto [f, _] = std::equal_range(valuationDates_.begin(), valuationDates_.end(), d);
    if (f == valuationDates_.end() || closeOutDates_.empty())
        return Date();
    return closeOutDates_[std::distance(valuationDates_.begin(), f)];
}

} // namespace data
} // namespace ore
