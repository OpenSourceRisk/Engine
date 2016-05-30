/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*
 Copyright (C) 2007, 2009 StatPro Italia srl
 Copyright (C) 2011 Ferdinando Ametrano

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

/*! \file discountingswapengine.hpp
    \brief discounting swap engine supporting simulated fixings,
           includes a par coupon approximation for ibor coupons,
           if used today's fixing will always be estimated on
           the curve for ibor coupons
*/

#ifndef quantext_discounting_swap_engine_hpp
#define quantext_discounting_swap_engine_hpp

#include <qle/indexes/simulatedfixingsmanager.hpp>

#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/handle.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! This version of the engine supports simulated fixings.
  It computes only the NPV, no BPS or start-, end-discounts,
  since during simulation we are in general not interested
  in these additional results.
  The assumption is that fixings are only relevant for
  cashflows instances of type FloatingRateCoupon or
  CappedFlooredCoupon, which should cover all relevant
  cases in the standard QuantLib. */
class DiscountingSwapEngine : public Swap::engine {
  public:
    // ctor with fixed settlement and npv date
    DiscountingSwapEngine(
        const Handle<YieldTermStructure> &discountCurve =
            Handle<YieldTermStructure>(),
        boost::optional<bool> includeSettlementDateFlows = boost::none,
        Date settlementDate = Date(), Date npvDate = Date(),
        bool optimized = true, bool parApproximation = true,
        Size gracePeriod = 5);
    // ctor with floating settlement and npv date lags
    DiscountingSwapEngine(const Handle<YieldTermStructure> &discountCurve,
                          boost::optional<bool> includeSettlementDateFlows,
                          Period settlementDateLag, Period npvDateLag,
                          Calendar calendar, bool optimized = true,
                          bool parApproximation = true, Size gracePeriod = 5);
    void calculate() const;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; }

    void flushCache() { arguments_cache_.clear(); }

  private:
    Handle<YieldTermStructure> discountCurve_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_, npvDate_;
    Period settlementDateLag_, npvDateLag_;
    Calendar calendar_;
    bool optimized_, parApproximation_;
    Size gracePeriod_;
    bool floatingLags_;
    // FIXME: remove cache
    typedef std::map<void *, boost::tuple<bool, bool, bool, bool,
                                          IborCoupon const *> > ArgumentsCache;
    mutable ArgumentsCache arguments_cache_;
};

Real simulatedFixingsNpv(const Leg &leg,
                         const YieldTermStructure &discountCurve,
                         const bool includeSettlementDateFlows,
                         const Date &settlementDate, const Date &today,
                         const bool vanilla, const bool nullSpread,
                         const bool equalDayCounters,
                         const bool parApprxomiation);

} // namespace QuantExt

#endif
