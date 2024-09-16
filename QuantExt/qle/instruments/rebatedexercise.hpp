/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file rebatedexercise.hpp
    \brief more flexible version of ql class
*/

#pragma once

#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/optional.hpp>

namespace QuantExt {

using namespace QuantLib;

//! Rebated exercise with exercise dates != notification dates and arbitrary period
class RebatedExercise : public QuantLib::Exercise {
public:
    //! as ql ctor
    RebatedExercise(const Exercise& exercise, const Real rebate = 0.0, const Natural rebateSettlementDays = 0,
                    const Calendar& rebatePaymentCalendar = NullCalendar(),
                    const BusinessDayConvention rebatePaymentConvention = Following);
    //! same, but takes settl period instead of days
    RebatedExercise(const Exercise& exercise, const Real rebate, const Period& rebateSettlementPeriod,
                    const Calendar& rebatePaymentCalendar, const BusinessDayConvention rebatePaymentConvention);
    //! as ql ctor
    RebatedExercise(const Exercise& exercise, const std::vector<Real>& rebates, const Natural rebateSettlementDays = 0,
                    const Calendar& rebatePaymentCalendar = NullCalendar(),
                    const BusinessDayConvention rebatePaymentConvention = Following);
    //! same, but takes settl period instead of days
    RebatedExercise(const Exercise& exercise, const std::vector<Real>& rebates, const Period& rebateSettlementPeriod,
                    const Calendar& rebatePaymentCalendar, const BusinessDayConvention rebatePaymentConvention);
    //! ctor that takes exercise dates != notification dates and a rebate settlement period
    RebatedExercise(const Exercise& exercise, const std::vector<Date>& exerciseDates, const std::vector<Real>& rebates,
                    const Period& rebateSettlementPeriod, const Calendar& rebatePaymentCalendar = NullCalendar(),
                    const BusinessDayConvention rebatePaymentConvention = Following);
    const Period& rebateSettlementPeriod() const { return rebateSettlementPeriod_; }
    const Calendar& rebatePaymentCalendar() const { return rebatePaymentCalendar_; }
    BusinessDayConvention rebatePaymentConvention() const { return rebatePaymentConvention_; }
    Real rebate(Size index) const;
    Date rebatePaymentDate(Size index) const;
    Date rebatePaymentDate(const Date& exerciseDate) const;
    const std::vector<Real>& rebates() const { return rebates_; }

private:
    std::vector<Date> exerciseDates_;
    std::vector<Real> rebates_;
    Period rebateSettlementPeriod_;
    Calendar rebatePaymentCalendar_;
    BusinessDayConvention rebatePaymentConvention_;
};

inline Real RebatedExercise::rebate(Size index) const {
    QL_REQUIRE(index < rebates_.size(),
               "rebate with index " << index << " does not exist (0..." << (rebates_.size() - 1) << ")");
    return rebates_[index];
}

inline Date RebatedExercise::rebatePaymentDate(Size index) const {
    QL_REQUIRE(type_ == European || type_ == Bermudan, "for american style exercises the rebate payment date "
                                                           << "has to be calculted in the client code");
    return rebatePaymentCalendar_.advance(exerciseDates_[index], rebateSettlementPeriod_, rebatePaymentConvention_);
}

inline Date RebatedExercise::rebatePaymentDate(const Date& exerciseDate) const {
    return rebatePaymentCalendar_.advance(exerciseDate, rebateSettlementPeriod_, rebatePaymentConvention_);
}

} // namespace QuantExt
