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
#include <boost/algorithm/string/case_conv.hpp>

using namespace QuantLib;

namespace QuantExt {

std::ostream& operator<<(std::ostream& out, EquityReturnType t) {
    switch (t) {
    case EquityReturnType::Price:
        return out << "Price";
    case EquityReturnType::Total:
        return out << "Total";
    case EquityReturnType::Absolute:
        return out << "Absolute";
    case EquityReturnType::Dividend:
        return out << "Dividend";
    default:
        QL_FAIL("unknown EquityReturnType(" << int(t) << ")");
    }
}

EquityReturnType parseEquityReturnType(const std::string& str) {
    if (boost::algorithm::to_upper_copy(str) == "PRICE")
        return EquityReturnType::Price;
    else if (boost::algorithm::to_upper_copy(str) == "TOTAL")
        return EquityReturnType::Total;
    else if (boost::algorithm::to_upper_copy(str) == "ABSOLUTE")
        return EquityReturnType::Absolute;
    else if (boost::algorithm::to_upper_copy(str) == "DIVIDEND")
        return EquityReturnType::Dividend;
    QL_FAIL("Invalid EquityReturnType " << str);
}

EquityCoupon::EquityCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                           Natural fixingDays, const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
                           const DayCounter& dayCounter, EquityReturnType returnType, Real dividendFactor,
                           bool notionalReset, Real initialPrice, Real quantity, const Date& fixingStartDate,
                           const Date& fixingEndDate, const Date& refPeriodStart, const Date& refPeriodEnd, 
                           const Date& exCouponDate, const QuantLib::ext::shared_ptr<FxIndex>& fxIndex,
                           const bool initialPriceIsInTargetCcy, Real legInitialNotional, const Date& legFixingDate)
    : Coupon(paymentDate, nominal, startDate, endDate, refPeriodStart, refPeriodEnd, exCouponDate),
      fixingDays_(fixingDays), equityCurve_(equityCurve), dayCounter_(dayCounter), returnType_(returnType),
      dividendFactor_(dividendFactor), notionalReset_(notionalReset), initialPrice_(initialPrice),
      initialPriceIsInTargetCcy_(initialPriceIsInTargetCcy), quantity_(quantity), fixingStartDate_(fixingStartDate),
      fixingEndDate_(fixingEndDate), fxIndex_(fxIndex), legInitialNotional_(legInitialNotional),
      legFixingDate_(legFixingDate)  {
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

    // QL_REQUIRE(!notionalReset_ || quantity_ != Null<Real>(), "EquityCoupon: quantity required if notional resets");
    QL_REQUIRE(notionalReset_ || nominal_ != Null<Real>(),
               "EquityCoupon: notional required if notional does not reset");
}

void EquityCoupon::setPricer(const QuantLib::ext::shared_ptr<EquityCouponPricer>& pricer) {
    if (pricer_)
        unregisterWith(pricer_);
    pricer_ = pricer;
    if (pricer_)
        registerWith(pricer_);
    update();
}

Real EquityCoupon::quantity() const {
    if (notionalReset_ && quantity_ == Null<Real>()) {
        QL_REQUIRE(legInitialNotional_ != Null<Real>() && legFixingDate_ != Date(),
           "leg initial notional and fixing date required to compute the missing quantity in case of a resetting equity leg");
        quantity_ = legInitialNotional_ / equityCurve_->fixing(legFixingDate_, false, false);
    }
    return quantity_;
}

