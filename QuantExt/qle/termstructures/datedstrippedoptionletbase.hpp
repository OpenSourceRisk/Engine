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

/*! \file qle/termstructures/datedstrippedoptionletbase.hpp
    \brief abstract class for optionlet surface with fixed reference date
    \ingroup termstructures
*/

#pragma once

#include <ql/patterns/lazyobject.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/time/businessdayconvention.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/types.hpp>

#include <vector>

using namespace QuantLib;
using std::vector;

namespace QuantExt {
//! Stripped Optionlet base class interface
/*! Abstract base class interface for a (time indexed) vector of (strike indexed) optionlet
    (i.e. caplet/floorlet) volatilities with a fixed reference date.

            \ingroup termstructures
*/
class DatedStrippedOptionletBase : public LazyObject {
public:
    virtual const vector<Rate>& optionletStrikes(Size i) const = 0;
    virtual const vector<Volatility>& optionletVolatilities(Size i) const = 0;

    virtual const vector<Date>& optionletFixingDates() const = 0;
    virtual const vector<Time>& optionletFixingTimes() const = 0;
    virtual Size optionletMaturities() const = 0;

    virtual const vector<Rate>& atmOptionletRates() const = 0;

    virtual const Date& referenceDate() const = 0;
    virtual const DayCounter& dayCounter() const = 0;
    virtual const Calendar& calendar() const = 0;
    virtual BusinessDayConvention businessDayConvention() const = 0;
    virtual VolatilityType volatilityType() const = 0;
    virtual Real displacement() const = 0;
};
} // namespace QuantExt
