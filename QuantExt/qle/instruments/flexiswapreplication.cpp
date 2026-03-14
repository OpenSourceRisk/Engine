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

#include <qle/cashflows/scaledcoupon.hpp>
#include <qle/instruments/flexiswapreplication.hpp>
#include <qle/instruments/rebatedexercise.hpp>

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace {

constexpr double tolerance = 2E-2;
constexpr Size gracePeriod = 5; // calendar days

bool lessThanTol(double a, double b) { return a + tolerance < b; }

bool equalTol(double a, double b) { return std::abs(a - b) < tolerance; }

Size locateDateInReferenceSchedule(const std::set<Date>& referenceSchedule, const Date& d) {
    return std::distance(referenceSchedule.begin(),
                         std::lower_bound(referenceSchedule.begin(), referenceSchedule.end(), d + gracePeriod)) -
           1;
}

std::pair<std::vector<Real>, std::vector<Size>> buildLevels(const std::vector<Real>& values, const std::string& label) {
    std::vector<Real> levels;
    std::vector<Size> indices;
    if (!values.empty()) {
        Real previousValue = QL_MAX_REAL;
        for (Size currentIndex = 0; currentIndex < values.size(); ++currentIndex) {
            QL_REQUIRE(lessThanTol(values[currentIndex], previousValue) ||
                           equalTol(values[currentIndex], previousValue),
                       "generateFlexiSwapReplication(): increasing notionals / lower bounds are not allowed: "
                           << values[currentIndex] << " > " << previousValue << " for " << label << " at index "
                           << currentIndex);
            if (!equalTol(values[currentIndex], previousValue)) {
                levels.push_back(values[currentIndex]);
                indices.push_back(currentIndex);
            }
            previousValue = values[currentIndex];
        }
    }
    return std::make_pair(levels, indices);
}

Leg filterLeg(const Leg& l, Size start, Size end, const std::vector<Size>& refIndices) {
    Leg result;
    for (Size i = 0; i < l.size(); ++i) {
        if (refIndices[i] >= start and refIndices[i] < end)
            result.push_back(l[i]);
    }
    return result;
}

void rescaleLeg(Leg& l, Real amount) {
    std::for_each(l.begin(), l.end(), [amount](ext::shared_ptr<CashFlow>& c) {
        if (auto cpn = ext::dynamic_pointer_cast<Coupon>(c))
            c = ext::make_shared<ScaledCoupon>(amount / cpn->nominal(), cpn);
    });
}

void unregisterWithCouponPricers(Leg& l) {
    std::for_each(l.begin(), l.end(), [](ext::shared_ptr<CashFlow>& c) {
        if (auto cpn = ext::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(c)) {
            cpn->unregisterWith(cpn->pricer());
        }
    });
}

} // namespace

