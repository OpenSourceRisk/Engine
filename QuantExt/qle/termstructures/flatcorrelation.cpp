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

#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/termstructures/flatcorrelation.hpp>

namespace QuantExt {
using namespace QuantLib;

FlatCorrelation::FlatCorrelation(const Date& referenceDate, const Handle<Quote>& correlation,
                                 const DayCounter& dayCounter)
    : CorrelationTermStructure(referenceDate, NullCalendar(), dayCounter), correlation_(correlation) {
    registerWith(correlation_);
}

FlatCorrelation::FlatCorrelation(const Date& referenceDate, Real correlation, const DayCounter& dayCounter)
    : CorrelationTermStructure(referenceDate, NullCalendar(), dayCounter),
      correlation_(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(correlation))) {}

FlatCorrelation::FlatCorrelation(Natural settlementDays, const Calendar& calendar, const Handle<Quote>& correlation,
                                 const DayCounter& dayCounter)
    : CorrelationTermStructure(settlementDays, calendar, dayCounter), correlation_(correlation) {
    registerWith(correlation_);
}

FlatCorrelation::FlatCorrelation(Natural settlementDays, const Calendar& calendar, Real correlation,
                                 const DayCounter& dayCounter)
    : CorrelationTermStructure(settlementDays, calendar, dayCounter),
      correlation_(QuantLib::ext::shared_ptr<Quote>(new SimpleQuote(correlation))) {}

} // namespace QuantExt
