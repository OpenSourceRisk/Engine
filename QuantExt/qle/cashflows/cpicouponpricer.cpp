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

/*! \file cpicouponpricer.cpp
    \brief CPI CashFlow and Coupon pricers that handle caps/floors
*/

#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <qle/cashflows/cpicouponpricer.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>

namespace QuantExt {

InflationCashFlowPricer::InflationCashFlowPricer(const Handle<CPIVolatilitySurface>& vol,
                                                 const Handle<YieldTermStructure>& yts)
    : vol_(vol), yts_(yts) {
    if (!vol_.empty())
        registerWith(vol_);
    if (yts_.empty())
        yts_ = Handle<YieldTermStructure>(
            boost::shared_ptr<YieldTermStructure>(new FlatForward(0, NullCalendar(), 0.05, Actual365Fixed())));
}

BlackCPICashFlowPricer::BlackCPICashFlowPricer(const Handle<CPIVolatilitySurface>& vol,
                                               const Handle<YieldTermStructure>& yts)
    : InflationCashFlowPricer(vol, yts) {
    engine_ = boost::make_shared<CPIBlackCapFloorEngine>(yieldCurve(), volatility());
}

BlackCPICouponPricer::BlackCPICouponPricer(const Handle<CPIVolatilitySurface>& vol,
                                           const Handle<YieldTermStructure>& yts)
    : CPICouponPricer(vol, yts) {
    if (nominalTermStructure_.empty())
        nominalTermStructure_ = Handle<YieldTermStructure>(
            boost::shared_ptr<YieldTermStructure>(new FlatForward(0, NullCalendar(), 0.05, Actual365Fixed())));
    engine_ = boost::make_shared<CPIBlackCapFloorEngine>(yieldCurve(), volatility());
}

} // namespace QuantExt
