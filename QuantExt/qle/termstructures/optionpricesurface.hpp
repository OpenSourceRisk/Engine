/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file optionpricesurface.hpp
    \brief Surface to store option prices
 */

#pragma once

#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/termstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/date.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>

namespace QuantExt {

//! Option Price Surface based on sparse matrix.
//!  \ingroup termstructures
class OptionPriceSurface : public QuantLib::TermStructure,
                           public OptionInterpolator2d<QuantLib::Linear, QuantLib::Linear> {

public:
    OptionPriceSurface(const QuantLib::Date& referenceDate, const std::vector<QuantLib::Date>& dates,
                       const std::vector<QuantLib::Real>& strikes, const std::vector<QuantLib::Real>& prices,
                       const QuantLib::DayCounter& dayCounter,
                       const QuantLib::Calendar& calendar = QuantLib::NullCalendar())
        : QuantLib::TermStructure(referenceDate, calendar, dayCounter),
          OptionInterpolator2d<QuantLib::Linear, QuantLib::Linear>(referenceDate, dayCounter, dates, strikes, prices){};

    //! \name TermStructure interface
    //@{
    QuantLib::Date maxDate() const override { return QuantLib::Date::maxDate(); }
    const QuantLib::Date& referenceDate() const override { return OptionInterpolator2d::referenceDate(); }
    QuantLib::DayCounter dayCounter() const override { return OptionInterpolator2d::dayCounter(); }
    //@}

    QuantLib::Real price(QuantLib::Time t, QuantLib::Real strike) const { return getValue(t, strike); };
    QuantLib::Real price(QuantLib::Date d, QuantLib::Real strike) const { return getValue(d, strike); };
};

} // namespace QuantExt
