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

/*! \file qle/cashflows/indexedcoupon.hpp
    \brief coupon with an indexed notional
    \ingroup cashflows
*/

#ifndef quantext_yoy_inflation_coupon_hpp
#define quantext_yoy_inflation_coupon_hpp

#include <ql/cashflow.hpp>
#include <ql/cashflows/capflooredinflationcoupon.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/time/schedule.hpp>

namespace QuantExt {
using namespace QuantLib;

//! %Extend the QuantLib YoYInflationCoupon, now the payoff is based on growth only (default behaviour) (I_t / I_{t-1} -
//! 1) or I_t / I_{t-1}

class YoYInflationCoupon : public QuantLib::YoYInflationCoupon {
public:
    YoYInflationCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                       Natural fixingDays, const ext::shared_ptr<YoYInflationIndex>& index,
                       const Period& observationLag, CPI::InterpolationType interpolation, const DayCounter& dayCounter,
                       Real gearing = 1.0, Spread spread = 0.0, const Date& refPeriodStart = Date(),
                       const Date& refPeriodEnd = Date(), bool addInflationNotional = false);

    /*! \deprecated Use the overload that passes an interpolation type instead.
                        Deprecated in version 1.36.
    */
    [[deprecated("Use the overload that passes an interpolation type instead")]]
    YoYInflationCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                       Natural fixingDays, const ext::shared_ptr<YoYInflationIndex>& index,
                       const Period& observationLag, const DayCounter& dayCounter, Real gearing = 1.0,
                       Spread spread = 0.0, const Date& refPeriodStart = Date(), const Date& refPeriodEnd = Date(),
                       bool addInflationNotional = false);

    // ! \name Coupon interface
    Rate rate() const override;
    //@}
    //@}
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}

private:
    bool addInflationNotional_;
};

class CappedFlooredYoYInflationCoupon : public QuantLib::CappedFlooredYoYInflationCoupon {
public:
    CappedFlooredYoYInflationCoupon(const ext::shared_ptr<YoYInflationCoupon>& underlying, Rate cap = Null<Rate>(),
                                    Rate floor = Null<Rate>(), bool addInflationNotional = false);



    CappedFlooredYoYInflationCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                                    Natural fixingDays, const ext::shared_ptr<YoYInflationIndex>& index,
                                    const Period& observationLag, CPI::InterpolationType interpolation,
                                    const DayCounter& dayCounter, Real gearing = 1.0,
                                    Spread spread = 0.0, const Rate cap = Null<Rate>(), const Rate floor = Null<Rate>(),
                                    const Date& refPeriodStart = Date(), const Date& refPeriodEnd = Date(),
                                    bool addInflationNotional = false);


    /*! \deprecated Use the overload that passes an interpolation type instead. */
    [[deprecated("Use the overload that passes an interpolation type instead")]]
    CappedFlooredYoYInflationCoupon(const Date& paymentDate, Real nominal, const Date& startDate, const Date& endDate,
                                    Natural fixingDays, const ext::shared_ptr<YoYInflationIndex>& index,
                                    const Period& observationLag, const DayCounter& dayCounter, Real gearing = 1.0,
                                    Spread spread = 0.0, const Rate cap = Null<Rate>(), const Rate floor = Null<Rate>(),
                                    const Date& refPeriodStart = Date(), const Date& refPeriodEnd = Date(),
                                    bool addInflationNotional = false);

    // ! \name Coupon interface
    Rate rate() const override;
    //! \name Visitability
    //@{
    virtual void accept(AcyclicVisitor&) override;
    //@}
private:
    bool addInflationNotional_;
};

//! Helper class building a sequence of capped/floored yoy inflation coupons
//! payoff is: spread + gearing x index
class yoyInflationLeg {
public:
    yoyInflationLeg(Schedule schedule, Calendar cal, ext::shared_ptr<YoYInflationIndex> index,
                    const Period& observationLag, CPI::InterpolationType interpolation);
    
                    /*! \deprecated Use the overload that passes an interpolation type instead. */
    [[deprecated("Use the overload that passes an interpolation type instead")]]
    yoyInflationLeg(Schedule schedule, Calendar cal, ext::shared_ptr<YoYInflationIndex> index,
                        const Period& observationLag);
    yoyInflationLeg& withNotionals(Real notional);
    yoyInflationLeg& withNotionals(const std::vector<Real>& notionals);
    yoyInflationLeg& withPaymentDayCounter(const DayCounter&);
    yoyInflationLeg& withPaymentAdjustment(BusinessDayConvention);
    yoyInflationLeg& withFixingDays(Natural fixingDays);
    yoyInflationLeg& withFixingDays(const std::vector<Natural>& fixingDays);
    yoyInflationLeg& withGearings(Real gearing);
    yoyInflationLeg& withGearings(const std::vector<Real>& gearings);
    yoyInflationLeg& withSpreads(Spread spread);
    yoyInflationLeg& withSpreads(const std::vector<Spread>& spreads);
    yoyInflationLeg& withCaps(Rate cap);
    yoyInflationLeg& withCaps(const std::vector<Rate>& caps);
    yoyInflationLeg& withFloors(Rate floor);
    yoyInflationLeg& withFloors(const std::vector<Rate>& floors);
    yoyInflationLeg& withRateCurve(const Handle<YieldTermStructure>& rateCurve);
    yoyInflationLeg& withInflationNotional(bool addInflationNotional_);
    operator Leg() const;

private:
    Schedule schedule_;
    ext::shared_ptr<YoYInflationIndex> index_;
    Period observationLag_;
    CPI::InterpolationType interpolation_;
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
};
} // namespace QuantExt
#endif
