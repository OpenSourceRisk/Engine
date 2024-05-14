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

#include <qle/cashflows/equitymargincoupon.hpp>
#include <qle/cashflows/equitymargincouponpricer.hpp>

#include <ql/utilities/vectors.hpp>

using namespace QuantLib;

namespace QuantExt {

EquityMarginCoupon::EquityMarginCoupon(const Date& paymentDate, Real nominal, Rate rate, Real marginFactor, const Date& startDate, const Date& endDate,
                           Natural fixingDays, const QuantLib::ext::shared_ptr<EquityIndex2>& equityCurve,
                           const DayCounter& dayCounter, bool isTotalReturn, Real dividendFactor, bool notionalReset,
                           Real initialPrice, Real quantity, const Date& fixingStartDate, const Date& fixingEndDate,
                           const Date& refPeriodStart, const Date& refPeriodEnd, const Date& exCouponDate, Real multiplier,
                           const QuantLib::ext::shared_ptr<FxIndex>& fxIndex, const bool initialPriceIsInTargetCcy)
    : Coupon(paymentDate, nominal, startDate, endDate, refPeriodStart, refPeriodEnd, exCouponDate),
      fixingDays_(fixingDays), equityCurve_(equityCurve), dayCounter_(dayCounter), isTotalReturn_(isTotalReturn),
      dividendFactor_(dividendFactor), notionalReset_(notionalReset), initialPrice_(initialPrice),
      initialPriceIsInTargetCcy_(initialPriceIsInTargetCcy), quantity_(quantity), fixingStartDate_(fixingStartDate),
      fixingEndDate_(fixingEndDate), fxIndex_(fxIndex), marginFactor_(marginFactor), 
      fixedRate_(InterestRate(rate, dayCounter, Simple, Annual)), multiplier_(multiplier) {
      
    QL_REQUIRE(dividendFactor_ > 0.0, "Dividend factor should not be negative. It is expected to be between 0 and 1.");
    QL_REQUIRE(equityCurve_, "Equity underlying an equity swap coupon cannot be empty.");

    // If a fixing start / end date is provided, use these
    // else adjust the start/endDate by the FixingDays - defaulted to 0
    if (fixingStartDate_ == Date())
        fixingStartDate_ =
            equityCurve_->fixingCalendar().advance(startDate, -static_cast<Integer>(fixingDays_), Days, Preceding);

    if (fixingEndDate_ == Date())
        fixingEndDate_ =
            equityCurve_->fixingCalendar().advance(endDate, -static_cast<Integer>(fixingDays_), Days, Preceding);

    registerWith(equityCurve_);
    registerWith(fxIndex_);
    registerWith(Settings::instance().evaluationDate());

    QL_REQUIRE(!notionalReset_ || quantity_ != Null<Real>(), "EquityCoupon: quantity required if notional resets");
    QL_REQUIRE(notionalReset_ || nominal_ != Null<Real>(),
               "EquityCoupon: notional required if notional does not reset");
      
}

void EquityMarginCoupon::setPricer(const QuantLib::ext::shared_ptr<EquityMarginCouponPricer>& pricer) {
    if (pricer_)
        unregisterWith(pricer_);
    pricer_ = pricer;
    if (pricer_)
        registerWith(pricer_);
    update();
}

Real EquityMarginCoupon::nominal() const {
    if (notionalReset_) {
        Real mult = (initialPrice_ == 0) ? 1 : initialPrice();
        return mult * (initialPriceIsInTargetCcy_ ? 1.0 : fxRate()) * quantity_;
    } else {
        return nominal_;
    }
}

Real EquityMarginCoupon::fxRate() const {
    // fxRate applied if equity underlying currency differs from leg
    return fxIndex_ ? fxIndex_->fixing(fixingStartDate_) : 1.0;
}

Real EquityMarginCoupon::initialPrice() const {
    if (initialPrice_ != Null<Real>())
        return initialPrice_;
    else
        return equityCurve_->fixing(fixingStartDate(), false, false);
}

bool EquityMarginCoupon::initialPriceIsInTargetCcy() const { return initialPriceIsInTargetCcy_; }

Real EquityMarginCoupon::accruedAmount(const Date& d) const {
    if (d <= accrualStartDate_ || d > paymentDate_) {
        return 0.0;
    } else {
        Time fullPeriod = dayCounter().yearFraction(accrualStartDate_, accrualEndDate_, refPeriodStart_, refPeriodEnd_);
        Time thisPeriod =
            dayCounter().yearFraction(accrualStartDate_, std::min(d, accrualEndDate_), refPeriodStart_, refPeriodEnd_);
        return nominal() * rate() * thisPeriod / fullPeriod;
    }
}

std::vector<Date> EquityMarginCoupon::fixingDates() const {
    std::vector<Date> fixingDates;

    fixingDates.push_back(fixingStartDate_);
    fixingDates.push_back(fixingEndDate_);

    return fixingDates;
};

Real EquityMarginCoupon::amount() const {
    return rate() * nominal() * multiplier();
}

Rate EquityMarginCoupon::rate() const {
    QL_REQUIRE(pricer_, "pricer not set");
    pricer_->initialize(*this);
    return pricer_->rate();
}

EquityMarginLeg::EquityMarginLeg(const Schedule& schedule, const QuantLib::ext::shared_ptr<EquityIndex2>& equityCurve,
                     const QuantLib::ext::shared_ptr<FxIndex>& fxIndex)
    : schedule_(schedule), equityCurve_(equityCurve), fxIndex_(fxIndex) {}

EquityMarginLeg& EquityMarginLeg::withCouponRates(Rate rate,
                                            const DayCounter& dc,
                                            Compounding comp,
                                            Frequency freq) {
    couponRates_.resize(1);
    couponRates_[0] = InterestRate(rate, dc, comp, freq);
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withCouponRates(const InterestRate& i) {
    couponRates_.resize(1);
    couponRates_[0] = i;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withCouponRates(const std::vector<Rate>& rates,
                                            const DayCounter& dc,
                                            Compounding comp,
                                            Frequency freq) {
    couponRates_.resize(rates.size());
    for (Size i=0; i<rates.size(); ++i)
        couponRates_[i] = InterestRate(rates[i], dc, comp, freq);
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withCouponRates(
                            const std::vector<InterestRate>& interestRates) {
    couponRates_ = interestRates;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withInitialMarginFactor(const Real& i) {
    marginFactor_ = i;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withNotional(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withPaymentLag(Natural paymentLag) {
    paymentLag_ = paymentLag;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withPaymentCalendar(const Calendar& calendar) {
    paymentCalendar_ = calendar;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withTotalReturn(bool totalReturn) {
    isTotalReturn_ = totalReturn;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withDividendFactor(Real dividendFactor) {
    dividendFactor_ = dividendFactor;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withInitialPrice(Real initialPrice) {
    initialPrice_ = initialPrice;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withMultiplier(Real multiplier) {
    multiplier_ = multiplier;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withInitialPriceIsInTargetCcy(bool initialPriceIsInTargetCcy) {
    initialPriceIsInTargetCcy_ = initialPriceIsInTargetCcy;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withFixingDays(Natural fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withValuationSchedule(const Schedule& valuationSchedule) {
    valuationSchedule_ = valuationSchedule;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withNotionalReset(bool notionalReset) {
    notionalReset_ = notionalReset;
    return *this;
}

EquityMarginLeg& EquityMarginLeg::withQuantity(Real quantity) {
    quantity_ = quantity;
    return *this;
}

EquityMarginLeg::operator Leg() const {

    Leg cashflows;
    Date startDate;
    Date endDate;
    Date paymentDate;
    InterestRate rate = couponRates_[0];

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

        InterestRate rate;
        if ((i-1) < couponRates_.size())
            rate = couponRates_[i-1];
        else
            rate = couponRates_.back();

        Real initialPrice = (i == 0) ? initialPrice_ : Null<Real>();
        bool initialPriceIsInTargetCcy = initialPrice != Null<Real>() ? initialPriceIsInTargetCcy_ : false;
        Real quantity = Null<Real>(), notional = Null<Real>();
        if (notionalReset_) {
            if (quantity_ != Null<Real>()) {
                quantity = quantity_;
                QL_REQUIRE(notionals_.empty(), "EquityMarginLeg: notional and quantity are given at the same time");
            } else {
                QL_REQUIRE(fxIndex_ == nullptr,
                           "EquityMarginLeg: can not compute quantity from nominal when fx conversion is required");
                QL_REQUIRE(!notionals_.empty(), "EquityMarginLeg: can not compute qunantity, since no notional is given");
                quantity = notionals_.front() ;
            }
        } else {
            if (!notionals_.empty()) {
                notional = detail::get(notionals_, i, 0.0);
                QL_REQUIRE(quantity_ == Null<Real>(), "EquityMarginLeg: notional and quantity are given at the same time");
            } else {
                QL_REQUIRE(fxIndex_ == nullptr,
                           "EquityMarginLeg: can not compute notional from quantity when fx conversion is required");
                QL_REQUIRE(quantity_ != Null<Real>(),
                           "EquityMarginLeg: can not compute notional, since no quantity is given");
                notional = quantity_;
            }
        }

        QuantLib::ext::shared_ptr<EquityMarginCoupon> cashflow(
            new EquityMarginCoupon(paymentDate, notional, rate, marginFactor_, startDate, endDate, fixingDays_, equityCurve_, paymentDayCounter_,
                             isTotalReturn_, dividendFactor_, notionalReset_, initialPrice, quantity, fixingStartDate,
                             fixingEndDate, Date(), Date(), Date(), multiplier_, fxIndex_, initialPriceIsInTargetCcy));

        QuantLib::ext::shared_ptr<EquityMarginCouponPricer> pricer(new EquityMarginCouponPricer);
        cashflow->setPricer(pricer);

        cashflows.push_back(cashflow);
    }
    return cashflows;
}

} // namespace QuantExt
