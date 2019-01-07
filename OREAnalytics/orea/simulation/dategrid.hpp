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

/*! \file orea/simulation/dategrid.hpp
    \brief The date grid class
    \ingroup simulation
*/

#pragma once

#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/timegrid.hpp>

namespace ore {
namespace analytics {

//! Simulation Date Grid
/*!
  Utility for building a simulation date grid.
  \ingroup simulation
 */
class DateGrid {
public:
    //! Build a date grid with a single date equal to Settings::instance().evaluationDate()
    DateGrid();

    /*! Build a date grid from a string which can be of the form 40,1M or 1D,2D,1W,2W,3Y,5Y or a fixed name (ALPHA,
        BETA) indicating a hard coded grid structure
    */
    DateGrid(const std::string& grid, const QuantLib::Calendar& gridCalendar = QuantLib::TARGET(),
             const QuantLib::DayCounter& dayCounter = QuantLib::ActualActual());

    //! Build a date grid from the given vector of tenors.
    DateGrid(const std::vector<QuantLib::Period>& tenors,
             const QuantLib::Calendar& gridCalendar = QuantLib::TARGET(),
             const QuantLib::DayCounter& dayCounter = QuantLib::ActualActual());

    //! Build a date grid from an explicit set of dates, sorted in ascending order.
    DateGrid(const std::vector<QuantLib::Date>& dates,
             const QuantLib::DayCounter& dayCounter = QuantLib::ActualActual());

    //! The size of the date grid
    QuantLib::Size size() const { return dates_.size(); }

    //! Truncate the grid up to the given date.
    /*! If overrun is true, we make sure the last date in the grid is greater than
        the portfolio maturity, even though every scenario portfolio NPV will be 0
        at this point we may need the market data.
        If overrun is false, the last date in the grid is the last date where the
        portfolio is live.
     */
    void truncate(const QuantLib::Date& d, bool overrun = true);

    //! Truncate the grid to the given length
    void truncate(QuantLib::Size length);

    //! \name Inspectors
    //@{
    const std::vector<QuantLib::Period>& tenors() const { return tenors_; }
    const std::vector<QuantLib::Date>& dates() const { return dates_; }

    //! Returns the times from Settings::instance().evaluationDate to each Date using the day counter
    const std::vector<QuantLib::Time>& times() const { return times_; }

    //! Returns the time grid associated with the vector of times (plus t=0)
    const QuantLib::TimeGrid& timeGrid() const { return timeGrid_; }
    //@}

    //! Accessor methods
    const QuantLib::Date& operator[](QuantLib::Size i) const { return dates_[i]; };

private:
    void buildDates(const QuantLib::Calendar& cal, const QuantLib::DayCounter& dc);
    // Log the constructed DateGrid
    void log();

    std::vector<QuantLib::Date> dates_;
    std::vector<QuantLib::Period> tenors_;
    std::vector<QuantLib::Time> times_;
    QuantLib::TimeGrid timeGrid_;
};
} // namespace analytics
} // namespace ore
