/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/fallbackiborindex.hpp
    \brief wrapper class for ibor index managing the fallback rules
    \ingroup indexes
*/

#include <qle/indexes/fallbackiborindex.hpp>

#include <qle/termstructures/iborfallbackcurve.hpp>

namespace QuantExt {

FallbackIborIndex::FallbackIborIndex(const QuantLib::ext::shared_ptr<IborIndex> originalIndex,
                                     const QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex, const Real spread,
                                     const Date& switchDate, const bool useRfrCurve)
    : FallbackIborIndex(originalIndex,
                        useRfrCurve ? rfrIndex
                                    : QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(
                                          rfrIndex->clone(originalIndex->forwardingTermStructure())),
                        spread, switchDate,
                        useRfrCurve ? Handle<YieldTermStructure>(QuantLib::ext::make_shared<IborFallbackCurve>(
                                          originalIndex, rfrIndex, spread, switchDate))
                                    : originalIndex->forwardingTermStructure()) {}

FallbackIborIndex::FallbackIborIndex(const QuantLib::ext::shared_ptr<IborIndex> originalIndex,
                                     const QuantLib::ext::shared_ptr<OvernightIndex> rfrIndex, const Real spread,
                                     const Date& switchDate, const Handle<YieldTermStructure>& forwardingCurve)
    : IborIndex(originalIndex->familyName(), originalIndex->tenor(), originalIndex->fixingDays(),
                originalIndex->currency(), originalIndex->fixingCalendar(), originalIndex->businessDayConvention(),
                originalIndex->endOfMonth(), originalIndex->dayCounter(), forwardingCurve),
      originalIndex_(originalIndex), rfrIndex_(rfrIndex), spread_(spread), switchDate_(switchDate) {
    registerWith(originalIndex);
    registerWith(rfrIndex);
    registerWith(forwardingCurve);
}

void FallbackIborIndex::addFixing(const Date& fixingDate, Real fixing, bool forceOverwrite) {
    if (fixingDate < switchDate_) {
        IborIndex::addFixing(fixingDate, fixing, forceOverwrite);
    } else {
        QL_FAIL("Can not add fixing value "
                << fixing << " for fixing date " << fixingDate << " to fall back ibor index '" << name()
                << "' fixing history, since fixing date is after switch date (" << switchDate_ << ")");
    }
}

QuantLib::ext::shared_ptr<OvernightIndexedCoupon> FallbackIborIndex::onCoupon(const Date& iborFixingDate,
                                                                      const bool telescopicValueDates) const {
    QL_REQUIRE(iborFixingDate >= switchDate_, "FallbackIborIndex: onCoupon for ibor fixing date "
                                                  << iborFixingDate << " requested, which is before switch date "
                                                  << switchDate_ << " for index '" << name() << "'");
    Date valueDate = originalIndex_->valueDate(iborFixingDate);
    Date maturityDate = originalIndex_->maturityDate(valueDate);
    return QuantLib::ext::make_shared<OvernightIndexedCoupon>(maturityDate, 1.0, valueDate, maturityDate, rfrIndex_, 1.0, 0.0,
                                                      Date(), Date(), DayCounter(), telescopicValueDates, false,
                                                      2 * Days, 0, Null<Size>());
}

Real FallbackIborIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {
    Date today = Settings::instance().evaluationDate();
    if (today < switchDate_ || fixingDate < switchDate_) {
        return originalIndex_->fixing(fixingDate, forecastTodaysFixing);
    }
    if (fixingDate > today) {
        return IborIndex::forecastFixing(fixingDate);
    } else {
      if (QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(originalIndex_))
	  return rfrIndex_->fixing(fixingDate) + spread_;
      else
	  return onCoupon(fixingDate, true)->rate() + spread_;
    }
}

Rate FallbackIborIndex::pastFixing(const Date& fixingDate) const {
    Date today = Settings::instance().evaluationDate();
    if (today < switchDate_) {
        return originalIndex_->pastFixing(fixingDate);
    }
    return fixing(fixingDate);
}

QuantLib::ext::shared_ptr<IborIndex> FallbackIborIndex::clone(const Handle<YieldTermStructure>& forwarding) const {
    return QuantLib::ext::make_shared<FallbackIborIndex>(originalIndex_, rfrIndex_, spread_, switchDate_, forwarding);
}

Rate FallbackIborIndex::forecastFixing(const Date& valueDate, const Date& endDate, Time t) const {
    Date today = Settings::instance().evaluationDate();
    Handle<YieldTermStructure> curve =
        today < switchDate_ ? originalIndex_->forwardingTermStructure() : forwardingTermStructure();
    QL_REQUIRE(!curve.empty(), "FallbackIborIndex: null term structure set for " << name() << ", today=" << today
                                                                                 << ", switchDate=" << switchDate_);
    DiscountFactor disc1 = curve->discount(valueDate);
    DiscountFactor disc2 = curve->discount(endDate);
    return (disc1 / disc2 - 1.0) / t;
}

QuantLib::ext::shared_ptr<IborIndex> FallbackIborIndex::originalIndex() const { return originalIndex_; }

QuantLib::ext::shared_ptr<OvernightIndex> FallbackIborIndex::rfrIndex() const { return rfrIndex_; }

Real FallbackIborIndex::spread() const { return spread_; }

const Date& FallbackIborIndex::switchDate() const { return switchDate_; }

} // namespace QuantExt
