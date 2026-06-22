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

#pragma once

#include <ql/currency.hpp>
#include <ql/math/comparison.hpp>
#include <ql/quote.hpp>
#include <ql/termstructure.hpp>
#include <qle/utilities/time.hpp>
#include <qle/termstructures/intradaypowerloadtermstructure.hpp>
#include <map>
#include <utility>
#include <vector>

namespace QuantExt {

using ShapeFactors = std::vector<std::pair<int, QuantLib::Real>>;

//! Intraday Price term structure
/*! This abstract class defines the interface of concrete
    price term structures which will be derived from this one.

    \ingroup termstructures
*/

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

inline QuantLib::Real calcShapeFactor(const QuantLib::Date& d, int startTime, int endTime, bool isDSTHour,
                                   const ShapeFactors& factors, const ShapeFactors& dstFactors, int dayTimeSavingsAdj) {
    QL_REQUIRE(isDSTHour == false || dayTimeSavingsAdj == 1,
               "isDSTHour is true but there is no forward DST change on " << QuantLib::io::iso_date(d));
    QL_REQUIRE(startTime >= 0, "startTime must be >= 0, got " << startTime);
    QL_REQUIRE(endTime > startTime, "endTime must be > startTime, got " << endTime << " <= " << startTime);
    QL_REQUIRE(endTime <= 24 * 3600, "endTime out of range, got " << endTime);

    if (dayTimeSavingsAdj < 1 && !factors.empty()) {
        return 1.0;
    }

    if (dayTimeSavingsAdj == 1 && !dstFactors.empty() && !factors.empty()) {
        return 1.0;
    }

    if (isDSTHour) {
        QL_REQUIRE(startTime >= 2 * 3600 && endTime <= 3 * 3600,
                   "For DST hour on " << d << " startTime and endTime must be between 2am and 3am, got startTime "
                                      << startTime << " and endTime " << endTime);
        if (!dstFactors.empty()) {
            QL_REQUIRE(dstFactors.begin()->first == 2 * 3600,
                       "DST shape factors must start at 2am, got first key " << dstFactors.begin()->first);
            return timeWeightedShapeFactor(dstFactors, startTime, endTime);
        }
        // dont have a special shape for the DST, use the normal shape
        return timeWeightedShapeFactor(factors, startTime, endTime);
    }

    auto factor = timeWeightedShapeFactor(factors, startTime, endTime);
    // not extra hour and not the missing hour on spring dst change date, return factor
    if (dayTimeSavingsAdj >= 0) {
        return factor;
    }
    // we are in spring dst date and 2 - 3am doesnt exists, need to adjust the factor to remove the weight of the 2-3am
    // hour
    auto dststart = std::max(2 * 3600, startTime);
    auto dstend = std::min(3 * 3600, endTime);
    auto dstOverlap = std::max(0, dstend - dststart);
    if (dstOverlap > 0) {
        // adjust the factor for the overlapping hours between 2 and 3 am if there is a DST change on that day
        auto dstFactor = timeWeightedShapeFactor(factors, dststart, dstend);
        factor = (factor * (endTime - startTime) - dstFactor * dstOverlap) / (endTime - startTime - dstOverlap);
    }
    return factor;
}

class IntradayShapeTermstructure {
public:
    IntradayShapeTermstructure(const std::map<QuantLib::Date, std::map<int, QuantLib::Real>>& shapeFactors,
                               const std::map<QuantLib::Date, std::map<int, QuantLib::Real>>& shapeFactorsDST,
                               const std::string& daylightSavingsLocation = "") :
        shapeFactors_(toSortedVectors(shapeFactors)), shapeFactorsDST_(toSortedVectors(shapeFactorsDST)), daylightSavingsLocation_(daylightSavingsLocation) {}
    
    const bool hasShapeFactors(const QuantLib::Date& d) const {
        auto it = shapeFactors_.upper_bound(d);
        return it != shapeFactors_.begin();
    }

    const ShapeFactors& shapeFactors(const QuantLib::Date& d) const {
        auto it = shapeFactors_.upper_bound(d);
        QL_REQUIRE(it != shapeFactors_.begin(), "no shape factors found for date " << d << " or earlier");
        --it;
        return it->second;
    }

    const bool hasShapeFactorsDST(const QuantLib::Date& d) const {
        auto it = shapeFactorsDST_.upper_bound(d);
        return it != shapeFactorsDST_.begin();  
    }

