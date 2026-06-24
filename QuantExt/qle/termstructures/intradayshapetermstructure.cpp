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

/*! \file qle/termstructures/intradayshapetermstructure.hpp
    \brief Term structure of intraday shape factors
*/

#include <qle/termstructures/intradayshapetermstructure.hpp>
#include <qle/utilities/intradaypower.hpp>

namespace QuantExt {

namespace {

inline QuantLib::Real timeWeightedShapeFactor(const ShapeFactors& factors, int startTime, int endTime) {
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
        const int segEnd = (nextIt == factors.end()) ? QuantExt::SECONDS_PER_DAY : nextIt->first;

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

inline QuantLib::Real calcShapeFactor(const QuantLib::Date& d, int startTime, int endTime, bool isDSTHour,
                                      const ShapeFactors& factors, const ShapeFactors& dstFactors,
                                      QuantExt::IntradayPowerDSTAdjustment dayTimeSavingsAdj) {

    
    QL_REQUIRE(startTime >= 0, "startTime must be >= 0, got " << startTime);
    QL_REQUIRE(endTime > startTime, "endTime must be > startTime, got " << endTime << " <= " << startTime);
    QL_REQUIRE(endTime <= QuantExt::SECONDS_PER_DAY, "endTime out of range, got " << endTime);

    if (dayTimeSavingsAdj != QuantExt::IntradayPowerDSTAdjustment::Backward && factors.empty()) {
        return 1.0;
    }

    if (dayTimeSavingsAdj == QuantExt::IntradayPowerDSTAdjustment::Backward && dstFactors.empty() && factors.empty()) {
        return 1.0;
    }

    if (isDSTHour) {
        QL_REQUIRE(startTime >= QuantExt::TWO_AM_IN_SECONDS && endTime <= QuantExt::THREE_AM_IN_SECONDS,
                   "For DST hour on " << d << " startTime and endTime must be between 2am and 3am, got startTime "
                                      << startTime << " and endTime " << endTime);
        // Ignore DST on non DST days
        if (dayTimeSavingsAdj != QuantExt::IntradayPowerDSTAdjustment::Backward) {
            return 0.0;
        }
        if (!dstFactors.empty()) {
            QL_REQUIRE(dstFactors.begin()->first == QuantExt::TWO_AM_IN_SECONDS,
                       "DST shape factors must start at 2am, got first key " << dstFactors.begin()->first);
            return timeWeightedShapeFactor(dstFactors, startTime, endTime);
        }
        // dont have a special shape for the DST, use the normal shape
        return timeWeightedShapeFactor(factors, startTime, endTime);
    }

    auto factor = timeWeightedShapeFactor(factors, startTime, endTime);
    // not extra hour and not the missing hour on spring dst change date, return factor
    if (dayTimeSavingsAdj != QuantExt::IntradayPowerDSTAdjustment::Forward) {
        return factor;
    }
    // we are in spring dst date and 2 - 3am doesnt exists, need to adjust the factor to remove the weight of the 2-3am
    // hour
    auto dststart = std::max(QuantExt::TWO_AM_IN_SECONDS, startTime);
    auto dstend = std::min(QuantExt::THREE_AM_IN_SECONDS, endTime);
    auto dstOverlap = std::max(0, dstend - dststart);
    if (dstOverlap > 0) {
        auto effectiveTime = endTime - startTime - dstOverlap;
        if (effectiveTime <= 0) {
            return 0.0;
        }
        // adjust the factor for the overlapping hours between 2 and 3 am if there is a DST change on that day
        auto dstFactor = timeWeightedShapeFactor(factors, dststart, dstend);
        factor = (factor * (endTime - startTime) - dstFactor * dstOverlap) / effectiveTime;
    }
    return factor;
}
} // namespace

QuantLib::Real IntradayShapeTermstructure::dayFactor(const QuantLib::Date& d) const {
    auto dayTimeSavingsAdj = dayTimeSavingsAdjustment(d, daylightSavingsLocation_);
    auto it = dayFactors_.find(d);
    if (it == dayFactors_.end()) {
        const ShapeFactors& shapeFactor = hasShapeFactors(d) ? shapeFactors(d) : ShapeFactors();
        const ShapeFactors& dstShapeFactor = hasShapeFactorsDST(d) ? shapeFactorsDST(d) : ShapeFactors();
        auto factor = calcShapeFactor(d, 0, QuantExt::SECONDS_PER_DAY, false, shapeFactor, dstShapeFactor, dayTimeSavingsAdj);
        if (dayTimeSavingsAdj == QuantExt::IntradayPowerDSTAdjustment::Backward) {
            auto dstFactor =
                calcShapeFactor(d, QuantExt::TWO_AM_IN_SECONDS, QuantExt::THREE_AM_IN_SECONDS, true, shapeFactor, dstShapeFactor, dayTimeSavingsAdj);
            factor = (factor * 24.0 + dstFactor) / 25.0;
        }
        dayFactors_[d] = factor;
    }
    return dayFactors_[d];
}

QuantLib::Real IntradayShapeTermstructure::intradayShapeFactor(const QuantLib::Date& d, int startTime, int endTime,
                                                               bool isDSTHour) const {
    auto dayTimeSavingsAdj = dayTimeSavingsAdjustment(d, daylightSavingsLocation_);
    const ShapeFactors& shapeFactor = hasShapeFactors(d) ? shapeFactors(d) : ShapeFactors();
    const ShapeFactors& dstShapeFactor = hasShapeFactorsDST(d) ? shapeFactorsDST(d) : ShapeFactors();
    return calcShapeFactor(d, startTime, endTime, isDSTHour, shapeFactor, dstShapeFactor, dayTimeSavingsAdj);
}

QuantLib::Real
IntradayShapeTermstructure::loadWeightedIntradayShapeFactor(const QuantLib::Date& d,
                                                            const IntradayPowerLoadProfile& load) const {
    if (load.empty()) {
        return 0.0;
    }
    auto dayTimeSavingsAdj = dayTimeSavingsAdjustment(d, daylightSavingsLocation_);
    const ShapeFactors& shapeFactor = hasShapeFactors(d) ? shapeFactors(d) : ShapeFactors();
    const ShapeFactors& dstShapeFactor = hasShapeFactorsDST(d) ? shapeFactorsDST(d) : ShapeFactors();
    auto amount = 0.0;
    auto totalMWh = 0.0;
    for (const auto& loadFactor : load) {
        auto mwh = daylightSavingAdjustedLoadMWh(loadFactor, dayTimeSavingsAdj);
        if (mwh <= 0.0) {
            continue;
        }
        amount += mwh * calcShapeFactor(d, loadFactor.startTime, loadFactor.endTime, loadFactor.isDSTextraHour, shapeFactor, dstShapeFactor, dayTimeSavingsAdj);
        totalMWh += mwh;
    }
    return (totalMWh > 0.0) ? amount / totalMWh : 0.0;
}

} // namespace QuantExt