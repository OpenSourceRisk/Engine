/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/time/schedule.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/instruments/subperiodsswap.hpp>

using namespace QuantLib;

namespace QuantExt {

SubPeriodsSwap::SubPeriodsSwap(const Date& effectiveDate, Real nominal, const Period& swapTenor, bool isPayer,
                               const Period& fixedTenor, Rate fixedRate, const Calendar& fixedCalendar,
                               const DayCounter& fixedDayCount, BusinessDayConvention fixedConvention,
                               const Period& floatPayTenor, const boost::shared_ptr<IborIndex>& iborIndex,
                               const DayCounter& floatingDayCount, DateGeneration::Rule rule,
                               SubPeriodsCoupon::Type type)

    : Swap(2), nominal_(nominal), isPayer_(isPayer), fixedRate_(fixedRate), fixedDayCount_(fixedDayCount),
      floatIndex_(iborIndex), floatDayCount_(floatingDayCount), floatPayTenor_(floatPayTenor), type_(type) {

    Date terminationDate = effectiveDate + swapTenor;

    // Fixed leg
    Schedule fixedSchedule = MakeSchedule()
                                 .from(effectiveDate)
                                 .to(terminationDate)
                                 .withTenor(fixedTenor)
                                 .withCalendar(fixedCalendar)
                                 .withConvention(fixedConvention)
                                 .withTerminationDateConvention(fixedConvention)
                                 .withRule(rule);

    legs_[0] = FixedRateLeg(fixedSchedule)
                   .withNotionals(nominal_)
                   .withCouponRates(fixedRate_, fixedDayCount_)
                   .withPaymentAdjustment(fixedConvention);

    // Sub Periods Leg, schedule is the PAY schedule
    BusinessDayConvention floatPmtConvention = iborIndex->businessDayConvention();
    Calendar floatPmtCalendar = iborIndex->fixingCalendar();
    Schedule floatSchedule = MakeSchedule()
                                 .from(effectiveDate)
                                 .to(terminationDate)
                                 .withTenor(floatPayTenor)
                                 .withCalendar(floatPmtCalendar)
                                 .withConvention(floatPmtConvention)
                                 .withTerminationDateConvention(floatPmtConvention)
                                 .withRule(rule);

    legs_[1] = SubPeriodsLeg(floatSchedule, floatIndex_)
                   .withNotional(nominal_)
                   .withPaymentAdjustment(floatPmtConvention)
                   .withPaymentDayCounter(floatDayCount_)
                   .withPaymentCalendar(floatPmtCalendar)
                   .includeSpread(false)
                   .withType(type_);

    // legs_[0] is fixed
    payer_[0] = isPayer_ ? -1.0 : +1.0;
    payer_[1] = isPayer_ ? +1.0 : -1.0;

    // Register this instrument with its coupons
    Leg::const_iterator it;
    for (it = legs_[0].begin(); it != legs_[0].end(); ++it)
        registerWith(*it);
    for (it = legs_[1].begin(); it != legs_[1].end(); ++it)
        registerWith(*it);
}

Real SubPeriodsSwap::fairRate() const {
    static const Spread basisPoint = 1.0e-4;
    calculate();
    QL_REQUIRE(legBPS_[0] != Null<Real>(), "result not available");
    return fixedRate_ - NPV_ / (legBPS_[0] / basisPoint);
}
} // namespace QuantExt
