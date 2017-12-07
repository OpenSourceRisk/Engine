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

#include <ql/cashflows/couponpricer.hpp>
#include <ql/indexes/interestrateindex.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/cashflows/floatingannuitycoupon.hpp>

namespace QuantExt {

FloatingAnnuityCoupon::FloatingAnnuityCoupon(Real annuity, bool underflow,
                                             const boost::shared_ptr<Coupon>& previousCoupon, const Date& paymentDate,
                                             const Date& startDate, const Date& endDate, Natural fixingDays,
                                             const boost::shared_ptr<InterestRateIndex>& index, Real gearing,
                                             Spread spread, const Date& refPeriodStart, const Date& refPeriodEnd,
                                             const DayCounter& dayCounter, bool isInArrears)
    : Coupon(paymentDate, 0.0, startDate, endDate, refPeriodStart, refPeriodEnd), annuity_(annuity),
      underflow_(underflow), previousCoupon_(previousCoupon), fixingDays_(fixingDays), index_(index), gearing_(gearing),
      spread_(spread), dayCounter_(dayCounter), isInArrears_(isInArrears) {

    if (dayCounter_ == DayCounter())
        dayCounter_ = index_->dayCounter();

    QL_REQUIRE(previousCoupon, "Non-empty previous coupon required for FloatingAnnuityCoupon");
    registerWith(previousCoupon);
    registerWith(index);
    registerWith(Settings::instance().evaluationDate());
}

void FloatingAnnuityCoupon::performCalculations() const {
    // If the previous coupon was a FloatingAnnuityCoupon we need to cast here in order to get its mutable nominal.
    // Using the Coupon interface previousCoupon_->nominal() would return zero.
    boost::shared_ptr<FloatingAnnuityCoupon> c = boost::dynamic_pointer_cast<FloatingAnnuityCoupon>(previousCoupon_);
    if (c)
        this->nominal_ = c->nominal() + c->amount() - annuity_;
    else
        this->nominal_ = previousCoupon_->nominal() + previousCoupon_->amount() - annuity_;

    // The following requires a QuantLib change (expected in 1.11), making the Coupon nominal() interface virtual
    // so that it can be overridden by this class.
    // this->nominal_ = previousCoupon_->nominal() + previousCoupon_->amount() - annuity_;
    if (this->nominal_ < 0.0 && underflow_ == false)
        this->nominal_ = 0.0;
    // std::cout << "FloatingAnnuityCoupon called() for startDate " << QuantLib::io::iso_date(accrualStartDate_) << " "
    // 	  << "Nominal " << this->nominal_ << std::endl;
}

Rate FloatingAnnuityCoupon::previousNominal() const { return previousCoupon_->nominal(); }

Rate FloatingAnnuityCoupon::nominal() const {
    calculate(); // lazy
    // performCalculations(); // not lazy
    return this->nominal_;
}

Real FloatingAnnuityCoupon::amount() const {
    calculate();
    return rate() * accrualPeriod() * this->nominal_;
}

Real FloatingAnnuityCoupon::accruedAmount(const Date& d) const {
    if (d <= accrualStartDate_ || d > paymentDate_) {
        return 0.0;
    } else {
        return this->nominal_ * rate() *
               dayCounter().yearFraction(accrualStartDate_, std::min(d, accrualEndDate_), refPeriodStart_,
                                         refPeriodEnd_);
    }
}

Rate FloatingAnnuityCoupon::rate() const { return gearing_ * (indexFixing() + spread_); }

Date FloatingAnnuityCoupon::fixingDate() const {
    // if isInArrears_ fix at the end of period
    Date refDate = isInArrears_ ? accrualEndDate_ : accrualStartDate_;
    return index_->fixingCalendar().advance(refDate, -static_cast<Integer>(fixingDays_), Days, Preceding);
}

Real FloatingAnnuityCoupon::indexFixing() const { return index_->fixing(fixingDate()); }

Real FloatingAnnuityCoupon::price(const Handle<YieldTermStructure>& discountingCurve) const {
    return amount() * discountingCurve->discount(date());
}

void FloatingAnnuityCoupon::accept(AcyclicVisitor& v) {
    Visitor<FloatingAnnuityCoupon>* v1 = dynamic_cast<Visitor<FloatingAnnuityCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

} // namespace QuantExt
