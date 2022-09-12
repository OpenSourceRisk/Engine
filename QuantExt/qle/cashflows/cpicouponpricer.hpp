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
    \brief CPI cash flow and coupon pricers that handle caps/floors using a CpiCapFloorEngine
*/

#ifndef quantext_cpicouponpricer_hpp
#define quantext_cpicouponpricer_hpp

#include <ql/cashflows/cpicouponpricer.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>
#include <ql/option.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/schedule.hpp>
#include <qle/cashflows/cpicoupon.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Base class for CPI CashFLow and Coupon pricers
class InflationCashFlowPricer : public virtual Observer, public virtual Observable {
public:
    InflationCashFlowPricer(const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
                            const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>());
    virtual ~InflationCashFlowPricer() {}

    //! Inspectors
    //@{
    Handle<CPIVolatilitySurface> volatility() { return vol_; }
    Handle<YieldTermStructure> yieldCurve() { return yts_; }
    //@}

    //! \name Observer interface
    //@{
    virtual void update() override { notifyObservers(); }
    //@}
protected:
    Handle<CPIVolatilitySurface> vol_;
    Handle<YieldTermStructure> yts_;
};

//! Black CPI CashFlow Pricer.
class BlackCPICashFlowPricer : public InflationCashFlowPricer {
public:
    BlackCPICashFlowPricer(const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
                           const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>(),
                           const bool useLastFixing = false);
    boost::shared_ptr<PricingEngine> engine() { return engine_; }

private:
    // engine to price the underlying cap/floor
    boost::shared_ptr<PricingEngine> engine_;
};

class BlackCPICouponPricer : public CPICouponPricer {
public:
    BlackCPICouponPricer(const Handle<CPIVolatilitySurface>& vol = Handle<CPIVolatilitySurface>(),
                         const Handle<YieldTermStructure>& yts = Handle<YieldTermStructure>(),
                         const bool useLastFixing = false);
    Handle<YieldTermStructure> yieldCurve() { return nominalTermStructure(); }
    Handle<CPIVolatilitySurface> volatility() { return capletVolatility(); }
    boost::shared_ptr<PricingEngine> engine() { return engine_; }

private:
    // engine to price the underlying cap/floor
    boost::shared_ptr<PricingEngine> engine_;
};

} // namespace QuantExt

#endif
