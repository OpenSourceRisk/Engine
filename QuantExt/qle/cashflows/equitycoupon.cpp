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

EquityCoupon::EquityCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                           Natural fixingDays, const boost::shared_ptr<EquityIndex>& equityCurve,
                           const DayCounter& dayCounter, bool isTotalReturn, Real dividendFactor, 
                           bool notionalReset, Real initialPrice, Real quantity,
                           const Date& refPeriodStart, const Date& refPeriodEnd,
                           const Date& exCouponDate)
    : Coupon(paymentDate, nominal, startDate, endDate, refPeriodStart, refPeriodEnd, exCouponDate),
      fixingDays_(fixingDays), equityCurve_(equityCurve), dayCounter_(dayCounter),
      isTotalReturn_(isTotalReturn), dividendFactor_(dividendFactor), notionalReset_(notionalReset), 
      initialPrice_(initialPrice), quantity_(quantity) {
    QL_REQUIRE(dividendFactor_ > 0.0, "Dividend factor should not be negative. It is expected to be between 0 and 1.");
    QL_REQUIRE(equityCurve_, "Equity underlying an equity swap coupon cannot be empty.");

    // If a refPeriodStart/End Date are provided, use these for the fixing dates, 
    // else adjust the start/endDate by the FixingDays - defaulted to 0
    if (refPeriodStart == Date())
        fixingStartDate_ =
            equityCurve_->fixingCalendar().advance(startDate, -static_cast<Integer>(fixingDays_), Days, Preceding);
    else
        fixingStartDate_ = refPeriodStart;

    if (refPeriodEnd == Date())
        fixingEndDate_ =
        equityCurve_->fixingCalendar().advance(endDate, -static_cast<Integer>(fixingDays_), Days, Preceding);
    else
        fixingEndDate_ = refPeriodEnd;

    registerWith(equityCurve_);
    registerWith(Settings::instance().evaluationDate());
}

void EquityCoupon::setPricer(const boost::shared_ptr<EquityCouponPricer>& pricer) {
    // QL_REQUIRE(checkPricerImpl(pricer), "pricer given is wrong type");
    if (pricer_)
        unregisterWith(pricer_);
    pricer_ = pricer;
    if (pricer_)
        registerWith(pricer_);
    update();
}

Real EquityCoupon::nominal() const {
    if (notionalReset_) 
        return initialPrice() * quantity();
    else
        return nominal_;
}

Real EquityCoupon::initialPrice() const {
    if (initialPrice_)
        return initialPrice_;
    else
        return equityCurve_->fixing(fixingStartDate(), false, false);
}

Real EquityCoupon::accruedAmount(const Date& d) const {
    if (d <= accrualStartDate_ || d > paymentDate_) {
        return 0.0;
    } else {
        Time fullPeriod = dayCounter().yearFraction(accrualStartDate_, accrualEndDate_, refPeriodStart_, refPeriodEnd_);
        Time thisPeriod =
            dayCounter().yearFraction(accrualStartDate_, std::min(d, accrualEndDate_), refPeriodStart_, refPeriodEnd_);
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

std::vector<Date> EquityCoupon::fixingDates() const {
    std::vector<Date> fixingDates;

    fixingDates.push_back(fixingStartDate_);
    fixingDates.push_back(fixingEndDate_);

    return fixingDates;
};

EquityLeg::EquityLeg(const Schedule& schedule, const boost::shared_ptr<EquityIndex>& equityCurve)
    : schedule_(schedule), equityCurve_(equityCurve), paymentAdjustment_(Following), paymentCalendar_(Calendar()),
      dividendFactor_(1.0), fixingDays_(0) {}

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

EquityLeg& EquityLeg::withDividendFactor(Real dividendFactor) {
    dividendFactor_ = dividendFactor;
    return *this;
}

EquityLeg& EquityLeg::withInitialPrice(Real initialPrice) {
    initialPrice_ = initialPrice;
    return *this;
}

EquityLeg& EquityLeg::withFixingDays(Natural fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

EquityLeg& EquityLeg::withValuationSchedule(const Schedule& valuationSchedule) {
    valuationSchedule_ = valuationSchedule;
    return *this;
}

EquityLeg& EquityLeg::withNotionalReset(bool notionalReset) {
    notionalReset_ = notionalReset;
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
    Real quantity = Real();
    if (initialPrice_ && notionalReset_)
        quantity = notionals_.front() / initialPrice_;

    if (valuationSchedule_.size() > 0){
        QL_REQUIRE(valuationSchedule_.size() == schedule_.size(), "Valuation and Payment Schedule sizes do not match");
    }

    for (Size i = 0; i < numPeriods; ++i) {
        startDate = schedule_.date(i);
        endDate = schedule_.date(i + 1); 
        paymentDate = calendar.adjust(endDate, paymentAdjustment_);

        Date refStartDate = Date();
        Date refEndDate = Date();
        if (valuationSchedule_.size() > 0) {
            refStartDate = valuationSchedule_.date(i);
            refEndDate = valuationSchedule_.date(i + 1);
        }

        Real initialPrice = (i == 0) ? initialPrice_ : Real();

        boost::shared_ptr<EquityCoupon> cashflow(
            new EquityCoupon(paymentDate, detail::get(notionals_, i, notionals_.back()), startDate, endDate,
                             fixingDays_, equityCurve_, paymentDayCounter_, isTotalReturn_, dividendFactor_, 
                             notionalReset_, initialPrice, quantity, refStartDate, refEndDate));

        boost::shared_ptr<EquityCouponPricer> pricer(new EquityCouponPricer);
        cashflow->setPricer(pricer);

        cashflows.push_back(cashflow);
    }
    return cashflows;
}

} // namespace QuantExt
