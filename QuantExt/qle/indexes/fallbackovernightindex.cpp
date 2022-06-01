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

#include <qle/indexes/fallbackovernightindex.hpp>

#include <qle/termstructures/overnightfallbackcurve.hpp>

namespace QuantExt {

FallbackOvernightIndex::FallbackOvernightIndex(const boost::shared_ptr<OvernightIndex> originalIndex,
					       const boost::shared_ptr<OvernightIndex> rfrIndex, const Real spread,
					       const Date& switchDate, const bool useRfrCurve)
    : FallbackOvernightIndex(originalIndex,
                        useRfrCurve ? rfrIndex
                                    : boost::dynamic_pointer_cast<OvernightIndex>(
                                          rfrIndex->clone(originalIndex->forwardingTermStructure())),
                        spread, switchDate,
                        useRfrCurve ? Handle<YieldTermStructure>(boost::make_shared<OvernightFallbackCurve>(
                                          originalIndex, rfrIndex, spread, switchDate))
                                    : originalIndex->forwardingTermStructure()) {}

FallbackOvernightIndex::FallbackOvernightIndex(const boost::shared_ptr<OvernightIndex> originalIndex,
					       const boost::shared_ptr<OvernightIndex> rfrIndex, const Real spread,
					       const Date& switchDate, const Handle<YieldTermStructure>& forwardingCurve)
    : OvernightIndex(originalIndex->familyName(), originalIndex->fixingDays(),
		     originalIndex->currency(), originalIndex->fixingCalendar(), 
		     originalIndex->dayCounter(), forwardingCurve),
      originalIndex_(originalIndex), rfrIndex_(rfrIndex), spread_(spread), switchDate_(switchDate),
      useRfrCurve_(false) {
    registerWith(originalIndex);
    registerWith(rfrIndex);
    registerWith(forwardingCurve);
}

void FallbackOvernightIndex::addFixing(const Date& fixingDate, Real fixing, bool forceOverwrite) {
    if (fixingDate < switchDate_) {
        IborIndex::addFixing(fixingDate, fixing, forceOverwrite);
    } else {
        QL_FAIL("Can not add fixing value "
                << fixing << " for fixing date " << fixingDate << " to fall back ibor index '" << name()
                << "' fixing history, since fixing date is after switch date (" << switchDate_ << ")");
    }
}

Real FallbackOvernightIndex::fixing(const Date& fixingDate, bool forecastTodaysFixing) const {
    Date today = Settings::instance().evaluationDate();
    if (today < switchDate_ || fixingDate < switchDate_) {
        return originalIndex_->fixing(fixingDate, forecastTodaysFixing);
    }
    if (fixingDate > today) {
        return IborIndex::forecastFixing(fixingDate);
    } else {
        return rfrIndex_->fixing(fixingDate) + spread_;
    }
}

Rate FallbackOvernightIndex::pastFixing(const Date& fixingDate) const {
    Date today = Settings::instance().evaluationDate();
    if (today < switchDate_) {
        return originalIndex_->pastFixing(fixingDate);
    }
    return fixing(fixingDate);
}

boost::shared_ptr<IborIndex> FallbackOvernightIndex::clone(const Handle<YieldTermStructure>& forwarding) const {
    return boost::make_shared<FallbackOvernightIndex>(originalIndex_, rfrIndex_, spread_, switchDate_, forwarding);
}

Rate FallbackOvernightIndex::forecastFixing(const Date& valueDate, const Date& endDate, Time t) const {
    Date today = Settings::instance().evaluationDate();
    Handle<YieldTermStructure> curve =
        today < switchDate_ ? originalIndex_->forwardingTermStructure() : forwardingTermStructure();
    QL_REQUIRE(!curve.empty(), "FallbackOvernightIndex: null term structure set for " << name() << ", today=" << today
	       << ", switchDate=" << switchDate_);
    DiscountFactor disc1 = curve->discount(valueDate);
    DiscountFactor disc2 = curve->discount(endDate);
    return (disc1 / disc2 - 1.0) / t;
}

boost::shared_ptr<OvernightIndex> FallbackOvernightIndex::originalIndex() const { return originalIndex_; }

boost::shared_ptr<OvernightIndex> FallbackOvernightIndex::rfrIndex() const { return rfrIndex_; }

Real FallbackOvernightIndex::spread() const { return spread_; }

const Date& FallbackOvernightIndex::switchDate() const { return switchDate_; }

bool FallbackOvernightIndex::useRfrCurve() const { return useRfrCurve_; }

} // namespace QuantExt
