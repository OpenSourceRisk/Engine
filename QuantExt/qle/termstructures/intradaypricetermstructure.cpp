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

#include <qle/termstructures/intradaypricetermstructure.hpp>
#include <qle/utilities/time.hpp>

namespace QuantExt {

IntradayPriceTermStructure::IntradayPriceTermStructure(
    const QuantLib::Handle<PriceTermStructure>& underlying,
    const QuantLib::ext::shared_ptr<IntradayShapeTermstructure>& shape)
    : PriceTermStructure(underlying->referenceDate(), underlying->calendar(), underlying->dayCounter()),
      underlying_(underlying), shape_(shape) {
    registerWith(underlying_);
}

//! \name Prices
//@{
QuantLib::Real IntradayPriceTermStructure::price(QuantLib::Time t, bool extrapolate) const {
    auto d = lowerDate(t, referenceDate(), dayCounter());
    return underlying_->price(t, extrapolate) * (shape_ == nullptr ? 1.0 : shape_->dayFactor(d));
}
QuantLib::Real IntradayPriceTermStructure::price(const QuantLib::Date& d, bool extrapolate) const {
    return underlying_->price(d, extrapolate) * (shape_ == nullptr ? 1.0 : shape_->dayFactor(d));
}

QuantLib::Real timeWeightedPriceFactor(const std::map<int, QuantLib::Real>& factors, const int& startTime,
                                       const int& endTime) {
    QuantLib::Real weightedSum = 0.0;
    if (factors.empty()) {
        return 1.0;
    }
    // First active segment at startTime.
    auto it = factors.upper_bound(startTime);
    --it; // Safe because first key is required to be 0 and startTime >= 0.

    while (it != factors.end()) {
        const int segStart = it->first;
        const QuantLib::Real segWeight = it->second;
        
        auto nextIt = std::next(it);
        const int segEnd = (nextIt == factors.end()) ? 24 * 3600 : nextIt->first;

        // Overlap of [startTime, endTime) with [segStart, segEnd).
        const int overlapStart = std::max(startTime, segStart);
        const int overlapEnd = std::min(endTime, segEnd);
        // std::cout << "Segment starting at " << segStart << " with weight " << segWeight << " overlaps with [" << startTime << ", " << endTime << ") in the range [" << overlapStart << ", " << overlapEnd << ")" << std::endl;
        if (overlapEnd > overlapStart) {
            weightedSum += segWeight * static_cast<QuantLib::Real>(overlapEnd - overlapStart);
        }

        if (segEnd >= endTime) {
            break;
        }

        it = nextIt;
    }
    //std::cout << "Total weighted sum: " << weightedSum << " for time range [" << startTime << ", " << endTime << ")" << std::endl;
    return weightedSum / static_cast<QuantLib::Real>(endTime - startTime);
}

//! Average price for 1MW for the period between startTime and endTime on date d,
// where startTime and endTime are given in seconds from the start of the day (e.g. 3600 for 1am, 7200 for 2am, etc.)
// as price in $/MWh
QuantLib::Real IntradayPriceTermStructure::price(const QuantLib::Date& d, const int& startTime,
                                                 const int& endTime, const bool isDSTHour,
                                                 bool extrapolate) const {
    if(shape_ == nullptr || !shape_->hasShapeFactors(d)) {
        return underlying_->price(d, extrapolate);
    }
    auto factors = shape_->shapeFactors(d);
    if (factors.empty()) {
        // If there are no shape factors for the given date, we assume a flat shape with factor 1.0 and return the underlying price
        return underlying_->price(d, extrapolate);
    }
    const auto dts = shape_->dayTimeSavingsAdjustment(d);

    if (dts == -1 && startTime < 3 * 3600 && endTime > 2 * 3600) {
        QL_FAIL("Time between 2am and 3am on " << d << " is not valid due to DST change, got startTime " << startTime
                    << " and endTime " << endTime);
    } 

    if (dts == 1 && startTime < 3 * 3600 && endTime > 2 * 3600 && isDSTHour) {
        QL_FAIL("Time between 2am and 3am on " << d << " is not valid due to DST change, got startTime " << startTime
                    << " and endTime " << endTime);
    }
    if (dts == 1 && isDSTHour){
        QL_REQUIRE(startTime >= 2 * 3600 && endTime <= 3 * 3600, "For DST hour on " << d << " startTime and endTime must be between 2am and 3am, got startTime " << startTime
                    << " and endTime " << endTime);
        const auto dstFactors = shape_->shapeFactorsDST(d);
        QL_REQUIRE(!dstFactors.empty(), "no DST shape factors found for date " << d);
        QL_REQUIRE(dstFactors.begin()->first == 2 * 3600,
                   "DST shape factors must start at 2am, got first key " << dstFactors.begin()->first);
        
         return underlying_->price(d, extrapolate) * timeWeightedPriceFactor(dstFactors, startTime, endTime);
    } 

    QL_REQUIRE(startTime >= 0, "startTime must be >= 0, got " << startTime);
    QL_REQUIRE(endTime > startTime, "endTime must be > startTime, got " << endTime << " <= " << startTime);
    QL_REQUIRE(endTime <= 24 * 3600, "endTime out of range, got " << endTime);

    QL_REQUIRE(!factors.empty(), "no shape factors found for date " << d);
    QL_REQUIRE(factors.begin()->first == 0,
               "shape factors must start at 0 seconds, got first key " << factors.begin()->first);
    //std::cout << "Calculating price for date " << d << " and time range [" << startTime << ", " << endTime << ") with shape factors: ";
   // std::cout << "underlying price: " << underlying_->price(d, extrapolate) << std::endl;
    return underlying_->price(d, extrapolate) * timeWeightedPriceFactor(factors, startTime, endTime);
}

QuantLib::Real
IntradayPriceTermStructure::price(const QuantLib::Date& d,
                                  const QuantLib::ext::shared_ptr<QuantExt::IntradayLoadingTermstructure>& load,
                                  bool extrapolate) const {
    if (load == nullptr) {
        return price(d, extrapolate);
    }        
    auto loadProfile = load->loadProfile(d);
    if (loadProfile == nullptr || loadProfile->loadProfile().empty()) {
        return price(d, extrapolate);
    }
    auto price = 0.0;
    for(const auto& [startend, loadFactor] : loadProfile->loadProfile()) {
        const auto& [start, end] = startend;
        QL_REQUIRE(start >= 0, "start time in load profile must be >= 0, got " << start);
        QL_REQUIRE(end > start, "end time in load profile must be > start time, got " << end << " <= " << start);
        QL_REQUIRE(end <= 24 * 3600, "end time in load profile out of range, got " << end);
        price += loadFactor * (end-start) / 3600. * this->price(d, start, end, false, extrapolate);
    }
    for(const auto& [startend, loadFactor] : loadProfile->loadProfileDST()) {
        const auto& [start, end] = startend;
        QL_REQUIRE(start >= 2 * 3600 && start < 3 * 3600, "start time in DST load profile must be between 2am and 3am, got " << start);
        QL_REQUIRE(end > start && end <= 3 * 3600, "end time in DST load profile must be between 2am and 3am and greater than start time, got " << end);
        price += loadFactor * (end-start) / 3600. * this->price(d, start, end, true, extrapolate);
    }
    return price / loadProfile->totalMWh();
}
//@}

void IntradayPriceTermStructure::update() {
    TermStructure::update();
}


} // namespace QuantExt

