/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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

/*! \file qle/cashflows/nonstandardyoyinflationcoupon.hpp
    \brief capped floored coupon which generalize the yoy inflation coupon
    it pays:
         N * (alpha * I_t/I_s + beta)
         N * (alpha * (I_t/I_s - 1) + beta)
    with an arbitrary time s<t, instead of a fixed 1y offset
    \ingroup cashflows
*/

#ifndef quantext_nonstandardcappedlfooredyoycoupon_hpp
#define quantext_nonstandardcappedlfooredyoycoupon_hpp

#include <qle/cashflows/nonstandardinflationcouponpricer.hpp>
#include <qle/cashflows/nonstandardyoyinflationcoupon.hpp>
#include <qle/cashflows/yoyinflationcoupon.hpp>

namespace QuantExt {
using namespace QuantLib;
//! Capped or floored inflation coupon.
/*! Essentially a copy of the nominal version but taking a
    different index and a set of pricers (not just one).

    The payoff \f$ P \f$ of a capped inflation-rate coupon
    with paysWithin = true is:

    \f[ P = N \times T \times \min(a L + b, C). \f]

    where \f$ N \f$ is the notional, \f$ T \f$ is the accrual
    time, \f$ L \f$ is the inflation rate, \f$ a \f$ is its
    gearing, \f$ b \f$ is the spread, and \f$ C \f$ and \f$ F \f$
    the strikes.

    The payoff of a floored inflation-rate coupon is:

    \f[ P = N \times T \times \max(a L + b, F). \f]

    The payoff of a collared inflation-rate coupon is:

    \f[ P = N \times T \times \min(\max(a L + b, F), C). \f]

    If paysWithin = false then the inverse is returned
    (this provides for instrument cap and caplet prices).

    They can be decomposed in the following manner.  Decomposition
    of a capped floating rate coupon when paysWithin = true:
    \f[
    R = \min(a L + b, C) = (a L + b) + \min(C - b - \xi |a| L, 0)
    \f]
    where \f$ \xi = sgn(a) \f$. Then:
    \f[
    R = (a L + b) + |a| \min(\frac{C - b}{|a|} - \xi L, 0)
    \f]
 */
class NonStandardCappedFlooredYoYInflationCoupon : public NonStandardYoYInflationCoupon {
public:
    // we may watch an underlying coupon ...
    NonStandardCappedFlooredYoYInflationCoupon(const ext::shared_ptr<NonStandardYoYInflationCoupon>& underlying,
                                               Rate cap = Null<Rate>(), Rate floor = Null<Rate>());

    // ... or not
    NonStandardCappedFlooredYoYInflationCoupon(const Date& paymentDate, Real nominal, const Date& startDate,
                                               const Date& endDate, Natural fixingDays,
                                               const ext::shared_ptr<ZeroInflationIndex>& index,
                                               const Period& observationLag, const DayCounter& dayCounter,
                                               Real gearing = 1.0, Spread spread = 0.0, const Rate cap = Null<Rate>(),
                                               const Rate floor = Null<Rate>(), const Date& refPeriodStart = Date(), 
                                               const Date& refPeriodEnd = Date(), bool addInflationNotional = false,
                                               QuantLib::CPI::InterpolationType interpolation = QuantLib::CPI::InterpolationType::Flat);

    //! \name augmented Coupon interface
    //@{
    //! swap(let) rate
    Rate rate() const override;
    //! cap
    Rate cap() const;
    //! floor
    Rate floor() const;
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
    virtual void accept(AcyclicVisitor& v) override;
    //@}

    bool isCapped() const { return isCapped_; }
    bool isFloored() const { return isFloored_; }

    void setPricer(const ext::shared_ptr<NonStandardYoYInflationCouponPricer>&);

protected:
    virtual void setCommon(Rate cap, Rate floor);

    // data, we only use underlying_ if it was constructed that way,
    // generally we use the shared_ptr conversion to boolean to test
    ext::shared_ptr<NonStandardYoYInflationCoupon> underlying_;
    bool isFloored_, isCapped_;
    Rate cap_, floor_;
};

class NonStandardYoYInflationLeg {
public:
    NonStandardYoYInflationLeg(const Schedule& schedule, const Calendar& cal,
                               const ext::shared_ptr<ZeroInflationIndex>& index, const Period& observationLag);
    NonStandardYoYInflationLeg& withNotionals(Real notional);
    NonStandardYoYInflationLeg& withNotionals(const std::vector<Real>& notionals);
    NonStandardYoYInflationLeg& withPaymentDayCounter(const DayCounter&);
    NonStandardYoYInflationLeg& withPaymentAdjustment(BusinessDayConvention);
    NonStandardYoYInflationLeg& withFixingDays(Natural fixingDays);
    NonStandardYoYInflationLeg& withFixingDays(const std::vector<Natural>& fixingDays);
    NonStandardYoYInflationLeg& withGearings(Real gearing);
    NonStandardYoYInflationLeg& withGearings(const std::vector<Real>& gearings);
    NonStandardYoYInflationLeg& withSpreads(Spread spread);
    NonStandardYoYInflationLeg& withSpreads(const std::vector<Spread>& spreads);
    NonStandardYoYInflationLeg& withCaps(Rate cap);
    NonStandardYoYInflationLeg& withCaps(const std::vector<Rate>& caps);
    NonStandardYoYInflationLeg& withFloors(Rate floor);
    NonStandardYoYInflationLeg& withFloors(const std::vector<Rate>& floors);
    NonStandardYoYInflationLeg& withRateCurve(const Handle<YieldTermStructure>& rateCurve);
    NonStandardYoYInflationLeg& withInflationNotional(bool addInflationNotional_);
    NonStandardYoYInflationLeg& withObservationInterpolation(QuantLib::CPI::InterpolationType interpolation);
    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<ZeroInflationIndex> index_;
    Period observationLag_;
    std::vector<Real> notionals_;
    DayCounter paymentDayCounter_;
    BusinessDayConvention paymentAdjustment_;
    Calendar paymentCalendar_;
    std::vector<Natural> fixingDays_;
    std::vector<Real> gearings_;
    std::vector<Spread> spreads_;
    std::vector<Rate> caps_, floors_;
    Handle<YieldTermStructure> rateCurve_;
    bool addInflationNotional_;
    QuantLib::CPI::InterpolationType interpolation_;
};

} // namespace QuantExt

#endif
