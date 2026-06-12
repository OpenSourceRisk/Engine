/*
 Copyright (C) 2026 AcadiaSoft Inc
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

/*! \file qle/termstructures/pricetermstructure.cpp
    \brief Term structure of intraday power prices
*/

#include <qle/termstructures/intradaypowerpricetermstructure.hpp>
#include <qle/utilities/time.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace QuantExt {




IntradayPowerPriceTermStructure::IntradayPowerPriceTermStructure(
    const QuantLib::Handle<PriceTermStructure>& underlying,
    const QuantLib::ext::shared_ptr<IntradayShapeTermstructure>& shape)
    : PriceTermStructure(underlying->referenceDate(), underlying->calendar(), underlying->dayCounter()),
      underlying_(underlying), shape_(shape) {
    registerWith(underlying_);
}

//! \name Prices
//@{
QuantLib::Real IntradayPowerPriceTermStructure::price(QuantLib::Time t, bool extrapolate) const {
    auto d = lowerDate(t, referenceDate(), dayCounter());
    return underlying_->price(t, extrapolate) * (shape_ == nullptr ? 1.0 : shape_->dayFactor(d));
}
QuantLib::Real IntradayPowerPriceTermStructure::price(const QuantLib::Date& d, bool extrapolate) const {
    return underlying_->price(d, extrapolate) * (shape_ == nullptr ? 1.0 : shape_->dayFactor(d));
}

QuantLib::Real timeWeightedShapeFactor(const ShapeFactors& factors, int startTime, int endTime) {
    QuantLib::Real weightedSum = 0.0;
    if (factors.empty()) {
        return 1.0;
    }
    // First active segment at startTime: first segment whose start > startTime, stepped back one.
    auto it = std::upper_bound(factors.begin(), factors.end(), startTime,
                               [](int t, const std::pair<int, QuantLib::Real>& seg) { return t < seg.first; });
    --it; // Safe because first key is required to be 0 and startTime >= 0.

    for (; it != factors.end(); ++it) {
        const int segStart = it->first;
        const QuantLib::Real segWeight = it->second;

        auto nextIt = std::next(it);
        const int segEnd = (nextIt == factors.end()) ? 24 * 3600 : nextIt->first;

        // Overlap of [startTime, endTime) with [segStart, segEnd).
        const int overlapStart = std::max(startTime, segStart);
        const int overlapEnd = std::min(endTime, segEnd);
        if (overlapEnd > overlapStart) {
            weightedSum += segWeight * static_cast<QuantLib::Real>(overlapEnd - overlapStart);
        }

        if (segEnd >= endTime) {
            break;
        }
    }
    return weightedSum / static_cast<QuantLib::Real>(endTime - startTime);
}

QuantLib::Real intradayShapeFactor(const QuantLib::Date& d, int startTime, int endTime, bool isDSTHour,
                                   const ShapeFactors& factors, const ShapeFactors& dstFactors, int dts) {
    if ((dts < 1 && factors.empty()) || (dts == 1 && dstFactors.empty() && factors.empty())) {
        return 1.0;
    }
    if (dts == -1 && startTime < 3 * 3600 && endTime > 2 * 3600) {
        QL_FAIL("Time between 2am and 3am on " << d << " is not valid due to DST change, got startTime " << startTime
                                               << " and endTime " << endTime);
    }
    if (dts == 1 && isDSTHour) {
        // The repeated 2-3am hour on a fall-back DST day.
        QL_REQUIRE(startTime >= 2 * 3600 && endTime <= 3 * 3600,
                   "For DST hour on " << d << " startTime and endTime must be between 2am and 3am, got startTime "
                                      << startTime << " and endTime " << endTime);
        if (dstFactors.empty()) {
            return timeWeightedShapeFactor(factors, startTime, endTime);
        }
        QL_REQUIRE(dstFactors.begin()->first == 2 * 3600,
                   "DST shape factors must start at 2am, got first key " << dstFactors.begin()->first);
        return timeWeightedShapeFactor(dstFactors, startTime, endTime);
    }
    QL_REQUIRE(startTime >= 0, "startTime must be >= 0, got " << startTime);
    QL_REQUIRE(endTime > startTime, "endTime must be > startTime, got " << endTime << " <= " << startTime);
    QL_REQUIRE(endTime <= 24 * 3600, "endTime out of range, got " << endTime);
    return timeWeightedShapeFactor(factors, startTime, endTime);
}



QuantLib::Real
IntradayPowerPriceTermStructure::price(const QuantLib::Date& d,
                                  const QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>& load,
                                  bool extrapolate) const {
    if (shape_ == nullptr || load == nullptr || load->loadProfile().empty()) {
        return price(d, extrapolate);
    }
    auto underlyingPrice = underlying_->price(d, extrapolate);
    auto shapeFactor = shape_->hasShapeFactors(d) ? shape_->shapeFactors(d) : ShapeFactors();
    auto dstShapeFactor = shape_->hasShapeFactorsDST(d) ? shape_->shapeFactorsDST(d) : ShapeFactors();
    auto dstAdj = shape_->dayTimeSavingsAdjustment(d);
    auto amount = 0.0;
    for(const auto& [start, end, loadFactor] : load->loadProfile()) {
        QL_REQUIRE(start >= 0, "start time in load profile must be >= 0, got " << start);
        QL_REQUIRE(end > start, "end time in load profile must be > start time, got " << end << " <= " << start);
        QL_REQUIRE(end <= 24 * 3600, "end time in load profile out of range, got " << end);
        amount += loadFactor * (end-start) / 3600. * intradayShapeFactor(d, start, end, false, shapeFactor, dstShapeFactor, dstAdj) *  underlyingPrice;
    }
    for(const auto& [start, end, loadFactor] : load->loadProfileDST()) {
        QL_REQUIRE(start >= 2 * 3600 && start < 3 * 3600, "start time in DST load profile must be between 2am and 3am, got " << start);
        QL_REQUIRE(end > start && end <= 3 * 3600, "end time in DST load profile must be between 2am and 3am and greater than start time, got " << end);
        amount += loadFactor * (end-start) / 3600. * intradayShapeFactor(d, start, end, true, shapeFactor, dstShapeFactor, dstAdj) * underlyingPrice;
    }
    return amount / load->totalMWh();
}

QuantLib::Real
IntradayPowerPriceTermStructure::price(QuantLib::Time t,
                                  const QuantLib::ext::shared_ptr<QuantExt::IntradayLoadProfile>& load,
                                  bool extrapolate) const {
    auto d = lowerDate(t, referenceDate(), dayCounter());
    return price(d, load, extrapolate);
}
//@}

void IntradayPowerPriceTermStructure::update() {
    TermStructure::update();
}


} // namespace QuantExt

