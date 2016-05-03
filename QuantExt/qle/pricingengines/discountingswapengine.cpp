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

#include <qle/pricingengines/discountingswapengine.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/utilities/dataformatters.hpp>

#include <boost/tuple/tuple.hpp>

namespace QuantExt {

DiscountingSwapEngine::DiscountingSwapEngine(
    const Handle<YieldTermStructure> &discountCurve,
    boost::optional<bool> includeSettlementDateFlows, Date settlementDate,
    Date npvDate, bool optimized, bool parApproximation, Size gracePeriod)
    : discountCurve_(discountCurve),
      includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDate_(settlementDate), npvDate_(npvDate), optimized_(optimized),
      parApproximation_(parApproximation), gracePeriod_(gracePeriod),
      floatingLags_(false) {
    registerWith(discountCurve_);
}

DiscountingSwapEngine::DiscountingSwapEngine(
    const Handle<YieldTermStructure> &discountCurve,
    boost::optional<bool> includeSettlementDateFlows, Period settlementDateLag,
    Period npvDateLag, Calendar calendar, bool optimized, bool parApproximation,
    Size gracePeriod)
    : discountCurve_(discountCurve),
      includeSettlementDateFlows_(includeSettlementDateFlows),
      settlementDateLag_(settlementDateLag), npvDateLag_(npvDateLag),
      calendar_(calendar), optimized_(optimized),
      parApproximation_(parApproximation), gracePeriod_(gracePeriod),
      floatingLags_(true) {
    registerWith(discountCurve_);
}

void DiscountingSwapEngine::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(),
               "discounting term structure handle is empty");

    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();

    Date today = discountCurve_->referenceDate();
    Date settlementDate;
    if (floatingLags_) {
        QL_REQUIRE(settlementDateLag_.length() >= 0,
                   "non negative period required");
        settlementDate = calendar_.advance(today, settlementDateLag_);
    } else {
        if (settlementDate_ == Date()) {
            settlementDate = today;
        } else {
            QL_REQUIRE(settlementDate >= today,
                       "settlement date (" << settlementDate
                                           << ") before "
                                              "discount curve reference date ("
                                           << today << ")");
        }
    }

    if (floatingLags_) {
        QL_REQUIRE(npvDateLag_.length() >= 0, "non negative period required");
        results_.valuationDate = calendar_.advance(today, npvDateLag_);
    } else {
        if (npvDate_ == Date()) {
            results_.valuationDate = today;
        } else {
            QL_REQUIRE(npvDate_ >= today,
                       "npv date (" << npvDate_
                                    << ") before "
                                       "discount curve reference date ("
                                    << today << ")");
        }
    }
    results_.npvDateDiscount = discountCurve_->discount(results_.valuationDate);

    Size n = arguments_.legs.size();
    results_.legNPV.resize(n);
    results_.legBPS.resize(n);
    results_.startDiscounts.resize(n);
    results_.endDiscounts.resize(n);

    bool includeRefDateFlows =
        includeSettlementDateFlows_
            ? *includeSettlementDateFlows_
            : Settings::instance().includeReferenceDateEvents();

    // can we simplify things ... :
    // vanilla means 2 legs with one floating leg, one fix leg,
    // floating leg has ibor coupons with same index,
    // in case of a vanilla swap ...
    // ... nullSpread means that all spreads are zero
    // ... equalDayCounters means that the coupon day counter
    // is equal the index day counter
    // ... parApproximation means that the coupon fixing days
    // are equal to the index fixing days (with a grace tol)
    // and that the fixing is in advance

    bool vanilla = true, nullSpread = true, equalDayCounters = true,
         parApproximation = true;
    if (!optimized_) {
        vanilla = nullSpread = equalDayCounters = parApproximation = false;
    } else {
        IborCoupon const *c0;

        ArgumentsCache::const_iterator it =
            arguments_cache_.find(&arguments_.legs);
        if (it != arguments_cache_.end()) {
            vanilla = it->second.get<0>();
            nullSpread = it->second.get<1>();
            equalDayCounters = it->second.get<2>();
            parApproximation = it->second.get<3>();
            c0 = it->second.get<4>();
        } else {

            // determine swap type

            if (arguments_.legs.size() == 2) {
                Size floatIdx = 2;
                c0 = boost::dynamic_pointer_cast<IborCoupon>(
                         arguments_.legs[0].front())
                         .get();
                if (c0 != NULL) {
                    floatIdx = 0;
                } else {
                    c0 = boost::dynamic_pointer_cast<IborCoupon>(
                             arguments_.legs[1].front())
                             .get();
                    if (c0 != NULL) {
                        floatIdx = 1;
                    } else {
                        vanilla = false;
                    }
                }
                if (vanilla) {
                    // check for fixed coupons
                    for (Size i = 0; i < arguments_.legs[1 - floatIdx].size();
                         ++i) {
                        boost::shared_ptr<FixedRateCoupon> c1 =
                            boost::dynamic_pointer_cast<FixedRateCoupon>(
                                arguments_.legs[1].front());
                        if (c1 == NULL) {
                            vanilla = false;
                            break;
                        }
                    }
                    // check for float coupons, same index, same fixing days
                    // also check for null spread and equal day counters
                    parApproximation &=
                        std::abs<int>(
                            static_cast<int>(c0->fixingDays()) -
                            static_cast<int>(c0->index()->fixingDays())) <
                        static_cast<int>(gracePeriod_);
                    parApproximation &= !c0->isInArrears();
                    nullSpread &= close_enough(c0->spread(), 0.0);
                    equalDayCounters &=
                        c0->dayCounter() == c0->index()->dayCounter();
                    if (vanilla) {
                        for (Size i = 1; i < arguments_.legs[floatIdx].size();
                             ++i) {
                            boost::shared_ptr<IborCoupon> c1 =
                                boost::dynamic_pointer_cast<IborCoupon>(
                                    arguments_.legs[floatIdx][i]);
                            vanilla &= (c1 != NULL);
                            if (!vanilla) {
                                break;
                            }
                            vanilla &=
                                (c1->index()->name() == c0->index()->name());
                            if (!vanilla) {
                                break;
                            }
                            parApproximation &=
                                std::abs<int>(
                                    static_cast<int>(c1->fixingDays()) -
                                    static_cast<int>(
                                        c0->index()->fixingDays())) <
                                static_cast<int>(gracePeriod_);
                            parApproximation &= !c1->isInArrears();
                            nullSpread &= close_enough(c1->spread(), 0.0);
                            equalDayCounters &=
                                c1->dayCounter() == c0->index()->dayCounter();
                        }
                    }
                }
            } // two legs
            else {
                vanilla = false;
            }
            if (!vanilla) {
                nullSpread = equalDayCounters = false;
            }
            arguments_cache_.insert(std::make_pair(
                &arguments_.legs,
                boost::make_tuple(vanilla, nullSpread, equalDayCounters,
                                  parApproximation, c0)));
        } // end of swap type detection
    }     // optimized = true

    // compute leg npvs

    for (Size i = 0; i < n; ++i) {
        try {
            const YieldTermStructure &discount_ref = **discountCurve_;
            results_.legNPV[i] =
                simulatedFixingsNpv(arguments_.legs[i], discount_ref,
                                    includeRefDateFlows, settlementDate, today,
                                    vanilla, nullSpread, equalDayCounters,
                                    parApproximation_ && parApproximation) *
                arguments_.payer[i] / results_.npvDateDiscount;

        } catch (std::exception &e) {
            QL_FAIL(io::ordinal(i + 1) << " leg: " << e.what());
        }
        results_.value += results_.legNPV[i];
        // not computed, set to zero anyway
        results_.legBPS[i] = 0.0;
        results_.startDiscounts[i] = 0.0;
        results_.endDiscounts[i] = 0.0;
    }

} // calculate

