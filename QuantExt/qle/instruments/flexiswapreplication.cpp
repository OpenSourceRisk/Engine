/*
 Copyright (C) 2026 AcadiaSoft, Inc.

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

#include <qle/instruments/flexiswapreplication.hpp>

#include <ql/cashflows/coupon.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace {

Size locateDateInReferenceSchedule(const std::set<Date>& referenceSchedule, const Date& d) {
    constexpr Size gracePeriod = 5; // calendar days
    return std::distance(referenceSchedule.begin(),
                         std::lower_bound(referenceSchedule.begin(), referenceSchedule.end(), d - gracePeriod));
}

std::pair<std::vector<Real>, std::vector<Size>> buildLevels(const std::vector<Real>& values) {
    std::vector<Real> levels;
    std::vector<Size> indices;
    if (!values.empty()) {
        Real currentLevel = values.front();
        for (Size currentIndex = 0; currentIndex < values.size(); ++currentIndex) {
            Real thisValue = currentIndex < values.size() - 1 ? values[currentIndex + 1] : -QL_MAX_REAL;
            QL_REQUIRE(thisValue < currentLevel || close_enough(values[currentIndex + 1], currentLevel),
                       "generateFlexiSwapReplication(): increasing notionals / lower bounds are not allowed: "
                           << thisValue << " > " << currentLevel);
            if (!close_enough(thisValue, currentLevel)) {
                levels.push_back(currentLevel);
                indices.push_back(currentIndex);
            }
            currentLevel = thisValue;
        }
    }
    return std::make_pair(levels, indices);
}

} // namespace

std::vector<ext::shared_ptr<MultiLegOption>>
generateFlexiSwapReplication(const Date& referenceDate, const std::vector<Leg>& legs, const std::vector<bool>& payer,
                             const std::vector<Currency>& currency,
                             const std::vector<std::vector<Real>>& lowerNotionalBounds,
                             const bool generateNotionalExchanges) {

    // check consistency of input data

    QL_REQUIRE(legs.size() == payer.size(), "generateFlexiSwapReplication(): legs ("
                                                << legs.size() << ") inconsistent with payer (" << payer.size() << ")");
    QL_REQUIRE(legs.size() == currency.size(), "generateFlexiSwapReplication(): legs ("
                                                   << legs.size() << ") inconsistent with currency (" << currency.size()
                                                   << ")");
    QL_REQUIRE(legs.size() == lowerNotionalBounds.size(),
               "generateFlexiSwapReplication(): legs (" << legs.size() << ") inconsistent with lowerNotionalBounds ("
                                                        << lowerNotionalBounds.size() << ")");
    for (Size i = 0; i < legs.size(); ++i) {
        QL_REQUIRE(legs[i].size() == lowerNotionalBounds[i].size(),
                   "generateFlexiSwapReplication(): leg #" << i << " size (" << legs[i].size()
                                                           << ") inconsistent to lower notional bounds #" << i << " ("
                                                           << lowerNotionalBounds[i].size() << ")");
    }

    // if no legs are given return empty basket

    if (legs.empty())
        return {};

    // set up reference accrual schedule based on shortest leg (assuming this has the lowest pay frequency)

    auto minLeg =
        std::min_element(legs.begin(), legs.end(), [](const auto& l, const auto& r) { return l.size() < r.size(); });

    std::set<Date> referenceSchedule;
    std::for_each(minLeg->begin(), minLeg->end(),
                  [&referenceSchedule, &referenceDate](const ext::shared_ptr<CashFlow>& c) {
                      if (auto cpn = ext::dynamic_pointer_cast<Coupon>(c); cpn->accrualStartDate() > referenceDate) {
                          referenceSchedule.insert(cpn->accrualStartDate());
                      }
                  });

    // for each leg, set up current notional and lower bond on the reference accrual schedule

    std::vector<std::vector<Real>> legNotionals(legs.size(),
                                                std::vector<Real>(referenceSchedule.size() + 1, Null<Real>()));
    std::vector<std::vector<Real>> legLowerBounds(legs.size(),
                                                  std::vector<Real>(referenceSchedule.size() + 1, Null<Real>()));

    // the min accrual start dates (include a grace period) across all legs for the reference accrual schedule

    std::vector<Date> minAccrualStartDate(referenceSchedule.begin(), referenceSchedule.end());

    for (Size i = 0; i < legs.size(); ++i) {
        for (Size j = 0; j < legs[i].size(); ++j) {
            if (auto cpn = ext::dynamic_pointer_cast<Coupon>(legs[i][j])) {
                if (cpn->date() <= referenceDate)
                    continue;
                Size index = locateDateInReferenceSchedule(referenceSchedule, cpn->accrualStartDate());
                QL_REQUIRE(legNotionals[i][index] == Null<Real>() ||
                               close_enough(cpn->nominal(), legNotionals[i][index]),
                           "generateFlexiSwapReplication(): leg #"
                               << i << " at accrual start date " << cpn->accrualStartDate() << " with notional "
                               << cpn->nominal() << " conflicts with reference notional " << legNotionals[i][index]);
                legNotionals[i][index] = cpn->nominal();
                legLowerBounds[i][index] = lowerNotionalBounds[i][j];
                minAccrualStartDate[index] = std::min(minAccrualStartDate[index], cpn->accrualStartDate());
            }
        }
    }

    // build the vector of (decreasing) notionals and lower bound levels and their last index in the ref schedule

    std::vector<Size> notionalLevelIndices, lowerBoundLevelIndices;
    std::vector<Size> thisNotionalLevelIndices, thisLowerBoundLevelIndices;
    std::vector<std::vector<Real>> notionalLevels, lowerBoundLevels;

    for (Size i = 0; i < legs.size(); ++i) {
        auto [tmpNotionalLevels, tmpNotionalIndices] = buildLevels(legNotionals[i]);
        notionalLevels.push_back(tmpNotionalLevels);
        if (i == 0)
            notionalLevelIndices = tmpNotionalIndices;
        else
            QL_REQUIRE(tmpNotionalIndices == notionalLevelIndices,
                       "generateFlexiSwapReplication(): notional level changes are inconsistent on leg #"
                           << i << " vs leg #0");

        auto [tmpLowerBoundLevels, tmpLowerBoundIndices] = buildLevels(legLowerBounds[i]);
        lowerBoundLevels.push_back(tmpLowerBoundLevels);
        if (i == 0)
            lowerBoundLevelIndices = tmpLowerBoundIndices;
        else
            QL_REQUIRE(tmpLowerBoundIndices == lowerBoundLevelIndices,
                       "generateFlexiSwapReplication(): lowerBound level changes are inconsistent on leg #"
                           << i << " vs leg #0");
    }

    return {};
}

} // namespace QuantExt
