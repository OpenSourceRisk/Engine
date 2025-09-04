/*
 Copyright (C) 2025 Quaternion Risk Management Ltd.
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
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.*/

#include <qle/pricingengines/fdcallablebondevents.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/timegrid.hpp>

namespace QuantExt {

namespace {
std::string getDateStr(const Date& d) {
    std::ostringstream os;
    os << QuantLib::io::iso_date(d);
    return os.str();
}
} // namespace

FdCallableBondEvents::FdCallableBondEvents(const Date& today, const DayCounter& dc, const Real N0,
                                                 const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equity,
                                                 const QuantLib::ext::shared_ptr<FxIndex>& fxConversion)
    : today_(today), dc_(dc), N0_(N0), equity_(equity), fxConversion_(fxConversion) {}

Real FdCallableBondEvents::time(const Date& d) const { return dc_.yearFraction(today_, d); }

void FdCallableBondEvents::registerBondCashflow(
    const QuantLib::ext::shared_ptr<NumericLgmMultiionEngineBase::CashflowInfo>& c) {
    if (c->date() > today_) {
        registeredBondCashflows_.push_back(c);
        times_.insert(time(c->payDate));
    }
}

void FdCallableBondEvents::registerCall(const CallableBond::CallabilityData& c) {
    registeredCallData_.push_back(c);
    if (c.exerciseDate > today_) {
        times_.insert(time(c.exerciseDate));
    }
}

void FdCallableBondEvents::registerPut(const CallableBond::CallabilityData& c) {
    registeredPutData_.push_back(c);
    if (c.exerciseDate > today_) {
        times_.insert(time(c.exerciseDate));
    }
}

const std::set<Real>& FdCallableBondEvents::times() const { return times_; }

Date FdCallableBondEvents::nextExerciseDate(const Date& d,
                                            const std::vector<CallableBond::CallabilityData>& data) const {
    Date result = Date::maxDate();
    for (auto const& x : data) {
        if (x.exerciseDate > d)
            result = std::min(result, x.exerciseDate);
    }
    if (result == Date::maxDate())
        return Null<Date>();
    return result;
}

void FdCallableBondEvents::processBondCashflows() {
    lastRedemptionDate_ = Date::minDate();
    for (auto const& c : registeredBondCashflows_) {
        if (c.couponStartTime == Null<Real>())
            lastRedemptionDate_ = std::max(lastRedemptionDate_, c->payDate);
    }
    for (auto const& d : registeredBondCashflows_) {
        bool isRedemption = d.couponStartTime == Null<Real>();
        Size index = grid_.index(time(d->payDate));
        hasBondCashflow_[index] = true;
        associatedDate_[index] = d->payDate;
        if (isRedemption && d->payDate == lastRedemptionDate_)
            bondFinalRedemption_[index].push_back(d);
        else
            bondCashflow_[index].push_back(d);
    }
}

void FdCallableBondEvents::processExerciseData(const std::vector<CallableBond2::CallabilityData>& sourceData,
                                                  std::vector<bool>& targetFlags, std::vector<CallData>& targetData) {
    for (auto const& c : sourceData) {
        if (c.exerciseDate <= today_ && c.exerciseType == CallableBond::CallabilityData::ExerciseType::OnThisDate)
            continue;
        Size indexStart = grid_.index(time(std::max(c.exerciseDate, today_)));
        Size indexEnd;
        associatedDate_[indexStart] = std::max(c.exerciseDate, today_);
        if (c.exerciseType == CallableBond::CallabilityData::ExerciseType::OnThisDate) {
            indexEnd = indexStart;
        } else if (c.exerciseType == CallableBond::CallabilityData::ExerciseType::FromThisDateOn) {
            Date nextDate = nextExerciseDate(c.exerciseDate, sourceData);
            QL_REQUIRE(nextDate != Null<Date>(),
                       "FdCallableBondEvents::processExerciseData(): internal error: did not find a next exercise "
                       "date after "
                           << c.exerciseDate
                           << ", the last exercise date should not have exercise type FromThisDateOn");
            if (nextDate <= today_)
                continue;
            indexEnd = grid_.index(time(nextDate)) - 1;
        } else {
            QL_FAIL("FdCallableBondEvents: internal error, exercise type not "
                    "recognized");
        }
        for (Size i = indexStart; i <= indexEnd; ++i) {
            targetFlags[i] = true;
            targetData[i] = CallData{c.price,
                                     c.priceType,
                                     c.includeAccrual,
                                     c.isSoft,
                                     c.softTriggerRatio,
                                     c.softTriggerM,
                                     c.softTriggerN,
                                     std::function<Real(Real, Real)>()};
        }
    }
}

void FdCallableBondEvents::finalise(const TimeGrid& grid) {
    QL_REQUIRE(!finalised_, "FdCallableBondEvents: internal error, events already finalised");
    finalised_ = true;
    grid_ = grid;

    hasBondCashflow_.resize(grid.size(), false);
    hasCall_.resize(grid.size(), false);
    hasPut_.resize(grid.size(), false);

    bondCashflow_.resize(grid.size(), 0.0);
    bondFinalRedemption_.resize(grid.size(), 0.0);
    callData_.resize(grid.size());
    putData_.resize(grid.size());

    associatedDate_.resize(grid.size(), Null<Date>());

    // process data

    processBondCashflows();
    processExerciseData(registeredCallData_, hasCall_, callData_);
    processExerciseData(registeredPutData_, hasPut_, putData_);

    // checks

    Size lastRedemptionIndex = grid_.index(time(lastRedemptionDate_));
    for (Size k = lastRedemptionIndex + 1; k < grid_.size(); ++k) {
        QL_REQUIRE(!hasConversion(k) && !hasMandatoryConversion(k),
                   "FdCallableBondEvents: conversion right after last bond redemption flow not allowed");
    }
}

bool FdCallableBondEvents::hasBondCashflow(const Size i) const { return hasBondCashflow_.at(i); }
bool FdCallableBondEvents::hasCall(const Size i) const { return hasCall_.at(i); }
bool FdCallableBondEvents::hasPut(const Size i) const { return hasPut_.at(i); }

std::vector<QuantLib::ext::shared_ptr<NumericLgmMultiLegOptionEngineBase::CashflowInfo>>
FdCallableBondEvents::getBondCashflow(const Size i) const {
    return bondCashflow_.at(i);
}
std::vector<QuantLib::ext::shared_ptr<NumericLgmMultiLegOptionEngineBase::CashflowInfo>>
FdCallableBondEvents::getBondFinalRedemption(const Size i) const {
    return bondFinalRedemption_.at(i);
}

const FdCallableBondEvents::CallData& FdCallableBondEvents::getCallData(const Size i) const {
    return callData_.at(i);
}

const FdCallableBondEvents::CallData& FdCallableBondEvents::getPutData(const Size i) const {
    return putData_.at(i);
}

Date FdCallableBondEvents::getAssociatedDate(const Size i) const { return associatedDate_.at(i); }

} // namespace QuantExt
