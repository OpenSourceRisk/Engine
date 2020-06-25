/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2017 Joseph Jeisman
 Copyright (C) 2017 Fabrice Lecuyer

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file overnightindexedcoupon.hpp
    \brief coupon paying the compounded daily overnight rate,
           copy of QL class, added includeSpread flag
*/

#pragma once

#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

namespace QuantLib {
class OptionletVolatilityStructure;
}

namespace QuantExt {
using namespace QuantLib;

//! overnight coupon
/*! %Coupon paying the compounded interest due to daily overnight fixings.

    \warning telescopicValueDates optimizes the schedule for calculation speed,
    but might fail to produce correct results if the coupon ages by more than
    a grace period of 7 days. It is therefore recommended not to set this flag
    to true unless you know exactly what you are doing. The intended use is
    rather by the OISRateHelper which is safe, since it reinitialises the
    instrument each time the evaluation date changes.

    if includeSpread = true, the spread is included in the daily compounding,
    otherwise it is added to the effective coupon rate after the compounding
*/
class OvernightIndexedCoupon : public FloatingRateCoupon {
public:
    OvernightIndexedCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                           const ext::shared_ptr<OvernightIndex>& overnightIndex, Real gearing = 1.0,
                           Spread spread = 0.0, const Date& refPeriodStart = Date(), const Date& refPeriodEnd = Date(),
                           const DayCounter& dayCounter = DayCounter(), bool telescopicValueDates = false,
                           bool includeSpread = false, const Period& lookback = 0 * Days, const Natural rateCutoff = 0,
                           const Natural fixingDays = Null<Size>());
    //! \name Inspectors
    //@{
    //! fixing dates for the rates to be compounded
    const std::vector<Date>& fixingDates() const { return fixingDates_; }
    //! accrual (compounding) periods
    const std::vector<Time>& dt() const { return dt_; }
    //! fixings to be compounded
    const std::vector<Rate>& indexFixings() const;
    //! value dates for the rates to be compounded
    const std::vector<Date>& valueDates() const { return valueDates_; }
    //! include spread in compounding?
    bool includeSpread() const { return includeSpread_; }
    /*! effectiveSpread and effectiveIndexFixing are set such that
        coupon amount = notional * accrualPeriod * ( gearing * effectiveIndexFixing + effectiveSpread )
        notice that
        - gearing = 1 is required if includeSpread = true
        - effectiveSpread = spread() if includeSpread = false */
    Real effectiveSpread() const;
    Real effectiveIndexFixing() const;
    //! lookback period
    const Period& lookback() const { return lookback_; }
    //! rate cutoff
    Natural rateCutoff() const { return rateCutoff_; }
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    //! the date when the coupon is fully determined
    Date fixingDate() const { return fixingDates_[fixingDates_.size() - 1 - rateCutoff_]; }
    //@}
    //! \name Visitability
    //@{
    void accept(AcyclicVisitor&);
    //@}
private:
    std::vector<Date> valueDates_, fixingDates_;
    mutable std::vector<Rate> fixings_;
    Size n_;
    std::vector<Time> dt_;
    bool includeSpread_;
    Period lookback_;
    Natural rateCutoff_;
};

//! capped floored overnight indexed coupon
class CappedFlooredOvernightIndexedCoupon : public FloatingRateCoupon {
public:
    CappedFlooredOvernightIndexedCoupon(const ext::shared_ptr<OvernightIndexedCoupon>& underlying,
                                        Real cap = Null<Real>(), Real floor = Null<Real>(), bool nakedOption = false);

    //! \name Coupon interface
    //@{
    Rate rate() const override;
    Rate convexityAdjustment() const override;
    //@}
    //! \name FloatingRateCoupon interface
    //@{
    Date fixingDate() const override { return underlying_->fixingDate(); }
    //@}
    //! cap
    Rate cap() const { return cap_; }
    //! floor
    Rate floor() const { return floor_; }
    //! effective cap of fixing
    Rate effectiveCap() const;
    //! effective floor of fixing
    Rate effectiveFloor() const;
    //@}
    //! \name Observer interface
    //@{
    void update() override;
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;

    bool isCapped() const { return cap_ != Null<Real>(); }
    bool isFloored() const { return floor_ != Null<Real>(); }

    ext::shared_ptr<OvernightIndexedCoupon> underlying() const { return underlying_; }

protected:
    ext::shared_ptr<OvernightIndexedCoupon> underlying_;
    Rate cap_, floor_;
    bool nakedOption_;
};

//! capped floored overnight indexed coupon pricer base class
class CappedFlooredOvernightIndexedCouponPricer : public FloatingRateCouponPricer {
public:
    CappedFlooredOvernightIndexedCouponPricer(const Handle<OptionletVolatilityStructure>& v);
    Handle<OptionletVolatilityStructure> capletVolatility() const;

private:
    Handle<OptionletVolatilityStructure> capletVol_;
};

//! helper class building a sequence of overnight coupons
class OvernightLeg {
public:
    OvernightLeg(const Schedule& schedule, const ext::shared_ptr<OvernightIndex>& overnightIndex);
    OvernightLeg& withNotionals(Real notional);
    OvernightLeg& withNotionals(const std::vector<Real>& notionals);
    OvernightLeg& withPaymentDayCounter(const DayCounter&);
    OvernightLeg& withPaymentAdjustment(BusinessDayConvention);
    OvernightLeg& withPaymentCalendar(const Calendar&);
    OvernightLeg& withPaymentLag(Natural lag);
    OvernightLeg& withGearings(Real gearing);
    OvernightLeg& withGearings(const std::vector<Real>& gearings);
    OvernightLeg& withSpreads(Spread spread);
    OvernightLeg& withSpreads(const std::vector<Spread>& spreads);
    OvernightLeg& withTelescopicValueDates(bool telescopicValueDates);
    OvernightLeg& includeSpread(bool includeSpread);
    OvernightLeg& withLookback(const Period& lookback);
    OvernightLeg& withRateCutoff(const Natural rateCutoff);
    OvernightLeg& withFixingDays(const Natural fixingDays);
    OvernightLeg& withCaps(Rate cap);
    OvernightLeg& withCaps(const std::vector<Rate>& caps);
    OvernightLeg& withFloors(Rate floor);
    OvernightLeg& withFloors(const std::vector<Rate>& floors);
    OvernightLeg& withNakedOption(const bool nakedOption);
    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<OvernightIndex> overnightIndex_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    Calendar paymentCalendar_;
    BusinessDayConvention paymentAdjustment_;
    Natural paymentLag_;
    std::vector<Real> gearings_;
    std::vector<Spread> spreads_;
    bool telescopicValueDates_;
    bool includeSpread_;
    Period lookback_;
    Natural rateCutoff_;
    Natural fixingDays_;
    std::vector<Rate> caps_, floors_;
    bool nakedOption_;
};

} // namespace QuantExt
