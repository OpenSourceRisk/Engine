/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/instruments/rebatedexercise.hpp>

#include <ql/currency.hpp>

namespace QuantExt {

namespace {
std::vector<std::vector<Real>> singleCurrencyRebates(const std::vector<Real>& rebates) {
    std::vector<std::vector<Real>> result(rebates.size());
    std::transform(rebates.begin(), rebates.end(), result.begin(), [](Real r) { return std::vector<Real>(1, r); });
    return result;
}
} // namespace

RebatedExercise::RebatedExercise(const Exercise& exercise, const Real rebate, const Natural rebateSettlementDays,
                                 const Calendar& rebatePaymentCalendar,
                                 const BusinessDayConvention rebatePaymentConvention)
    : RebatedExercise(exercise, exercise.dates(),
                      std::vector<std::vector<Real>>(exercise.dates().size(), std::vector<Real>(1, rebate)),
                      std::vector<Currency>(1), rebateSettlementDays * Days, rebatePaymentCalendar,
                      rebatePaymentConvention) {}

RebatedExercise::RebatedExercise(const Exercise& exercise, const Real rebate, const Period& rebateSettlementPeriod,
                                 const Calendar& rebatePaymentCalendar,
                                 const BusinessDayConvention rebatePaymentConvention)
    : RebatedExercise(exercise, exercise.dates(),
                      std::vector<std::vector<Real>>(exercise.dates().size(), std::vector<Real>(1, rebate)),
                      std::vector<Currency>(1), rebateSettlementPeriod, rebatePaymentCalendar,
                      rebatePaymentConvention) {}

RebatedExercise::RebatedExercise(const Exercise& exercise, const std::vector<Real>& rebates,
                                 const Natural rebateSettlementDays, const Calendar& rebatePaymentCalendar,
                                 const BusinessDayConvention rebatePaymentConvention)
    : RebatedExercise(exercise, exercise.dates(), singleCurrencyRebates(rebates), std::vector<Currency>(1),
                      rebateSettlementDays * Days, rebatePaymentCalendar, rebatePaymentConvention) {}

RebatedExercise::RebatedExercise(const Exercise& exercise, const std::vector<Real>& rebates,
                                 const Period& rebateSettlementPeriod, const Calendar& rebatePaymentCalendar,
                                 const BusinessDayConvention rebatePaymentConvention)
    : RebatedExercise(exercise, exercise.dates(), singleCurrencyRebates(rebates), std::vector<Currency>(1),
                      rebateSettlementPeriod, rebatePaymentCalendar, rebatePaymentConvention) {}

RebatedExercise::RebatedExercise(const Exercise& exercise, const std::vector<Date>& exerciseDates,
                                 const std::vector<std::vector<Real>>& rebates,
                                 const std::vector<Currency>& rebateCurrencies, const Period& rebateSettlementPeriod,
                                 const Calendar& rebatePaymentCalendar,
                                 const BusinessDayConvention rebatePaymentConvention)
    : Exercise(exercise), exerciseDates_(exerciseDates), rebates_(rebates), rebateCurrencies_(rebateCurrencies),
      rebateSettlementPeriod_(rebateSettlementPeriod), rebatePaymentCalendar_(rebatePaymentCalendar),
      rebatePaymentConvention_(rebatePaymentConvention) {
    QL_REQUIRE(exerciseDates_.empty() || exerciseDates_.size() == dates().size(),
               "then number of notification dates ("
                   << dates().size() << ") must be equal to the number of exercise dates (" << exerciseDates_.size()
                   << ")");
    QL_REQUIRE(rebates_.size() == dates().size(),
               "the number of rebates (" << rebates_.size() << ") must be equal to the number of exercise dates ("
                                         << dates().size());
    for (Size i = 0; i < rebates_.size(); ++i) {
        QL_REQUIRE(rebates_[i].size() == rebateCurrencies.size(), "the number of rebates at index "
                                                                      << i << " (" << rebates[i].size()
                                                                      << ") does not much size of currencies ("
                                                                      << rebateCurrencies.size() << ")");
    }
}

} // namespace QuantExt
