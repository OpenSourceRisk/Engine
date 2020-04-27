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
                           const DayCounter& dayCounter, bool isTotalReturn, Real dividendFactor, bool notionalReset,
                           Real initialPrice, Real quantity, const Date& fixingStartDate, const Date& fixingEndDate,
                           const Date& refPeriodStart, const Date& refPeriodEnd, const Date& exCouponDate,
                           const boost::shared_ptr<FxIndex>& fxIndex)
    : Coupon(paymentDate, nominal, startDate, endDate, refPeriodStart, refPeriodEnd, exCouponDate),
      fixingDays_(fixingDays), equityCurve_(equityCurve), dayCounter_(dayCounter), isTotalReturn_(isTotalReturn),
      dividendFactor_(dividendFactor), notionalReset_(notionalReset), initialPrice_(initialPrice), quantity_(quantity),
      fixingStartDate_(fixingStartDate), fixingEndDate_(fixingEndDate), fxIndex_(fxIndex) {
    QL_REQUIRE(dividendFactor_ > 0.0, "Dividend factor should not be negative. It is expected to be between 0 and 1.");
    QL_REQUIRE(equityCurve_, "Equity underlying an equity swap coupon cannot be empty.");

    // If a refPeriodStart/End Date are provided, use these for the fixing dates, 
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

    QL_REQUIRE(!notionalReset_ || quantity_ != Null<Real>(), "Resetting EquityCoupon requires quantity");
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
    if (notionalReset_) {
        // fxRate applied if equity underlying currency differs from leg
        Real fxRate = fxIndex_ ? fxIndex_->fixing(fixingStartDate_) : 1.0;
        return initialPrice() * fxRate * quantity();
    } else {
        return nominal_;
    }
}

Real EquityCoupon::initialPrice() const {
    if (initialPrice_ != Null<Real>())
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

EquityLeg::EquityLeg(const Schedule& schedule, const boost::shared_ptr<EquityIndex>& equityCurve,
                     const boost::shared_ptr<FxIndex>& fxIndex)
    : schedule_(schedule), equityCurve_(equityCurve), fxIndex_(fxIndex), paymentAdjustment_(Following),
      paymentCalendar_(Calendar()), isTotalReturn_(true), initialPrice_(Null<Real>()), dividendFactor_(1.0),
      fixingDays_(0), notionalReset_(false), quantity_(Null<Real>()) {}

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
    }
    else {
        calendar = schedule_.calendar();
    }

    Size numPeriods = schedule_.size() - 1;
    Real quantity = Null<Real>();

    // populate fixing start, initial price and fx rate for the cases where they are required below
    Date fixingStartDate;
    Real initialPrice = Null<Real>(), fxRate = Null<Real>();
    if ((quantity_ != Null<Real>() && !notionalReset_) || (!notionals_.empty() && notionalReset_)) {
        fixingStartDate = valuationSchedule_.size() > 0
                              ? valuationSchedule_.date(0)
                              : equityCurve_->fixingCalendar().advance(
                                    schedule_.date(0), -static_cast<Integer>(fixingDays_), Days, Preceding);
        initialPrice =
            initialPrice_ != Null<Real>() ? initialPrice_ : equityCurve_->fixing(fixingStartDate, false, false);
        fxRate = fxIndex_ ? fxIndex_->fixing(fixingStartDate) : 1.0;
    }

    std::vector<Real> notionals = notionals_;

    if (quantity_ != Null<Real>()) {
        QL_REQUIRE(notionals_.empty(), "EquityLeg: notional and quantity must not be given at the same time");
        if(notionalReset_) {
            quantity = quantity_;
        } else {
            QL_REQUIRE(initialPrice != Null<Real>() && fxRate != Null<Real>(),
                       "EquityLeg: initialPrice or fxRate not given, this is unexpected");
            notionals = std::vector<Real>(1, quantity_ * initialPrice * fxRate);
        }
    } else if (!notionals_.empty()) {
        QL_REQUIRE(quantity_ == Null<Real>(), "EquityLeg: notional and quantity must not be given at the same time");
        if (notionalReset_) {
            QL_REQUIRE(initialPrice != Null<Real>() && fxRate != Null<Real>(),
                       "EquityLeg: initialPrice or fxRate not given, this is unexpected");
            quantity = notionals_.front() / (initialPrice * fxRate);
        }
    } else {
        QL_FAIL("EquityLeg: either notional or quantity must be given");
    }

    if (valuationSchedule_.size() > 0){
        QL_REQUIRE(valuationSchedule_.size() == schedule_.size(), "Valuation and Payment Schedule sizes do not match");
    }

    for (Size i = 0; i < numPeriods; ++i) {
        startDate = schedule_.date(i);
        endDate = schedule_.date(i + 1); 
        paymentDate = calendar.adjust(endDate, paymentAdjustment_);

        Date fixingStartDate = Date();
        Date fixingEndDate = Date();
        if (valuationSchedule_.size() > 0) {
            fixingStartDate = valuationSchedule_.date(i);
            fixingEndDate = valuationSchedule_.date(i + 1);
        }

        Real initialPrice = (i == 0) ? initialPrice_ : Null<Real>();

        boost::shared_ptr<EquityCoupon> cashflow(
            new EquityCoupon(paymentDate, detail::get(notionals_, i, 0.0), startDate, endDate, fixingDays_,
                             equityCurve_, paymentDayCounter_, isTotalReturn_, dividendFactor_, notionalReset_,
                             initialPrice, quantity, fixingStartDate, fixingEndDate, Date(), Date(), Date(), fxIndex_));

        boost::shared_ptr<EquityCouponPricer> pricer(new EquityCouponPricer);
        cashflow->setPricer(pricer);

        cashflows.push_back(cashflow);
    }
    return cashflows;
}

} // namespace QuantExt
