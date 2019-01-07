/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/equitycouponpricer.hpp>

#include <ql/utilities/vectors.hpp>

using namespace QuantLib;

namespace QuantExt {

EquityCoupon::EquityCoupon(const Date& paymentDate, Real nominal,
    const Date& startDate, const Date& endDate,
    const boost::shared_ptr<EquityIndex>& equityCurve,
    const DayCounter& dayCounter, bool isTotalReturn,
    const Date& refPeriodStart,
    const Date& refPeriodEnd, const Date& exCouponDate)
    : Coupon(paymentDate, nominal, startDate, endDate, refPeriodStart,
    refPeriodEnd, exCouponDate), equityCurve_(equityCurve),  dayCounter_(dayCounter),
    isTotalReturn_(isTotalReturn) {

    registerWith(equityCurve_);
    registerWith(Settings::instance().evaluationDate());
}

void EquityCoupon::setPricer(const boost::shared_ptr<EquityCouponPricer>& pricer) {
    //QL_REQUIRE(checkPricerImpl(pricer), "pricer given is wrong type");
    if (pricer_)
        unregisterWith(pricer_);
    pricer_ = pricer;
    if (pricer_)
        registerWith(pricer_);
    update();
}

Real EquityCoupon::accruedAmount(const Date& d) const {
    if (d <= accrualStartDate_ || d > paymentDate_) {
        return 0.0;
    }
    else {
        Time fullPeriod = dayCounter().yearFraction(accrualStartDate_,
            accrualEndDate_, refPeriodStart_, refPeriodEnd_);
        Time thisPeriod = dayCounter().yearFraction(accrualStartDate_,
            std::min(d, accrualEndDate_), refPeriodStart_, refPeriodEnd_);
        return nominal() * rate() * thisPeriod / fullPeriod;
    }
}

Rate EquityCoupon::rate() const {
    QL_REQUIRE(pricer_, "pricer not set");
    // we know it is the correct type because checkPricerImpl checks on setting
    // in general pricer_ will be a derived class, as will *this on calling
    pricer_->initialize(*this);
    return pricer_->swapletRate();
}

EquityLeg::EquityLeg(const Schedule& schedule,
                     const boost::shared_ptr<EquityIndex>& equityCurve)
    : schedule_(schedule), equityCurve_(equityCurve), paymentAdjustment_(Following), 
    paymentCalendar_(Calendar()) {}

EquityLeg& EquityLeg::withNotional(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

EquityLeg& EquityLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

EquityLeg& EquityLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

EquityLeg& EquityLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

EquityLeg& EquityLeg::withPaymentCalendar(const Calendar& calendar) {
    paymentCalendar_ = calendar;
    return *this;
}

EquityLeg& EquityLeg::withTotalReturn(bool totalReturn) {
    isTotalReturn_ = totalReturn;
    return *this;
}

EquityLeg::operator Leg() const {

    QL_REQUIRE(!notionals_.empty(), "No notional given for equity leg.");

    Leg cashflows;
    Date startDate;
    Date endDate;
    Date paymentDate;

    Calendar calendar;
    if (!paymentCalendar_.empty()) {
        calendar = paymentCalendar_;
    }
    else {
        calendar = schedule_.calendar();
    }

    Size numPeriods = schedule_.size() - 1;
    for (Size i = 0; i < numPeriods; ++i) {
        startDate = schedule_.date(i);
        endDate = schedule_.date(i + 1);
        paymentDate = calendar.adjust(endDate, paymentAdjustment_);

        boost::shared_ptr<EquityCoupon> cashflow(new EquityCoupon(
            paymentDate, detail::get(notionals_, i, notionals_.back()), startDate, endDate, 
            equityCurve_, paymentDayCounter_, isTotalReturn_));

        boost::shared_ptr<EquityCouponPricer> pricer(new EquityCouponPricer);
        cashflow->setPricer(pricer);
        
        cashflows.push_back(cashflow);
    }
    return cashflows;
}

} // namespace QuantExt
