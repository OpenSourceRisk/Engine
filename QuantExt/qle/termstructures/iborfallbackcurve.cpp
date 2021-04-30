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

IborFallbackCurve::IborFallbackCurve(const boost::shared_ptr<IborIndex>& originalIndex,
                                     const boost::shared_ptr<OvernightIndex>& rfrIndex, const Real spread,
                                     const Date& switchDate)
    : YieldTermStructure(originalIndex->forwardingTermStructure()->dayCounter()), originalIndex_(originalIndex),
      rfrIndex_(rfrIndex), spread_(spread), switchDate_(switchDate) {
    registerWith(originalIndex->forwardingTermStructure());
    registerWith(rfrIndex->forwardingTermStructure());
    initLogDiscounts();
}

boost::shared_ptr<IborIndex> IborFallbackCurve::originalIndex() const { return originalIndex_; }

boost::shared_ptr<OvernightIndex> IborFallbackCurve::rfrIndex() const { return rfrIndex_; }

Real IborFallbackCurve::spread() const { return spread_; }

void IborFallbackCurve::update() { initLogDiscounts(); }

const Date& IborFallbackCurve::switchDate() const { return switchDate_; }

const Date& IborFallbackCurve::referenceDate() const {
    return originalIndex_->forwardingTermStructure()->referenceDate();
}

Date IborFallbackCurve::maxDate() const { return originalIndex_->forwardingTermStructure()->maxDate(); }

Calendar IborFallbackCurve::calendar() const { return originalIndex_->forwardingTermStructure()->calendar(); }

Natural IborFallbackCurve::settlementDays() const {
    return originalIndex_->forwardingTermStructure()->settlementDays();
}

Real IborFallbackCurve::discountImpl(QuantLib::Time t) const {
    Date today = Settings::instance().evaluationDate();
    if (today < switchDate_) {
        return originalIndex_->forwardingTermStructure()->discount(t);
    }
    auto r = std::lower_bound(
        logDiscounts_.begin(), logDiscounts_.end(), std::make_pair(t, 0.0),
        [](const std::pair<Real, Real>& x, const std::pair<Real, Real>& y) { return x.first < y.first; });
    if (r == logDiscounts_.end()) {
        extendTabulation(t);
        return discountImpl(t);
    }
    if (r == logDiscounts_.begin())
        return r->second;
    auto l = std::next(r, -1);
    Real alpha = (t - l->first) / (r->first - l->first);
    return std::exp(r->second * alpha + l->second * (1 - alpha));
}

void IborFallbackCurve::initLogDiscounts() {
    Real firstValueTime = timeFromReference(originalIndex_->valueDate(referenceDate())) + 1E-6;
    logDiscounts_ = {std::make_pair(0.0, 1.0), std::make_pair(firstValueTime, 1.0)};
    lastComputedFixingDate_ = referenceDate() - 1;
}

void IborFallbackCurve::extendTabulation(QuantLib::Time t) const {
    Size safeCounter = 0;
    constexpr Size safeCounterThreshold = 100000;
    Real t2;
    do {
        ++lastComputedFixingDate_;
        Date valueDate = originalIndex_->valueDate(lastComputedFixingDate_);
        Date maturityDate = originalIndex_->maturityDate(valueDate);
        OvernightIndexedCoupon oncpn(maturityDate, 1.0, valueDate, maturityDate, rfrIndex_, 1.0, 0.0, Date(), Date(),
                                     DayCounter(), true, false, 2 * Days, 0, Null<Size>());
        Real liborRate = oncpn.rate() + spread_;
        Real t1 = timeFromReference(valueDate);
        t2 = timeFromReference(maturityDate);
        Real spanningTime = originalIndex_->dayCounter().yearFraction(valueDate, maturityDate);
        Real disc1 = discountImpl(t1);
        Real disc2 = disc1 / (liborRate * spanningTime + 1.0);
        logDiscounts_.push_back(std::make_pair(t2, disc2));
    } while (t2 < t && !close_enough(t2, t) && ++safeCounter < safeCounterThreshold);
    QL_REQUIRE(safeCounter < safeCounterThreshold,
               "IborFallbackCurve::extendTabulation(t=" << t << "): internal error, safeCounter exceeds "
                                                        << safeCounterThreshold);
}

} // namespace QuantExt
