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

namespace QuantExt {

RebatedExercise::RebatedExercise(const Exercise& exercise, const Real rebate, const Natural rebateSettlementDays,
                                 const Calendar& rebatePaymentCalendar,
                                 const BusinessDayConvention rebatePaymentConvention)
    : Exercise(exercise), exerciseDates_(std::vector<Date>()), rebates_(std::vector<Real>(dates().size(), rebate)),
      rebateSettlementDays_(rebateSettlementDays), rebateSettlementPeriod_(boost::none),
      rebatePaymentCalendar_(rebatePaymentCalendar), rebatePaymentConvention_(rebatePaymentConvention) {}

RebatedExercise::RebatedExercise(const Exercise& exercise, const std::vector<Real>& rebates,
                                 const Natural rebateSettlementDays, const Calendar& rebatePaymentCalendar,
                                 const BusinessDayConvention rebatePaymentConvention)
    : Exercise(exercise), exerciseDates_(std::vector<Date>()), rebates_(rebates),
      rebateSettlementDays_(rebateSettlementDays), rebateSettlementPeriod_(boost::none),
      rebatePaymentCalendar_(rebatePaymentCalendar), rebatePaymentConvention_(rebatePaymentConvention) {

    QL_REQUIRE(type_ == Bermudan, "a rebate vector is allowed only for a bermudan style exercise");

    QL_REQUIRE(rebates.size() == dates().size(),
               "the number of rebates (" << rebates.size() << ") must be equal to the number of exercise dates ("
                                         << dates().size());
}

RebatedExercise::RebatedExercise(const Exercise& exercise, const std::vector<Date>& exerciseDates,
                                 const std::vector<Real>& rebates, const Period& rebateSettlementPeriod,
                                 const Calendar& rebatePaymentCalendar,
                                 const BusinessDayConvention rebatePaymentConvention)
    : Exercise(exercise), exerciseDates_(exerciseDates), rebates_(rebates), rebateSettlementDays_(0),
      rebateSettlementPeriod_(rebateSettlementPeriod), rebatePaymentCalendar_(rebatePaymentCalendar),
      rebatePaymentConvention_(rebatePaymentConvention) {
    QL_REQUIRE(exerciseDates_.size() == dates().size(),
               "then number of notification dates ("
                   << dates().size() << ") must be equal to the number of exercise dates (" << exerciseDates_.size());
    QL_REQUIRE(rebates_.size() == dates().size(),
               "the number of rebates (" << rebates_.size() << ") must be equal to the number of exercise dates ("
                                         << dates().size());
}

} // namespace QuantExt
