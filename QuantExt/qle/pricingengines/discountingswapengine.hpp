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
        bool parApproximation = true, Size gracePeriod = 5);
    // ctor with floating settlement and npv date lags
    DiscountingSwapEngine(const Handle<YieldTermStructure> &discountCurve,
                          boost::optional<bool> includeSettlementDateFlows,
                          Period settlementDateLag, Period npvDateLag,
                          Calendar calendar, bool parApproximation = true,
                          Size gracePeriod = 5);
    void calculate() const;
    Handle<YieldTermStructure> discountCurve() const { return discountCurve_; }

    void flushCache() { arguments_cache_.clear(); }

  private:
    Real npv(const Leg &leg, const YieldTermStructure &discountCurve,
             const bool includeSettlementDateFlows, const Date &settlementDate,
             const Date &today, const bool vanilla, const bool nullSpread,
             const bool equalDayCounters, const bool parApprxomiation) const;
    Handle<YieldTermStructure> discountCurve_;
    boost::optional<bool> includeSettlementDateFlows_;
    Date settlementDate_, npvDate_;
    Period settlementDateLag_, npvDateLag_;
    Calendar calendar_;
    bool parApproximation_;
    Size gracePeriod_;
    bool floatingLags_;
    // FIXME: remove cache
    typedef std::map<void *, boost::tuple<bool, bool, bool, bool,
                                          IborCoupon const *> > ArgumentsCache;
    mutable ArgumentsCache arguments_cache_;
};

// coupon handling

