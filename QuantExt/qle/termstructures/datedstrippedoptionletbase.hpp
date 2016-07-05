/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file qle/termstructures/datedstrippedoptionletbase.hpp
    \brief abstract class for optionlet surface with fixed reference date
*/

#pragma once

#include <ql/patterns/lazyobject.hpp>
#include <ql/time/businessdayconvention.hpp>
#include <ql/types.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/calendar.hpp>

#include <vector>

using namespace QuantLib;
using std::vector;

namespace QuantExt {
    /*! Abstract base class interface for a (time indexed) vector of (strike indexed) optionlet 
        (i.e. caplet/floorlet) volatilities with a fixed reference date.
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

}
