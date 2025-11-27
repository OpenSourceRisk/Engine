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

/*! \file qle/termstructures/iborfallbackcurve.hpp
    \brief projection curve for ibor fallback indices
*/

#include <qle/termstructures/iborfallbackcurve.hpp>

#include <qle/cashflows/overnightindexedcoupon.hpp>

namespace QuantExt {

SpreadedIndexYieldCurve::SpreadedIndexYieldCurve(const QuantLib::ext::shared_ptr<IborIndex>& originalIndex,
                                                 const QuantLib::ext::shared_ptr<IborIndex>& referenceIndex,
                                                 const Real spread, const bool spreadOnReference)
    : YieldTermStructure(originalIndex->forwardingTermStructure()->dayCounter()), originalIndex_(originalIndex),
      referenceIndex_(referenceIndex), spread_(spread), spreadOnReference_(spreadOnReference) {
    registerWith(originalIndex->forwardingTermStructure());
    registerWith(referenceIndex->forwardingTermStructure());
    // Always enable extrapolation: The original and rfr index forwarding curves might have different settings
    // so we do not want to make things overly complicated here.
    enableExtrapolation();
}

const Date& SpreadedIndexYieldCurve::referenceDate() const {
    return originalIndex_->forwardingTermStructure()->referenceDate();
}

Date SpreadedIndexYieldCurve::maxDate() const { return originalIndex_->forwardingTermStructure()->maxDate(); }

Calendar SpreadedIndexYieldCurve::calendar() const { return originalIndex_->forwardingTermStructure()->calendar(); }

Natural SpreadedIndexYieldCurve::settlementDays() const {
    return originalIndex_->forwardingTermStructure()->settlementDays();
}

Real SpreadedIndexYieldCurve::discountImpl(QuantLib::Time t) const {
    Date today = Settings::instance().evaluationDate();
    Date endDate = today + (spreadOnReference_ ? originalIndex_->tenor() : referenceIndex_->tenor());
    auto dc = spreadOnReference_ ? originalIndex_->dayCounter() : referenceIndex_->dayCounter();
    Real couponTime = dc.yearFraction(today, endDate);
    Real curveTime = timeFromReference(endDate);
    //Real sp = spreadOnReference_ ? spread_ : -spread_;
    Real s = std::log(1.0 + couponTime * spread_) / curveTime;
    return referenceIndex_->forwardingTermStructure()->discount(t) * std::exp(t * (spreadOnReference_ ? -s : s));
}

Real IborFallbackCurve::discountImpl(QuantLib::Time t) const {
    Date today = Settings::instance().evaluationDate();
    if (today < switchDate_) {
        return originalIndex()->forwardingTermStructure()->discount(t);
    }
    return SpreadedIndexYieldCurve::discountImpl(t);
}

} // namespace QuantExt
