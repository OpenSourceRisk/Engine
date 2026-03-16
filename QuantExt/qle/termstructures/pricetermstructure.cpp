/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/termstructures/pricetermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

PriceTermStructure::PriceTermStructure(const DayCounter& dc) : TermStructure(dc) {}

PriceTermStructure::PriceTermStructure(const Date& referenceDate, const Calendar& cal, const DayCounter& dc)
    : TermStructure(referenceDate, cal, dc) {}

PriceTermStructure::PriceTermStructure(Natural settlementDays, const Calendar& cal, const DayCounter& dc)
    : TermStructure(settlementDays, cal, dc) {}

Real PriceTermStructure::price(Time t, bool extrapolate) const {
    checkRange(t, extrapolate);
    return priceImpl(t);
}

Real PriceTermStructure::price(const Date& d, bool extrapolate) const {
    return price(timeFromReference(d), extrapolate);
}

void PriceTermStructure::update() { TermStructure::update(); }

Time PriceTermStructure::minTime() const {
    // Default implementation
    return 0.0;
}

void PriceTermStructure::checkRange(Time t, bool extrapolate) const {
    QL_REQUIRE(extrapolate || allowsExtrapolation() || t >= minTime() || close_enough(t, minTime()),
               "time (" << t << ") is before min curve time (" << minTime() << ")");

    // Now, do the usual TermStructure checks
    TermStructure::checkRange(t, extrapolate);
}

DerivedPriceQuote::DerivedPriceQuote(const QuantLib::Handle<PriceTermStructure>& priceTs, const QuantLib::Date& date) : priceTs_(priceTs), date_(date) {
    registerWith(priceTs_);
}

Real DerivedPriceQuote::value() const {
    QL_REQUIRE(isValid(), "Invalid DerivedPriceQuote");
    Real result = (date_ != QuantLib::Date() ?  priceTs_->price(date_, true) :  priceTs_->price(0, true));
    return result;
}

bool DerivedPriceQuote::isValid() const { return !priceTs_.empty(); }

void DerivedPriceQuote::update() { notifyObservers(); }

} // namespace QuantExt