std::vector<ext::shared_ptr<MultiLegOption>>
generateFlexiSwapReplication(const Date& referenceDate, const std::vector<Leg>& legs, const std::vector<bool>& payer,
                             const std::vector<Currency>& currency,
                             const std::vector<std::vector<Real>>& lowerNotionalBounds, const bool optionLong,
                             const bool generateNotionalExchangeOnExercise, const bool generateFinalNotionalExchange) {

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

    /* a) for each leg, set up:
       - current notional and lower bond on the reference accrual schedule
       - indices of leg coupons in reference schedule
       b) in addition determine:
       - the min accrual start dates (include a grace period) across all legs for the reference accrual schedule */

    std::vector<std::vector<Real>> legNotionals(legs.size(), std::vector<Real>(referenceSchedule.size(), Null<Real>()));
    std::vector<std::vector<Real>> legLowerBounds(legs.size(),
                                                  std::vector<Real>(referenceSchedule.size(), Null<Real>()));
    std::vector<std::vector<Size>> legCouponRefScheduleIndices(legs.size());
    std::vector<Date> minAccrualStartDate(referenceSchedule.begin(), referenceSchedule.end());

    for (Size i = 0; i < legs.size(); ++i) {
        legCouponRefScheduleIndices[i].resize(legs[i].size());
        for (Size j = 0; j < legs[i].size(); ++j) {
            if (auto cpn = ext::dynamic_pointer_cast<Coupon>(legs[i][j])) {
                if (cpn->accrualStartDate() <= referenceDate)
                    continue;
                Size index = locateDateInReferenceSchedule(referenceSchedule, cpn->accrualStartDate());
                QL_REQUIRE(legNotionals[i][index] == Null<Real>() || equalTol(cpn->nominal(), legNotionals[i][index]),
                           "generateFlexiSwapReplication(): leg #"
                               << i << " at accrual start date " << cpn->accrualStartDate() << " with notional "
                               << cpn->nominal() << " conflicts with reference notional " << legNotionals[i][index]);
                legNotionals[i][index] = cpn->nominal();
                legLowerBounds[i][index] = lowerNotionalBounds[i][j];
                legCouponRefScheduleIndices[i][j] = index;
                minAccrualStartDate[index] = std::min(minAccrualStartDate[index], cpn->accrualStartDate());
            }
        }
    }

    // build the vector of (decreasing) notionals and lower bound levels and their first index in the ref schedule

    std::vector<Size> notionalLevelIndices, lowerBoundLevelIndices;
    std::vector<std::vector<Real>> notionalLevels, lowerBoundLevels;

    std::vector<Size> thisNotionalLevelIndices, thisLowerBoundLevelIndices;
    for (Size i = 0; i < legs.size(); ++i) {
        auto [tmpNotionalLevels, tmpNotionalIndices] =
            buildLevels(legNotionals[i], "notionals leg #" + std::to_string(i + 1));
        notionalLevels.push_back(tmpNotionalLevels);
        if (i == 0)
            notionalLevelIndices = tmpNotionalIndices;
        else
            QL_REQUIRE(tmpNotionalIndices == notionalLevelIndices,
                       "generateFlexiSwapReplication(): notional level changes are inconsistent on leg #"
                           << i << " vs leg #0");

        auto [tmpLowerBoundLevels, tmpLowerBoundIndices] =
            buildLevels(legLowerBounds[i], "lower bounds leg #" + std::to_string(i + 1));
        lowerBoundLevels.push_back(tmpLowerBoundLevels);
        if (i == 0)
            lowerBoundLevelIndices = tmpLowerBoundIndices;
        else
            QL_REQUIRE(tmpLowerBoundIndices == lowerBoundLevelIndices,
                       "generateFlexiSwapReplication(): lowerBound level changes are inconsistent on leg #"
                           << i << " vs leg #0");
    }

    notionalLevelIndices.push_back(referenceSchedule.size());
    lowerBoundLevelIndices.push_back(referenceSchedule.size());

    for (Size i = 0; i < legs.size(); ++i) {
        notionalLevels[i].push_back(0.0);
        lowerBoundLevels[i].push_back(0.0);
    }

    // build the replication data per leg

    struct ReplicationData {
        Size start, end;
        Real amount;
    };

    std::vector<std::vector<ReplicationData>> replicationData(legs.size());

    for (Size legNo = 0; legNo < legs.size(); ++legNo) {

        Size n0 = 0, l0 = 0;

        Real workingNotional = notionalLevels[legNo][0];

        do {

            Real nextNotional = notionalLevels[legNo][n0 + 1];
            Real currentLowerBound = lowerBoundLevels[legNo][l0];

            if (lessThanTol(currentLowerBound, workingNotional)) {
                Real amount = 0.0;
                Size start = lowerBoundLevelIndices[l0];
                Size end = notionalLevelIndices[n0 + 1];
                if (lessThanTol(currentLowerBound, nextNotional)) {
                    amount = workingNotional - nextNotional;
                    workingNotional = nextNotional;
                    ++n0;
                } else {
                    amount = workingNotional - currentLowerBound;
                    workingNotional = currentLowerBound;
                    ++l0;
                }
                replicationData[legNo].push_back({start, end, amount});
            } else {
                ++l0;
            }

            while (notionalLevelIndices[n0] < lowerBoundLevelIndices[l0])
                ++n0;

            workingNotional = std::min(workingNotional, notionalLevels[legNo][n0]);

        } while (n0 < notionalLevelIndices.size() - 1);
    }

    // check the consistency of repcliation data across legs (same size, same start, end indices)

    for (Size legNo = 1; legNo < legs.size(); ++legNo) {

        QL_REQUIRE(replicationData[0].size() == replicationData[legNo].size(),
                   "generateFlexiSwapReplication(): replication data for leg #1 (size "
                       << replicationData[0].size() << ") inconsistent with leg #" << (legNo + 1) << " (size "
                       << replicationData[legNo].size() << ")");

        for (Size i = 0; i < replicationData[0].size(); ++i) {
            QL_REQUIRE(replicationData[0][i].start == replicationData[legNo][i].start &&
                           replicationData[0][i].end == replicationData[legNo][i].end,
                       "generateFlexiSwapReplication(): replication data entry at index #"
                           << i << " is inconsistent for leg #1 (" << replicationData[0][i].start << ","
                           << replicationData[0][i].end << "), leg #" << (legNo + 1) << " ("
                           << replicationData[legNo][i].start << "," << replicationData[legNo][i].end << ")");
        }
    }

    // build the replication basket of multileg option instruments

    std::vector<bool> effectivePayer(payer.size());
    std::transform(payer.begin(), payer.end(), effectivePayer.begin(),
                   [optionLong](bool b) { return optionLong ? !b : b; });

    std::vector<ext::shared_ptr<MultiLegOption>> basket;

    for (Size i = 0; i < replicationData[0].size(); ++i) {

        std::vector<Leg> tmpLegs;
        std::vector<Real> rebates;

        for (Size legNo = 0; legNo < legs.size(); ++legNo) {

            auto tmp = filterLeg(legs[legNo], replicationData[legNo][i].start, replicationData[legNo][i].end,
                                 legCouponRefScheduleIndices[legNo]);

            unregisterWithCouponPricers(tmp);
            rescaleLeg(tmp, replicationData[legNo][i].amount);

            if (generateFinalNotionalExchange) {
                tmp.push_back(ext::make_shared<SimpleCashFlow>(replicationData[legNo][i].amount, tmp.back()->date()));
            }

            tmpLegs.push_back(tmp);

            if (generateNotionalExchangeOnExercise) {
                rebates.push_back((effectivePayer[legNo] ? 1.0 : -1.0) * replicationData[legNo][i].amount);
            }
        }

        std::vector<Date> exerciseDates;
        for (Size j = replicationData[0][i].start; j < replicationData[0][i].end; ++j) {
            exerciseDates.push_back(minAccrualStartDate[j]);
        }

        ext::shared_ptr<Exercise> exercise = ext::make_shared<BermudanExercise>(exerciseDates);
        if (generateNotionalExchangeOnExercise) {
            exercise = ext::make_shared<QuantExt::RebatedExercise>(
                *exercise, exercise->dates(), std::vector<std::vector<Real>>(exercise->dates().size(), rebates),
                currency, 0 * Days);
        }

        basket.push_back(ext::make_shared<MultiLegOption>(tmpLegs, effectivePayer, currency, exercise));
    }

    // return the basket

    return basket;
}

} // namespace QuantExt
