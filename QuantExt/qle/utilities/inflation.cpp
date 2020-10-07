/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/utilities/inflation.hpp>

using QuantLib::Date;
using QuantLib::DayCounter;
using QuantLib::Frequency;
using QuantLib::Handle;
using QuantLib::InflationIndex;
using QuantLib::InflationTermStructure;
using QuantLib::inflationYearFraction;
using QuantLib::Real;
using QuantLib::Time;
using QuantLib::ZeroInflationTermStructure;
using std::pow;

namespace QuantExt {

Time inflationTime(const Date& date,
    const boost::shared_ptr<InflationTermStructure>& inflationTs,
    const DayCounter& dayCounter) {

    DayCounter dc = inflationTs->dayCounter();
    if (dayCounter != DayCounter())
        dc = dayCounter;

    return inflationYearFraction(inflationTs->frequency(), inflationTs->indexIsInterpolated(), 
        dc, inflationTs->baseDate(), date);
}

Real inflationGrowth(const Handle<ZeroInflationTermStructure>& ts, Time t, const DayCounter& dc) {
    auto lag = inflationTime(ts->referenceDate(), *ts, dc);
    return pow(1.0 + ts->zeroRate(t - lag), t);
}

Real inflationGrowth(const Handle<ZeroInflationTermStructure>& ts, Time t) {
    return inflationGrowth(ts, t, ts->dayCounter());
}

}
