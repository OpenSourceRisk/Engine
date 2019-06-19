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

/*! \file cpicouponpricer.hpp
    \brief CPI cash flow and coupon pricers that handle caps/floors
*/

#ifndef quantext_cpicouponpricer_hpp
#define quantext_cpicouponpricer_hpp

#include <ql/option.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/schedule.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <qle/cashflows/cpicoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Base class for CPI CashFLow and Coupon pricers
class CPIPricer : public virtual Observer, public virtual Observable {
public:
    CPIPricer(const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
              const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>() ) : vol_(vol), yts_(yts) {
        if (!vol_.empty())
            registerWith(vol_);
        if (yts_.empty())
            yts_ = Handle<YieldTermStructure>(boost::shared_ptr<YieldTermStructure>(new FlatForward(0, NullCalendar(), 0.05, Actual365Fixed())));
    }
    virtual ~CPIPricer() {}
    virtual void setVolatility(const Handle<CPIVolatilitySurface>& vol) {
        QL_REQUIRE(!vol.empty(), "empty vol handle")
        vol_ = vol;
        registerWith(vol_);
    }

    Handle<CPIVolatilitySurface> volatility() { return vol_; }
    Handle<YieldTermStructure> yieldCurve() { return yts_; }

    //! \name InflationCouponPricer interface
    //@{
    virtual Real amount() const = 0;
    virtual Real cap() const = 0;
    virtual Real floor() const = 0;
    //@}

    //! \name Observer interface
    //@{
    virtual void update() { notifyObservers(); }
    //@}
protected:
    Handle<CPIVolatilitySurface> vol_;
    Handle<YieldTermStructure> yts_;
};

//! Black CPI CashFlow Pricer.
class BlackCPICashFlowPricer : public CPIPricer {
public:
    BlackCPICashFlowPricer(const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>()) : CPIPricer(vol) {}
    Real amount() const;
    Real cap() const;
    Real floor() const;
    void initialize(const CappedFlooredCPICashFlow&);

private:
    Real optionletPrice(Option::Type optionType, Real effStrike) const;
    const CappedFlooredCPICashFlow* cashflow_;
};

class BlackCPICouponPricer : public InflationCouponPricer {
public:
    BlackCPICouponPricer(const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>()) : vol_(vol) {}
    Real swapletPrice() const;
    Rate swapletRate() const;
    Real capletPrice(Rate effectiveCap) const;
    Rate capletRate(Rate effectiveCap) const;
    Real floorletPrice(Rate effectiveFloor) const;
    Rate floorletRate(Rate effectiveFloor) const;
    void initialize(const InflationCoupon&);

private:
    Real optionletPrice(Option::Type optionType, Real effStrike) const;
    const CappedFlooredCPICoupon* coupon_;
    Handle<CPIVolatilitySurface> vol_;
};

} // namespace QuantExt

#endif
