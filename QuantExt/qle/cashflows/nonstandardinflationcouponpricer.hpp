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

/*! \file qle/cashflows/nonstandardinflationcouponpricer.hpp
    \brief pricer for the generalized (nonstandard) yoy coupon
    the payoff of the coupon is:
         N * (alpha * I_t/I_s + beta)
         N * (alpha * (I_t/I_s - 1) + beta)
    with arbitrary s < t. In the regular coupon the period between
    s and t is hardcoded to one year. This pricer ignores any convexity adjustments
    in the YoY coupon.
    \ingroup cashflows
*/

#ifndef quantext_nonstandard_yoy_inflation_coupon_pricer_hpp
#define quantext_nonstandard_yoy_inflation_coupon_pricer_hpp

#include <ql/cashflow.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/option.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <qle/cashflows/nonstandardyoyinflationcoupon.hpp>

namespace QuantExt {

using namespace QuantLib;
//! base pricer for capped/floored YoY inflation coupons
/*! \note this pricer can already do swaplets but to get
          volatility-dependent coupons you need the descendents.
*/
class NonStandardYoYInflationCouponPricer : public QuantLib::InflationCouponPricer {
public:
    NonStandardYoYInflationCouponPricer(const Handle<YieldTermStructure>& nominalTermStructure);
    NonStandardYoYInflationCouponPricer(const Handle<YoYOptionletVolatilitySurface>& capletVol,
                                        const Handle<YieldTermStructure>& nominalTermStructure);

    virtual Handle<YoYOptionletVolatilitySurface> capletVolatility() const { return capletVol_; }

    virtual Handle<YieldTermStructure> nominalTermStructure() const { return nominalTermStructure_; }

    virtual void setCapletVolatility(const Handle<YoYOptionletVolatilitySurface>& capletVol);

    //! \name InflationCouponPricer interface
    //@{
    virtual Real swapletPrice() const override;
    virtual Rate swapletRate() const override;
    virtual Real capletPrice(Rate effectiveCap) const override;
    virtual Rate capletRate(Rate effectiveCap) const override;
    virtual Real floorletPrice(Rate effectiveFloor) const override;
    virtual Rate floorletRate(Rate effectiveFloor) const override;
    virtual void initialize(const InflationCoupon&) override;
    //@}

protected:
    virtual Real optionletPrice(Option::Type optionType, Real effStrike) const;
    virtual Real optionletRate(Option::Type optionType, Real effStrike) const;

    /*! Derived classes usually only need to implement this.

        The name of the method is misleading.  This actually
        returns the rate of the optionlet (so not discounted and
        not accrued).
    */
    virtual Real optionletPriceImp(Option::Type, Real strike, Real forward, Real stdDev) const;
    virtual Rate adjustedFixing(Rate fixing = Null<Rate>()) const;

    //! data
    Handle<YoYOptionletVolatilitySurface> capletVol_;
    Handle<YieldTermStructure> nominalTermStructure_;
    const NonStandardYoYInflationCoupon* coupon_;
    Real gearing_;
    Spread spread_;
    Real discount_;
};

//! Black-formula pricer for capped/floored yoy inflation coupons
class NonStandardBlackYoYInflationCouponPricer : public NonStandardYoYInflationCouponPricer {
public:
    NonStandardBlackYoYInflationCouponPricer(const Handle<YieldTermStructure>& nominalTermStructure)
        : NonStandardYoYInflationCouponPricer(nominalTermStructure){};
    NonStandardBlackYoYInflationCouponPricer(const Handle<YoYOptionletVolatilitySurface>& capletVol,
                                             const Handle<YieldTermStructure>& nominalTermStructure)
        : NonStandardYoYInflationCouponPricer(capletVol, nominalTermStructure) {}

protected:
    Real optionletPriceImp(Option::Type, Real strike, Real forward, Real stdDev) const override;
};

//! Unit-Displaced-Black-formula pricer for capped/floored yoy inflation coupons
class NonStandardUnitDisplacedBlackYoYInflationCouponPricer : public NonStandardYoYInflationCouponPricer {
public:
    NonStandardUnitDisplacedBlackYoYInflationCouponPricer(const Handle<YieldTermStructure>& nominalTermStructure)
        : NonStandardYoYInflationCouponPricer(nominalTermStructure){};
    NonStandardUnitDisplacedBlackYoYInflationCouponPricer(const Handle<YoYOptionletVolatilitySurface>& capletVol,
                                                          const Handle<YieldTermStructure>& nominalTermStructure)
        : NonStandardYoYInflationCouponPricer(capletVol, nominalTermStructure) {}

protected:
    Real optionletPriceImp(Option::Type, Real strike, Real forward, Real stdDev) const override;
};

//! Bachelier-formula pricer for capped/floored yoy inflation coupons
class NonStandardBachelierYoYInflationCouponPricer : public NonStandardYoYInflationCouponPricer {
public:
    NonStandardBachelierYoYInflationCouponPricer(const Handle<YieldTermStructure>& nominalTermStructure)
        : NonStandardYoYInflationCouponPricer(nominalTermStructure){};

    NonStandardBachelierYoYInflationCouponPricer(const Handle<YoYOptionletVolatilitySurface>& capletVol,
                                                 const Handle<YieldTermStructure>& nominalTermStructure)
        : NonStandardYoYInflationCouponPricer(capletVol, nominalTermStructure) {}

protected:
    Real optionletPriceImp(Option::Type, Real strike, Real forward, Real stdDev) const override;
};

} // namespace QuantExt

#endif