Real EquityCoupon::nominal() const {
    // use quantity for dividend swaps, this ensures notional resetting is not relevant
    // swaplet rate returns the absolute dividend to match
    if (returnType_ == EquityReturnType::Dividend)
        return quantity();
    else if(notionalReset_) {
        Real mult = (initialPrice_ == 0) ? 1 : initialPrice();
        return mult * (initialPriceIsInTargetCcy_ ? 1.0 : fxRate()) * quantity();
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
    // We may need a fixing date at the leg start date if it a notionalReset and the quantity is null
    // Quantity is null if no initial price was given for the Swap
    if (notionalReset_ && quantity_ == Null<Real>())
        fixingDates.push_back(legFixingDate_);
    return fixingDates;
};

EquityLeg::EquityLeg(const Schedule& schedule, const QuantLib::ext::shared_ptr<QuantExt::EquityIndex2>& equityCurve,
                     const QuantLib::ext::shared_ptr<FxIndex>& fxIndex)
    : schedule_(schedule), equityCurve_(equityCurve), fxIndex_(fxIndex), paymentLag_(0), paymentAdjustment_(Following),
      paymentCalendar_(Calendar()), returnType_(EquityReturnType::Total), initialPrice_(Null<Real>()),
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

EquityLeg& EquityLeg::withReturnType(EquityReturnType returnType) {
    returnType_ = returnType;
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
                   "mismatch in valuationSchedule (" << valuationSchedule_.size() << ") and scheduleData ("
                                                     << schedule_.size() << ") sizes");
    }

    Date legFixingDate = Date();
    if (valuationSchedule_.size() > 0)
        legFixingDate = valuationSchedule_.dates().front();
    else if (schedule_.size() > 0)
        legFixingDate = equityCurve_->fixingCalendar().advance(schedule_.dates().front(),
                                                               -static_cast<Integer>(fixingDays_), Days, Preceding);
    else
        QL_FAIL("Cannot build equity leg, neither schedule nor valuation schedule are defined");
    
    Real quantity = Null<Real>(), notional = Null<Real>(), legInitialNotional = Null<Real>();
    if (notionalReset_) {
        // We need a quantity in each coupon in this case
        if (quantity_ != Null<Real>()) {
            quantity = quantity_;
            QL_REQUIRE(notionals_.empty(), "EquityLeg: notional and quantity are given at the same time");
        } else {
            // If we have a notional, but no quantity is given:
            // a) Compute quantity from notional and initial price
            // b) If initial price is missing, leave quantity undefined here:
            //    The coupon will compute it from legInitialNotional=notional and legFixingDate
            QL_REQUIRE(!notionals_.empty(), "EquityLeg: can not compute qunantity, since no notional is given");
            QL_REQUIRE(fxIndex_ == nullptr || initialPriceIsInTargetCcy_,
                           "EquityLeg: can not compute quantity from nominal when fx conversion is required");
            notional = notionals_.front();
            legInitialNotional = notional;
            if (initialPrice_ != Null<Real>())
                quantity = (initialPrice_ == 0) ? notional : notional / initialPrice_;
        }
    } else {
        if (!notionals_.empty()) {
            QL_REQUIRE(quantity_ == Null<Real>(), "EquityLeg: notional and quantity are given at the same time");
            // notional is determined below in the loop over the periods
            legInitialNotional = notionals_.front();
        } else {
            QL_REQUIRE(initialPrice_ != Null<Real>(), "EquityLeg: can not compute notional, since no intialPrice is given");
                QL_REQUIRE(quantity_ != Null<Real>(), "EquityLeg: can not compute notional, since no quantity is given");
            QL_REQUIRE(fxIndex_ == nullptr || initialPriceIsInTargetCcy_,
                           "EquityLeg: can not compute notional from quantity when fx conversion is required");
                notional = (initialPrice_ == 0) ? quantity_ : quantity_ * initialPrice_;
        }
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
        if (!notionalReset_ && !notionals_.empty()) {
            notional = detail::get(notionals_, i, 0.0);
        }

        QuantLib::ext::shared_ptr<EquityCoupon> cashflow(new EquityCoupon(
            paymentDate, notional, startDate, endDate, fixingDays_, equityCurve_, paymentDayCounter_, returnType_,
            dividendFactor_, notionalReset_, initialPrice, quantity, fixingStartDate, fixingEndDate, Date(), Date(),
            Date(), fxIndex_, initialPriceIsInTargetCcy, legInitialNotional, legFixingDate));

        QuantLib::ext::shared_ptr<EquityCouponPricer> pricer(new EquityCouponPricer);
        cashflow->setPricer(pricer);

        cashflows.push_back(cashflow);
    }
    return cashflows;
}

} // namespace QuantExt
