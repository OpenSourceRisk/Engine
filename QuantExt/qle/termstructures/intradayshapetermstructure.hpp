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
class IntradayShapeTermstructure {

public:
    //! \name Constructors
    //@{
    // Use map of maps to ensure that no duplicates and shapefactors are sorted
    IntradayShapeTermstructure(const std::map<QuantLib::Date, std::map<int, QuantLib::Real>>& shapeFactors,
                               const std::map<QuantLib::Date, std::map<int, QuantLib::Real>>& shapeFactorsDST,
                               const std::string& daylightSavingsLocation = "") :
        shapeFactors_(toSortedVectors(shapeFactors)), shapeFactorsDST_(toSortedVectors(shapeFactorsDST)), daylightSavingsLocation_(daylightSavingsLocation) {}
    //@}

    virtual ~IntradayShapeTermstructure() = default;

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
        if (!hasShapeFactors(d)) {
            return 1.0;
        }
        const auto& factors = shapeFactors(d);
        if (factors.empty()) {
            return 1.0;
        }
        auto it = dayFactors_.find(d);
        if (it == dayFactors_.end()) {
            QuantLib::Real sum = 0.0;
            auto prevFactor = 0.0;
            auto prevStart = 0;
            auto dayTimeSavingsAdj = dayTimeSavingsAdjustment(d);
            for (const auto& [start, factor] : factors) {
                QL_REQUIRE(start < 86400 && start >= 0,  "start time " << start << " is out of range");
                sum += (start - prevStart) * prevFactor;
                if (dayTimeSavingsAdj < 0 && prevStart >= 2 * 3600 && prevStart < 3 * 3600) {
                    // adjust the factor for the hour between 2 and 3 am if there is a DST change on that day
                    sum -= (std::min(start, 3 * 3600) - std::max(prevStart, 2 * 3600)) * factor;
                }
                prevStart = start;
                prevFactor = factor;
            }
            sum += (86400 - prevStart) * prevFactor;
            if (dayTimeSavingsAdj > 0) {
                // adjust the factor for the hour between 2 and 3 am if there is a DST change on that day
                const auto& dstFactors = shapeFactorsDST(d);
                // Add DST factors if they exists
                auto prevDSTStart = 2 * 3600;
                auto prevDSTFactor = 0;
                for (const auto& [dstStart, dstFactor] : dstFactors) {
                    QL_REQUIRE(dstStart < 3*3600 && dstStart >= 2*3600,
                               "DST start time " << dstStart << " is out of range");
                    sum += (dstStart - prevDSTStart) * prevDSTFactor;
                    prevDSTStart = dstStart;
                    prevDSTFactor = dstFactor;
                }
                sum += (3 * 3600 - prevDSTStart) * prevDSTFactor;
            }

            dayFactors_[d] = sum / (86400 + dayTimeSavingsAdjustment(d) * 3600);
        }
        return dayFactors_[d];
    }
    
    // returns -1, 0, +1 depending if the day is a day with a DST change and if the change is backward, no change, or forward
    virtual int dayTimeSavingsAdjustment(const QuantLib::Date& d) const {
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