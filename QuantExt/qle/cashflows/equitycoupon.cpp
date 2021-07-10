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
#include <ql/time/calendars/jointcalendar.hpp>

using namespace QuantLib;

namespace QuantExt {

EquityCoupon::EquityCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                           Natural fixingDays, const boost::shared_ptr<EquityIndex>& equityCurve,
                           const DayCounter& dayCounter, bool isTotalReturn, Real dividendFactor, bool notionalReset,
                           Real initialPrice, Real quantity, const Date& fixingStartDate, const Date& fixingEndDate,
                           const Date& refPeriodStart, const Date& refPeriodEnd, const Date& exCouponDate,
                           const boost::shared_ptr<FxIndex>& fxIndex, const bool initialPriceIsInTargetCcy, const bool absoluteReturn)
    : Coupon(paymentDate, nominal, startDate, endDate, refPeriodStart, refPeriodEnd, exCouponDate),
      fixingDays_(fixingDays), equityCurve_(equityCurve), dayCounter_(dayCounter), isTotalReturn_(isTotalReturn),
      dividendFactor_(dividendFactor), notionalReset_(notionalReset), initialPrice_(initialPrice),
      initialPriceIsInTargetCcy_(initialPriceIsInTargetCcy), quantity_(quantity), fixingStartDate_(fixingStartDate),
      fixingEndDate_(fixingEndDate), fxIndex_(fxIndex), isAbsoluteReturn_(absoluteReturn) {
    QL_REQUIRE(dividendFactor_ > 0.0, "Dividend factor should not be negative. It is expected to be between 0 and 1.");
    QL_REQUIRE(equityCurve_, "Equity underlying an equity swap coupon cannot be empty.");

    // set up fixing calendar as combined eq / fx calendar
    Calendar eqCalendar = NullCalendar();
    Calendar fxCalendar = NullCalendar();
    if (!equityCurve_->fixingCalendar().empty()) {
        eqCalendar = equityCurve_->fixingCalendar();
    }
    if (fxIndex_ && !fxIndex_->fixingCalendar().empty()) {
        fxCalendar = fxIndex_->fixingCalendar();
    }
    Calendar fixingCalendar = JointCalendar(eqCalendar, fxCalendar);

    // If a fixing start / end date is provided, use these
    // else adjust the start/endDate by the FixingDays - defaulted to 0
    if (fixingStartDate_ == Date())
        fixingStartDate_ =
            fixingCalendar.advance(startDate, -static_cast<Integer>(fixingDays_), Days, Preceding);

    if (fixingEndDate_ == Date())
        fixingEndDate_ =
            fixingCalendar.advance(endDate, -static_cast<Integer>(fixingDays_), Days, Preceding);

    registerWith(equityCurve_);
    registerWith(fxIndex_);
    registerWith(Settings::instance().evaluationDate());

    QL_REQUIRE(!notionalReset_ || quantity_ != Null<Real>(), "EquityCoupon: quantity required if notional resets");
    QL_REQUIRE(notionalReset_ || nominal_ != Null<Real>(),
               "EquityCoupon: notional required if notional does not reset");
}

void EquityCoupon::setPricer(const boost::shared_ptr<EquityCouponPricer>& pricer) {
    if (pricer_)
        unregisterWith(pricer_);
    pricer_ = pricer;
    if (pricer_)
        registerWith(pricer_);
    update();
}

Real EquityCoupon::nominal() const {
    if (notionalReset_) {
        Real mult = (initialPrice_ == 0) ? 1 : initialPrice();
        return mult * (initialPriceIsInTargetCcy_ ? 1.0 : fxRate()) * quantity_;
    } else {
        return nominal_;
    }
}

Real EquityCoupon::fxRate() const {
    // fxRate applied if equity underlying currency differs from leg
    return fxIndex_ ? fxIndex_->fixing(fixingStartDate_) : 1.0;
}

Real EquityCoupon::initialPrice() const {
    if (initialPrice_ != Null<Real>())
        return initialPrice_;
    else
        return equityCurve_->fixing(fixingStartDate(), false, false);
}

bool EquityCoupon::initialPriceIsInTargetCcy() const { return initialPriceIsInTargetCcy_; }

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

EquityLeg::EquityLeg(const Schedule& schedule, const boost::shared_ptr<EquityIndex>& equityCurve,
                     const boost::shared_ptr<FxIndex>& fxIndex)
    : schedule_(schedule), equityCurve_(equityCurve), fxIndex_(fxIndex), paymentLag_(0), paymentAdjustment_(Following),
      paymentCalendar_(Calendar()), isTotalReturn_(true), absoluteReturn_(false), initialPrice_(Null<Real>()),
      initialPriceIsInTargetCcy_(false), dividendFactor_(1.0), fixingDays_(0), notionalReset_(false),
      quantity_(Null<Real>()) {}

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