namespace {

class AmountGetter : public AcyclicVisitor,
                     public Visitor<CashFlow>,
                     public Visitor<Coupon>,
                     public Visitor<FloatingRateCoupon>,
                     public Visitor<CappedFlooredCoupon>,
                     public Visitor<IborCoupon> {
  private:
    const Date &today_;
    const bool enforcesTodaysHistoricFixings_, vanilla_, nullSpread_,
        equalDayCounters_, parApproximation_;
    Real amount_;
    std::string indexNameBefore_;
    Real fixing(const Date &fixingDate, const InterestRateIndex &index);
    Real pastFixing(const Date &fixingDate, const InterestRateIndex &index);
    void addBackwardFixing(const InterestRateIndex &index);

  public:
    AmountGetter(const Date &today, const bool enforcesTodaysHistoricFixings,
                 const bool vanilla, const bool nullSpread,
                 const bool equalDayCounters, const bool parApproximation)
        : today_(today),
          enforcesTodaysHistoricFixings_(enforcesTodaysHistoricFixings),
          vanilla_(vanilla), nullSpread_(nullSpread),
          equalDayCounters_(equalDayCounters),
          parApproximation_(parApproximation), amount_(0.0),
          indexNameBefore_("") {}
    Real amount() const { return amount_; }
    void visit(CashFlow &c);
    void visit(Coupon &c);
    void visit(FloatingRateCoupon &c);
    void visit(CappedFlooredCoupon &c);
    void visit(IborCoupon &c);
};

inline void AmountGetter::visit(CashFlow &c) { amount_ = c.amount(); }

inline void AmountGetter::visit(Coupon &c) { amount_ = c.amount(); }

inline void AmountGetter::visit(FloatingRateCoupon &c) {
    addBackwardFixing(*c.index());
    amount_ = (c.gearing() * fixing(c.fixingDate(), *c.index()) + c.spread()) *
              c.accrualPeriod() * c.nominal();
}

inline void AmountGetter::visit(CappedFlooredCoupon &c) {
    addBackwardFixing(*c.index());
    Real effFixing =
        (c.gearing() * fixing(c.fixingDate(), *c.index()) + c.spread());
    if (c.isFloored())
        effFixing = std::max(c.floor(), effFixing);
    if (c.isCapped())
        effFixing = std::min(c.cap(), effFixing);
    amount_ = effFixing * c.accrualPeriod() * c.nominal();
}

inline void AmountGetter::visit(IborCoupon &c) {
    if (!vanilla_) {
        addBackwardFixing(*c.index());
        amount_ =
            (c.gearing() * fixing(c.fixingDate(), *c.index()) + c.spread()) *
            c.accrualPeriod() * c.nominal();
    } else {
        // backward fixing was added in calculate method once and for all
        if (parApproximation_) {
            Real tmp = 0.0;
            if (c.fixingDate() < today_ /*||
                                          (c.fixingDate() == today_ && enforcesTodaysHistoricFixings_)*/) {
                tmp = pastFixing(c.fixingDate(), *c.index());
                if (!nullSpread_) {
                    tmp += c.spread();
                }
                tmp *= c.gearing();
                amount_ = tmp * c.accrualPeriod() * c.nominal();
                return;
            }
            // par coupon approximation, we assume that the accrual
            // period is equal to the index estimation period
            // note that the fixing days in the index are the same
            // as in the coupon since we have a vanilla swap
            boost::shared_ptr<IborIndex> ii =
                boost::static_pointer_cast<IborIndex>(c.index());
            Handle<YieldTermStructure> termStructure =
                ii->forwardingTermStructure();
            Date d1 = c.accrualStartDate();
            Date d2 = c.accrualEndDate();
            DiscountFactor disc1 = termStructure->discount(d1);
            DiscountFactor disc2 = termStructure->discount(d2);
            tmp = (disc1 / disc2 - 1.0);
            Real ti = c.accrualPeriod();
            if (!equalDayCounters_) {
                ti = c.index()->dayCounter().yearFraction(d1, d2);
                tmp *= c.accrualPeriod() / ti;
            }
            if (SimulatedFixingsManager::instance().estimationMethod() !=
                SimulatedFixingsManager::Backward) {
                SimulatedFixingsManager::instance().addForwardFixing(
                    c.index()->name(), c.fixingDate(), tmp / ti);
            }
            if (!nullSpread_) {
                tmp += c.accrualPeriod() * c.spread();
            }
            tmp *= c.gearing();
            amount_ = tmp * c.nominal();
        } else {
            amount_ = fixing(c.fixingDate(), *c.index());
            if (!nullSpread_) {
                amount_ += c.spread();
            }
            amount_ *= c.gearing() * c.accrualPeriod() * c.nominal();
        }
    }
}

inline void AmountGetter::addBackwardFixing(const InterestRateIndex &index) {
    if (index.name() != indexNameBefore_ &&
        !SimulatedFixingsManager::instance().hasBackwardFixing(index.name())) {
        Real value =
            index.fixing(index.fixingCalendar().adjust(today_, Following));
        SimulatedFixingsManager::instance().addBackwardFixing(index.name(),
                                                              value);
        indexNameBefore_ = index.name();
    }
}

inline Real AmountGetter::fixing(const Date &fixingDate,
                                 const InterestRateIndex &index) {
    Real fixing = 0.0;
    // is it a past fixing ?
    if (fixingDate < today_ /*||
                              (fixingDate == today_ && enforcesTodaysHistoricFixings_)*/) {
        return pastFixing(fixingDate, index);
    } else {
        // no past fixing, so forecast fixing (or in case
        // of todays fixing, read possibly the actual
        // fixing)
        fixing = index.fixing(fixingDate);
        // add the fixing to the simulated fixing data
        if (SimulatedFixingsManager::instance().estimationMethod() !=
            SimulatedFixingsManager::Backward) {
            SimulatedFixingsManager::instance().addForwardFixing(
                index.name(), fixingDate, fixing);
        }
    }
    return fixing;
} // AmountGetter::fixing()

inline Real AmountGetter::pastFixing(const Date &fixingDate,
                                     const InterestRateIndex &index) {
    Real nativeFixing = index.timeSeries()[fixingDate], fixing;
    if (nativeFixing != Null<Real>())
        fixing = nativeFixing;
    else
        fixing = SimulatedFixingsManager::instance().simulatedFixing(
            index.name(), fixingDate);
    QL_REQUIRE(fixing != Null<Real>(),
               "Missing " << index.name() << " fixing for " << fixingDate
                          << " (even when considering "
                             "simulated fixings)");
    return fixing;
} // AmountGetter::pastFixing()

} // anonymous namespace

// inline

inline Real DiscountingSwapEngine::npv(
    const Leg &leg, const YieldTermStructure &discountCurve,
    const bool includeSettlementDateFlows, const Date &settlementDate,
    const Date &today, const bool vanilla, const bool nullSpread,
    const bool equalDayCounters, const bool parApproximation) const {

    Real npv = 0.0;
    if (leg.empty()) {
        return npv;
    }

    const bool enforcesTodaysHistoricFixings =
        Settings::instance().enforcesTodaysHistoricFixings();
    QL_REQUIRE(!enforcesTodaysHistoricFixings,
               "enforcesTodaysHistoricFixings not supported");

    AmountGetter amountGetter(today, enforcesTodaysHistoricFixings, vanilla,
                              nullSpread, equalDayCounters, parApproximation);
    for (Size i = 0; i < leg.size(); ++i) {
        CashFlow &cf = *leg[i];
        if (cf.hasOccurred(settlementDate, includeSettlementDateFlows) ||
            cf.tradingExCoupon(settlementDate)) {
            continue;
        }
        Real tmp = 0.0;
        Real df = discountCurve.discount(cf.date());
        if (!SimulatedFixingsManager::instance().simulateFixings()) {
            tmp = cf.amount();
        } else {
            cf.accept(amountGetter);
            tmp = amountGetter.amount();
        }
        npv += tmp * df;
    }
    return npv;
}

} // namespace QuantExt

#endif
