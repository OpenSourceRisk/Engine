/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <qle/termstructures/calendarspreadfuturepricetermstructure.hpp>
#include <qle/utilities/time.hpp>

using namespace QuantLib;

namespace QuantExt {

CalendarSpreadFuturePriceTermStructure::CalendarSpreadFuturePriceTermStructure(
    const Handle<PriceTermStructure>& baseCurve,
    const QuantLib::ext::shared_ptr<FutureExpiryCalculator>& expiryCalculator, Natural calendarSpreadOffset)
    : PriceTermStructure(baseCurve->dayCounter()), baseCurve_(baseCurve), expiryCalculator_(expiryCalculator),
      calendarSpreadOffset_(calendarSpreadOffset) {
    QL_REQUIRE(calendarSpreadOffset_ > 0,
               "CalendarSpreadFuturePriceTermStructure: calendarSpreadOffset must be positive, got "
                   << calendarSpreadOffset_);
    registerWith(baseCurve_);
}

Date CalendarSpreadFuturePriceTermStructure::maxDate() const { return baseCurve_->maxDate(); }

const Date& CalendarSpreadFuturePriceTermStructure::referenceDate() const { return baseCurve_->referenceDate(); }

DayCounter CalendarSpreadFuturePriceTermStructure::dayCounter() const { return baseCurve_->dayCounter(); }

Time CalendarSpreadFuturePriceTermStructure::minTime() const { return baseCurve_->minTime(); }

const Currency& CalendarSpreadFuturePriceTermStructure::currency() const { return baseCurve_->currency(); }

std::vector<Date> CalendarSpreadFuturePriceTermStructure::pillarDates() const { return baseCurve_->pillarDates(); }

void CalendarSpreadFuturePriceTermStructure::deepUpdate() {
    baseCurve_->update();
    update();
}

const Handle<PriceTermStructure>& CalendarSpreadFuturePriceTermStructure::baseCurve() const { return baseCurve_; }

const QuantLib::ext::shared_ptr<FutureExpiryCalculator>&
CalendarSpreadFuturePriceTermStructure::expiryCalculator() const {
    return expiryCalculator_;
}

Natural CalendarSpreadFuturePriceTermStructure::calendarSpreadOffset() const { return calendarSpreadOffset_; }

Real CalendarSpreadFuturePriceTermStructure::price(const Date& d, bool extrapolate) const {
    Date nearExpiry = expiryCalculator_->nextExpiry(true, d, 0);
    Date farExpiry = expiryCalculator_->nextExpiry(true, d, calendarSpreadOffset_);
    return baseCurve_->price(nearExpiry, extrapolate) - baseCurve_->price(farExpiry, extrapolate);
}

Real CalendarSpreadFuturePriceTermStructure::price(Time t, bool extrapolate) const {
    Date d = lowerDate(t, referenceDate(), dayCounter());
    return price(d, extrapolate);
}

} // namespace QuantExt
