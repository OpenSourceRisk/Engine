/*
 Copyright (C) 2022 Quaternion Risk Management Ltd.
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

#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <qle/cashflows/cmbcoupon.hpp>
#include <utility>

namespace QuantExt {

CmbCoupon::CmbCoupon(const Date& paymentDate,
		     Real nominal,
		     const Date& startDate,
		     const Date& endDate,
		     Natural fixingDays,
		     const ext::shared_ptr<ConstantMaturityBondIndex>& bondIndex,
		     Real gearing,
		     Spread spread,
		     const Date& refPeriodStart,
		     const Date& refPeriodEnd,
		     const DayCounter& dayCounter,
		     bool isInArrears,
		     const Date& exCouponDate)
  : FloatingRateCoupon(paymentDate, nominal, startDate, endDate,
		       fixingDays, bondIndex, gearing, spread,
		       refPeriodStart, refPeriodEnd,
		       dayCounter, isInArrears, exCouponDate),
    bondIndex_(bondIndex) {
    registerWith(bondIndex_);
}

void CmbCoupon::accept(AcyclicVisitor& v) {
    auto* v1 = dynamic_cast<Visitor<CmbCoupon>*>(&v);
    if (v1 != nullptr)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

void CmbCouponPricer::initialize(const FloatingRateCoupon& coupon) {
    coupon_ = dynamic_cast<const CmbCoupon *>(&coupon);
    QL_REQUIRE(coupon_, "CmbCouponPricer: expected CmbCoupon");
    index_ = coupon_->bondIndex();
    gearing_ = coupon_->gearing();
    spread_ = coupon_->spread();
    fixingDate_ = coupon_->fixingDate();
}

Real CmbCouponPricer::swapletPrice() const {
    return 0.0;
}

Rate CmbCouponPricer::swapletRate() const {
    return gearing_ * (index_->fixing(fixingDate_) + spread_);
}

Real CmbCouponPricer::capletPrice(Rate effectiveCap) const {
    return 0.0;
};

Rate CmbCouponPricer::capletRate(Rate effectiveCap) const {
    return 0.0;
};

Real CmbCouponPricer::floorletPrice(Rate effectiveFloor) const {
    return 0.0;
};

Rate CmbCouponPricer::floorletRate(Rate effectiveFloor) const {
    return 0.0;
};

void CmbCoupon::setPricer(const ext::shared_ptr<FloatingRateCouponPricer>& pricer) {
    FloatingRateCoupon::setPricer(pricer);
}

CmbLeg::CmbLeg(Schedule schedule, std::vector<ext::shared_ptr<ConstantMaturityBondIndex>> bondIndices)
    : schedule_(std::move(schedule)), bondIndices_(bondIndices),
      paymentAdjustment_(Following), inArrears_(false), zeroPayments_(false) {
    QL_REQUIRE(bondIndices_.size() == schedule_.size() - 1, "vector size mismatch between schedule ("
                                                                << schedule_.size() << ") and bond indices ("
                                                                << bondIndices_.size() << ")");
}

CmbLeg& CmbLeg::withNotionals(Real notional) {
    notionals_ = std::vector<Real>(1, notional);
    return *this;
}

CmbLeg& CmbLeg::withNotionals(const std::vector<Real>& notionals) {
    notionals_ = notionals;
    return *this;
}
  
CmbLeg& CmbLeg::withPaymentDayCounter(const DayCounter& dayCounter) {
    paymentDayCounter_ = dayCounter;
    return *this;
}

CmbLeg& CmbLeg::withPaymentAdjustment(BusinessDayConvention convention) {
    paymentAdjustment_ = convention;
    return *this;
}

CmbLeg& CmbLeg::withFixingDays(Natural fixingDays) {
    fixingDays_ = std::vector<Natural>(1, fixingDays);
    return *this;
}

CmbLeg& CmbLeg::withFixingDays(const std::vector<Natural>& fixingDays) {
    fixingDays_ = fixingDays;
    return *this;
}
  
CmbLeg& CmbLeg::withGearings(Real gearing) {
    gearings_ = std::vector<Real>(1, gearing);
    return *this;
}

CmbLeg& CmbLeg::withGearings(const std::vector<Real>& gearings) {
    gearings_ = gearings;
    return *this;
}

CmbLeg& CmbLeg::withSpreads(Spread spread) {
    spreads_ = std::vector<Spread>(1, spread);
    return *this;
}

CmbLeg& CmbLeg::withSpreads(const std::vector<Spread>& spreads) {
    spreads_ = spreads;
    return *this;
}

CmbLeg& CmbLeg::withCaps(Rate cap) {
    caps_ = std::vector<Rate>(1, cap);
    return *this;
}

CmbLeg& CmbLeg::withCaps(const std::vector<Rate>& caps) {
    caps_ = caps;
    return *this;
}

CmbLeg& CmbLeg::withFloors(Rate floor) {
    floors_ = std::vector<Rate>(1, floor);
    return *this;
}

CmbLeg& CmbLeg::withFloors(const std::vector<Rate>& floors) {
    floors_ = floors;
    return *this;
}

CmbLeg& CmbLeg::inArrears(bool flag) {
    inArrears_ = flag;
    return *this;
}

CmbLeg& CmbLeg::withZeroPayments(bool flag) {
    zeroPayments_ = flag;
    return *this;
}

CmbLeg& CmbLeg::withPaymentCalendar(const Calendar& cal) {
    paymentCalendar_ = cal;
    return *this;
}
  
CmbLeg& CmbLeg::withExCouponPeriod(const Period& period,
				   const Calendar& cal,
				   BusinessDayConvention convention,
				   bool endOfMonth) {
    exCouponPeriod_ = period;
    exCouponCalendar_ = cal;
    exCouponAdjustment_ = convention;
    exCouponEndOfMonth_ = endOfMonth;
    return *this;
}

CmbLeg::operator Leg() const {
    Leg leg;
    for (Size i = 0; i < schedule_.size() - 1; i++) {
        Date paymentDate = paymentCalendar_.adjust(schedule_[i + 1], paymentAdjustment_);
	QuantLib::ext::shared_ptr<CmbCoupon> coupon
	    = QuantLib::ext::make_shared<CmbCoupon>(paymentDate, notionals_[i], schedule_[i], schedule_[i + 1],
					    fixingDays_[i], bondIndices_[i], gearings_[i], spreads_[i], Date(), Date(),
					    paymentDayCounter_, inArrears_);	
	auto pricer = QuantLib::ext::make_shared<CmbCouponPricer>();
	coupon->setPricer(pricer);
	leg.push_back(coupon);
    }
    return leg;
}

}