namespace {

class AmountGetter : public AcyclicVisitor,
                     public Visitor<CashFlow>,
                     public Visitor<Coupon>,
                     public Visitor<FloatingRateCoupon>,
                     public Visitor<CappedFlooredCoupon>,
                     public Visitor<IborCoupon> {
  private:
    const Date &today_;
    const bool /*enforcesTodaysHistoricFixings_,*/ vanilla_, nullSpread_,
        equalDayCounters_, parApproximation_;
    Real amount_;
    std::string indexNameBefore_;
    Real fixing(const Date &fixingDate, const InterestRateIndex &index);
    Real pastFixing(const Date &fixingDate, const InterestRateIndex &index);
    void addBackwardFixing(const InterestRateIndex &index);

  public:
    AmountGetter(
        const Date &today /*, const bool enforcesTodaysHistoricFixings*/,
        const bool vanilla, const bool nullSpread, const bool equalDayCounters,
        const bool parApproximation)
        : today_(today),
          /*enforcesTodaysHistoricFixings_(enforcesTodaysHistoricFixings),*/
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

void AmountGetter::visit(CashFlow &c) { amount_ = c.amount(); }

void AmountGetter::visit(Coupon &c) { amount_ = c.amount(); }

void AmountGetter::visit(FloatingRateCoupon &c) {
    addBackwardFixing(*c.index());
    amount_ = (c.gearing() * fixing(c.fixingDate(), *c.index()) + c.spread()) *
              c.accrualPeriod() * c.nominal();
}

void AmountGetter::visit(CappedFlooredCoupon &c) {
    addBackwardFixing(*c.index());
    Real effFixing =
        (c.gearing() * fixing(c.fixingDate(), *c.index()) + c.spread());
    if (c.isFloored())
        effFixing = std::max(c.floor(), effFixing);
    if (c.isCapped())
        effFixing = std::min(c.cap(), effFixing);
    amount_ = effFixing * c.accrualPeriod() * c.nominal();
}

void AmountGetter::visit(IborCoupon &c) {
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

void AmountGetter::addBackwardFixing(const InterestRateIndex &index) {
    if (index.name() != indexNameBefore_ &&
        !SimulatedFixingsManager::instance().hasBackwardFixing(index.name())) {
        Real value =
            index.fixing(index.fixingCalendar().adjust(today_, Following));
        SimulatedFixingsManager::instance().addBackwardFixing(index.name(),
                                                              value);
        indexNameBefore_ = index.name();
    }
}

Real AmountGetter::fixing(const Date &fixingDate,
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

Real AmountGetter::pastFixing(const Date &fixingDate,
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

Real simulatedFixingsNpv(const Leg &leg,
                         const YieldTermStructure &discountCurve,
                         const bool includeSettlementDateFlows,
                         const Date &settlementDate, const Date &today,
                         const bool vanilla = false,
                         const bool nullSpread = false,
                         const bool equalDayCounters = false,
                         const bool parApproximation = false) {

    Real npv = 0.0;
    if (leg.empty()) {
        return npv;
    }

    // add backward fixing once for all coupons in case of a vanilla swap
    if (vanilla && SimulatedFixingsManager::instance().simulateFixings()) {
        boost::shared_ptr<IborCoupon> c0 =
            boost::dynamic_pointer_cast<IborCoupon>(leg.front());
        if (c0 != NULL &&
            !SimulatedFixingsManager::instance().hasBackwardFixing(
                c0->index()->name())) {
            Real value = c0->index()->fixing(
                c0->index()->fixingCalendar().adjust(today, Following));
            SimulatedFixingsManager::instance().addBackwardFixing(
                c0->index()->name(), value);
        }
    }

    const bool enforcesTodaysHistoricFixings =
        Settings::instance().enforcesTodaysHistoricFixings();
    QL_REQUIRE(!enforcesTodaysHistoricFixings,
               "enforcesTodaysHistoricFixings not supported");

    AmountGetter amountGetter(today, /*enforcesTodaysHistoricFixings,*/ vanilla,
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
