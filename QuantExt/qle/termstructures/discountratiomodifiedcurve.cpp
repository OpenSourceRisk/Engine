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

#include <qle/termstructures/discountratiomodifiedcurve.hpp>

using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::DayCounter;
using QuantLib::DiscountFactor;
using QuantLib::Handle;
using QuantLib::Natural;
using QuantLib::Time;
using QuantLib::YieldTermStructure;

namespace QuantExt {

DiscountRatioModifiedCurve::DiscountRatioModifiedCurve(const Handle<YieldTermStructure>& baseCurve,
                                                       const Handle<YieldTermStructure>& numCurve,
                                                       const Handle<YieldTermStructure>& denCurve)
    : baseCurve_(baseCurve), numCurve_(numCurve), denCurve_(denCurve) {

    // Cannot construct with empty curves
    check();

    // All range checks will happen in underlying curves
    enableExtrapolation(true);

    // Observe the underlying curves
    registerWith(baseCurve_);
    registerWith(numCurve_);
    registerWith(denCurve_);
}

DayCounter DiscountRatioModifiedCurve::dayCounter() const { return baseCurve_->dayCounter(); }

Calendar DiscountRatioModifiedCurve::calendar() const { return baseCurve_->calendar(); }

Natural DiscountRatioModifiedCurve::settlementDays() const { return baseCurve_->settlementDays(); }

const Date& DiscountRatioModifiedCurve::referenceDate() const { return baseCurve_->referenceDate(); }

void DiscountRatioModifiedCurve::update() {
    // Make sure that any change to underlying curves leaves them valid
    check();

    YieldTermStructure::update();
}

DiscountFactor DiscountRatioModifiedCurve::discountImpl(Time t) const {
    return baseCurve_->discount(t) * numCurve_->discount(t) / denCurve_->discount(t);
}

void DiscountRatioModifiedCurve::check() const {
    QL_REQUIRE(!baseCurve_.empty(), "DiscountRatioModifiedCurve: base curve should not be empty");
    QL_REQUIRE(!numCurve_.empty(), "DiscountRatioModifiedCurve: numerator curve should not be empty");
    QL_REQUIRE(!denCurve_.empty(), "DiscountRatioModifiedCurve: denominator curve should not be empty");
}

} // namespace QuantExt
