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

/*! \file qle/cashflows/cmbcoupon.hpp
    \brief Constant Maturity Bond yield coupon
*/

#ifndef quantext_cmb_coupon_hpp
#define quantext_cmb_coupon_hpp

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/time/schedule.hpp>
#include <qle/indexes/bondindex.hpp>

namespace QuantExt {

//! CMB coupon class
class CmbCoupon : public FloatingRateCoupon {
public:
    CmbCoupon(const Date& paymentDate,
	      Real nominal,
	      const Date& startDate,
	      const Date& endDate,
	      Natural fixingDays,
	      const ext::shared_ptr<ConstantMaturityBondIndex>& index,
	      Real gearing = 1.0,
	      Spread spread = 0.0,
	      const Date& refPeriodStart = Date(),
	      const Date& refPeriodEnd = Date(),
	      const DayCounter& dayCounter = DayCounter(),
	      bool isInArrears = false,
	      const Date& exCouponDate = Date());
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&) override;
    //@}

    //! \name Inspectors
    //@{
    const ext::shared_ptr<ConstantMaturityBondIndex>& bondIndex() const { return bondIndex_; }
    //@}

    void setPricer(const ext::shared_ptr<FloatingRateCouponPricer>& pricer) override;

private:
    ext::shared_ptr<ConstantMaturityBondIndex> bondIndex_;
};

//! Base pricer for vanilla CMB coupons
/*! 
  \todo: caching of coupon data, timing and convexity adjustment
 */
class CmbCouponPricer : public FloatingRateCouponPricer {
public:
    explicit CmbCouponPricer() {}
    void initialize(const FloatingRateCoupon& coupon) override;
    Real swapletPrice() const override;
    Rate swapletRate() const override;
    Real capletPrice(Rate effectiveCap) const override;
    Rate capletRate(Rate effectiveCap) const override;
    Real floorletPrice(Rate effectiveFloor) const override;
    Rate floorletRate(Rate effectiveFloor) const override;
private:
    const CmbCoupon* coupon_;
    ext::shared_ptr<ConstantMaturityBondIndex> index_;
    Real gearing_;
    Real spread_;
    Date fixingDate_;
};

//! helper class building a sequence of capped/floored cmb coupons
class CmbLeg {
public:
    CmbLeg(Schedule schedule, std::vector<ext::shared_ptr<ConstantMaturityBondIndex>> bondIndices);
    CmbLeg& withNotionals(Real notional);
    CmbLeg& withNotionals(const std::vector<Real>& notionals);
    CmbLeg& withPaymentDayCounter(const DayCounter&);
    CmbLeg& withPaymentCalendar(const Calendar& cal);
    CmbLeg& withPaymentAdjustment(BusinessDayConvention);
    CmbLeg& withFixingDays(Natural fixingDays);
    CmbLeg& withFixingDays(const std::vector<Natural>& fixingDays);
    CmbLeg& withGearings(Real gearing);
    CmbLeg& withGearings(const std::vector<Real>& gearings);
    CmbLeg& withSpreads(Spread spread);
    CmbLeg& withSpreads(const std::vector<Spread>& spreads);
    CmbLeg& withCaps(Rate cap);
    CmbLeg& withCaps(const std::vector<Rate>& caps);
    CmbLeg& withFloors(Rate floor);
    CmbLeg& withFloors(const std::vector<Rate>& floors);
    CmbLeg& inArrears(bool flag = true);
    CmbLeg& withZeroPayments(bool flag = true);
    CmbLeg& withExCouponPeriod(const Period&,
			       const Calendar&,
			       BusinessDayConvention,
			       bool endOfMonth);
    operator Leg() const;
private:
    Schedule schedule_;
    std::vector<ext::shared_ptr<ConstantMaturityBondIndex>> bondIndices_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    BusinessDayConvention paymentAdjustment_;
    Calendar paymentCalendar_;
    std::vector<Natural> fixingDays_;
    std::vector<Real> gearings_;
    std::vector<Spread> spreads_;
    std::vector<Rate> caps_, floors_;
    bool inArrears_, zeroPayments_;
    Period exCouponPeriod_;
    Calendar exCouponCalendar_;
    BusinessDayConvention exCouponAdjustment_;
    bool exCouponEndOfMonth_;
};

}

#endif