EquityLeg& EquityLeg::withPaymentLag(Natural paymentLag) {
    paymentLag_ = paymentLag;
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

EquityLeg& EquityLeg::withAbsoluteReturn(bool absoluteReturn) {
    absoluteReturn_ = absoluteReturn;
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

EquityLeg& EquityLeg::withInitialPriceIsInTargetCcy(bool initialPriceIsInTargetCcy) {
    initialPriceIsInTargetCcy_ = initialPriceIsInTargetCcy;
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

EquityLeg& EquityLeg::withQuantity(Real quantity) {
    quantity_ = quantity;
    return *this;
}

EquityLeg::operator Leg() const {

    Leg cashflows;
    Date startDate;
    Date endDate;
    Date paymentDate;

    Calendar calendar;
    if (!paymentCalendar_.empty()) {
        calendar = paymentCalendar_;
    } else {
        calendar = schedule_.calendar();
    }

    Size numPeriods = schedule_.size() - 1;
    
    if (valuationSchedule_.size() > 0) {
        QL_REQUIRE(valuationSchedule_.size() == schedule_.size(),
                   "mismatch in valuationSchedule (" << valuationSchedule_.size() << ") and scheduleData (" <<  schedule_.size() << ") sizes");
    }
    
    for (Size i = 0; i < numPeriods; ++i) {
        startDate = schedule_.date(i);
        endDate = schedule_.date(i + 1);
        paymentDate = calendar.advance(endDate, paymentLag_, Days, paymentAdjustment_);

        Date fixingStartDate = Date();
        Date fixingEndDate = Date();
        if (valuationSchedule_.size() > 0) {
            fixingStartDate = valuationSchedule_.date(i);
            fixingEndDate = valuationSchedule_.date(i + 1);
        }

        Real initialPrice = (i == 0) ? initialPrice_ : Null<Real>();
        bool initialPriceIsInTargetCcy = initialPrice != Null<Real>() ? initialPriceIsInTargetCcy_ : false;
        Real quantity = Null<Real>(), notional = Null<Real>();
        if (notionalReset_) {
            if (quantity_ != Null<Real>()) {
                quantity = quantity_;
                QL_REQUIRE(notionals_.empty(), "EquityLeg: notional and quantity are given at the same time");
            } else {
                QL_REQUIRE(fxIndex_ == nullptr,
                           "EquityLeg: can not compute quantity from nominal when fx conversion is required");
                QL_REQUIRE(!notionals_.empty(), "EquityLeg: can not compute qunantity, since no notional is given");
                QL_REQUIRE(initialPrice_ != Null<Real>(),
                           "EquityLeg: can not compute quantity, since no initialPrice is given");
                quantity = (initialPrice_ == 0) ? notionals_.front() : notionals_.front() / initialPrice_;
            }
        } else {
            if (!notionals_.empty()) {
                notional = detail::get(notionals_, i, 0.0);
                QL_REQUIRE(quantity_ == Null<Real>(), "EquityLeg: notional and quantity are given at the same time");
            } else {
                QL_REQUIRE(fxIndex_ == nullptr,
                           "EquityLeg: can not compute notional from quantity when fx conversion is required");
                QL_REQUIRE(quantity_ != Null<Real>(),
                           "EquityLeg: can not compute notional, since no quantity is given");
                QL_REQUIRE(initialPrice_ != Null<Real>(),
                           "EquityLeg: can not compute notional, since no intialPrice is given");
                notional = (initialPrice_ == 0) ? quantity_ : quantity_ * initialPrice_;
            }
        }

        boost::shared_ptr<EquityCoupon> cashflow(
            new EquityCoupon(paymentDate, notional, startDate, endDate, fixingDays_, equityCurve_, paymentDayCounter_,
                             isTotalReturn_, dividendFactor_, notionalReset_, initialPrice, quantity, fixingStartDate,
                             fixingEndDate, Date(), Date(), Date(), fxIndex_, initialPriceIsInTargetCcy, absoluteReturn_));

        boost::shared_ptr<EquityCouponPricer> pricer(new EquityCouponPricer);
        cashflow->setPricer(pricer);

        cashflows.push_back(cashflow);
    }
    return cashflows;
}

} // namespace QuantExt