    const ShapeFactors& shapeFactorsDST(const QuantLib::Date& d) const {
        auto it = shapeFactorsDST_.upper_bound(d);
        QL_REQUIRE(it != shapeFactorsDST_.begin(), "no DST shape factors found for date " << d << " or earlier");
        --it;
        return it->second;
    }
    
    QuantLib::Real dayFactor(const QuantLib::Date& d) const {
        auto dayTimeSavingsAdj = dayTimeSavingsAdjustment(d);
        auto it = dayFactors_.find(d);
        if (it == dayFactors_.end()) {
            auto& shapeFactor = hasShapeFactors(d) ? shapeFactors(d) : ShapeFactors();
            auto& dstShapeFactor = hasShapeFactorsDST(d) ? shapeFactorsDST(d) : ShapeFactors();
            auto factor =  calcShapeFactor(d, 0, 24 * 3600, false, shapeFactor, dstShapeFactor, dayTimeSavingsAdj);
            if (dayTimeSavingsAdj == 1) {
                auto dstFactor = calcShapeFactor(d, 2 * 3600, 3 * 3600, true, shapeFactor, dstShapeFactor, dayTimeSavingsAdj);
                factor = (factor * 24.0 + dstFactor) / 25.0;
            }
            dayFactors_[d] = factor;
        }
        return dayFactors_[d];
    }


    QuantLib::Real intradayShapeFactor(const QuantLib::Date& d, int startTime, int endTime, bool isDSTHour) const {
        auto dayTimeSavingsAdj = dayTimeSavingsAdjustment(d);
        auto& shapeFactor = hasShapeFactors(d) ? shapeFactors(d) : ShapeFactors();
        auto& dstShapeFactor = hasShapeFactorsDST(d) ? shapeFactorsDST(d) : ShapeFactors();
        return calcShapeFactor(d, startTime, endTime, isDSTHour, shapeFactor, dstShapeFactor, dayTimeSavingsAdj);
    }

    QuantLib::Real loadWeightedIntradayShapeFactor(const QuantLib::Date& d, const QuantLib::ext::shared_ptr<IntradayLoadProfile>& load) const {
        auto dayTimeSavingsAdj = dayTimeSavingsAdjustment(d);
        auto& shapeFactor = hasShapeFactors(d) ? shapeFactors(d) : ShapeFactors();
        auto& dstShapeFactor = hasShapeFactorsDST(d) ? shapeFactorsDST(d) : ShapeFactors();
        auto amount = 0.0;
        for (const auto& [start, end, loadFactor, isDst, mwh] : load->loadProfile()) {
            amount += mwh * calcShapeFactor(d, start, end, isDst, shapeFactor, dstShapeFactor, dayTimeSavingsAdj);
        }
        return (load->totalMWh() > 0.0) ? amount / load->totalMWh() : 0.0;
    }

    // returns -1, 0, +1 depending if the day is a day with a DST change and if the change is backward, no change, or forward
    int dayTimeSavingsAdjustment(const QuantLib::Date& d) const {
        return daylightSavingCorrection(daylightSavingsLocation_.empty() ? "Null" : daylightSavingsLocation_, d, d+1);
    }

    QuantLib::Real hoursPerDay(const QuantLib::Date& d) const { return 24.0 + dayTimeSavingsAdjustment(d); }

private:
    
    //! Convert the per-day map of (startTime -> factor) into a per-day sorted vector.
    //  std::map iteration is already ordered by key, so the resulting vector is sorted.
    static std::map<QuantLib::Date, ShapeFactors>
    toSortedVectors(const std::map<QuantLib::Date, std::map<int, QuantLib::Real>>& src) {
        std::map<QuantLib::Date, ShapeFactors> result;
        for (const auto& [d, inner] : src) {
            ShapeFactors v;
            v.reserve(inner.size());
            for (const auto& [start, factor] : inner)
                v.emplace_back(start, factor);
            result.emplace(d, std::move(v));
        }
        return result;
    }

    std::map<QuantLib::Date, ShapeFactors> shapeFactors_;
    std::map<QuantLib::Date, ShapeFactors> shapeFactorsDST_;
    mutable std::map<QuantLib::Date, QuantLib::Real> dayFactors_;
    std::string daylightSavingsLocation_;
};

} // namespace QuantExt