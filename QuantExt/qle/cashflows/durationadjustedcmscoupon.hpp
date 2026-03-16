/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

/*! \file durationadjustedcmscoupon.hpp
    \brief cms coupon scaled by a duration number
    \ingroup cashflows
*/

#pragma once

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {

using namespace QuantLib;

class DurationAdjustedCmsCoupon : public FloatingRateCoupon {
public:
    /* indexFixing(), rate(), etc. refer to the adjusted cms index fixing.
       The adjustment factor is defined as 1.0 if the duration is 0, otherwise it is
       sum_i 1 / (1+S)^i where the sum runs over i = 1, ... , duration */
    DurationAdjustedCmsCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                              Natural fixingDays, const QuantLib::ext::shared_ptr<SwapIndex>& index, Size duration = 0,
                              Real gearing = 1.0, Spread spread = 0.0, const Date& refPeriodStart = Date(),
                              const Date& refPeriodEnd = Date(), const DayCounter& dayCounter = DayCounter(),
                              bool isInArrears = false, const Date& exCouponDate = Date());
    const QuantLib::ext::shared_ptr<SwapIndex>& swapIndex() const { return swapIndex_; }
    Size duration() const;
    Real durationAdjustment() const;
    Rate indexFixing() const override;
    void accept(AcyclicVisitor&) override;

private:
    QuantLib::ext::shared_ptr<SwapIndex> swapIndex_;
    Size duration_;
};

class DurationAdjustedCmsLeg {
public:
    DurationAdjustedCmsLeg(const Schedule& schedule, const QuantLib::ext::shared_ptr<SwapIndex>& swapIndex,
                           const Size duration);
    DurationAdjustedCmsLeg& withNotionals(Real notional);
    DurationAdjustedCmsLeg& withNotionals(const std::vector<Real>& notionals);
    DurationAdjustedCmsLeg& withPaymentDayCounter(const DayCounter&);
    DurationAdjustedCmsLeg& withPaymentAdjustment(BusinessDayConvention);
    DurationAdjustedCmsLeg& withPaymentLag(Natural lag);
    DurationAdjustedCmsLeg& withPaymentCalendar(const Calendar&);
    DurationAdjustedCmsLeg& withFixingDays(Natural fixingDays);
    DurationAdjustedCmsLeg& withFixingDays(const std::vector<Natural>& fixingDays);
    DurationAdjustedCmsLeg& withGearings(Real gearing);
    DurationAdjustedCmsLeg& withGearings(const std::vector<Real>& gearings);
    DurationAdjustedCmsLeg& withSpreads(Spread spread);
    DurationAdjustedCmsLeg& withSpreads(const std::vector<Spread>& spreads);
    DurationAdjustedCmsLeg& withCaps(Rate cap);
    DurationAdjustedCmsLeg& withCaps(const std::vector<Rate>& caps);
    DurationAdjustedCmsLeg& withFloors(Rate floor);
    DurationAdjustedCmsLeg& withFloors(const std::vector<Rate>& floors);
    DurationAdjustedCmsLeg& inArrears(bool flag = true);
    DurationAdjustedCmsLeg& withZeroPayments(bool flag = true);
    DurationAdjustedCmsLeg& withExCouponPeriod(const Period&, const Calendar&, BusinessDayConvention,
                                               bool endOfMonth = false);
    DurationAdjustedCmsLeg& withDuration(Size duration);

    operator Leg() const;

private:
    Schedule schedule_;
    QuantLib::ext::shared_ptr<SwapIndex> swapIndex_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    Natural paymentLag_;
    Calendar paymentCalendar_;
    BusinessDayConvention paymentAdjustment_;
    std::vector<Natural> fixingDays_;
    std::vector<Real> gearings_;
    std::vector<Spread> spreads_;
    std::vector<Rate> caps_, floors_;
    bool inArrears_, zeroPayments_;
    Period exCouponPeriod_;
    Calendar exCouponCalendar_;
    BusinessDayConvention exCouponAdjustment_;
    bool exCouponEndOfMonth_;
    Size duration_;
};

} // namespace QuantExt
