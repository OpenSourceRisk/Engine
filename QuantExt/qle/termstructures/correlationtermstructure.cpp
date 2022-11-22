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

#include <qle/termstructures/correlationtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

CorrelationTermStructure::CorrelationTermStructure(const DayCounter& dc) : TermStructure(dc) {}

CorrelationTermStructure::CorrelationTermStructure(const Date& referenceDate, const Calendar& cal, const DayCounter& dc)
    : TermStructure(referenceDate, cal, dc) {}

CorrelationTermStructure::CorrelationTermStructure(Natural settlementDays, const Calendar& cal, const DayCounter& dc)
    : TermStructure(settlementDays, cal, dc) {}

Real CorrelationTermStructure::correlation(Time t, Real strike, bool extrapolate) const {
    checkRange(t, strike, extrapolate);

    // Fail if correlation is negative
    Real correlation = correlationImpl(t, strike);
    QL_REQUIRE(correlation >= -1 && correlation <= 1,
               "Correlation returned from CorrelationTermStructure must be between -1 and 1 (" << correlation << ")");

    return correlation;
}

Real CorrelationTermStructure::correlation(const Date& d, Real strike, bool extrapolate) const {
    return correlation(timeFromReference(d), strike, extrapolate);
}

Time CorrelationTermStructure::minTime() const {
    // Default implementation
    return 0.0;
}

void CorrelationTermStructure::checkRange(Time t, Real strike,  bool extrapolate) const {
    QL_REQUIRE(extrapolate || allowsExtrapolation() || t >= minTime() || close_enough(t, minTime()),
               "time (" << t << ") is before min curve time (" << minTime() << ")");

    // Now, do the usual TermStructure checks
    TermStructure::checkRange(t, extrapolate);
}

NegativeCorrelationTermStructure::NegativeCorrelationTermStructure(const Handle<CorrelationTermStructure>& c)
    : CorrelationTermStructure(c->dayCounter()), c_(c) {
    registerWith(c_);
}

Real NegativeCorrelationTermStructure::correlationImpl(Time t, Real strike) const {
    return -c_->correlation(t, strike);
}

CorrelationValue::CorrelationValue(const Handle<CorrelationTermStructure>& correlation, const Time t, const Real strike)
    : correlation_(correlation), t_(t), strike_(strike) {
    registerWith(correlation_);
}

Real CorrelationValue::value() const {
    QL_REQUIRE(!correlation_.empty(), "no source correlation term structure given");
    return correlation_->correlation(t_, strike_);
}

bool CorrelationValue::isValid() const { return !correlation_.empty(); }

void CorrelationValue::update() { notifyObservers(); }

} // namespace QuantExt
