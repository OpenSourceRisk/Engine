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
    \brief discounting swap engine supporting simulated fixings
*/

#ifndef quantext_discounting_swap_engine_hpp
#define quantext_discounting_swap_engine_hpp

#include <qle/indexes/simulatedfixingsmanager.hpp>

#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/instruments/swap.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
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
        DiscountingSwapEngine(
               const Handle<YieldTermStructure>& discountCurve =
                                                 Handle<YieldTermStructure>(),
               boost::optional<bool> includeSettlementDateFlows = boost::none,
               Date settlementDate = Date(),
               Date npvDate = Date());
        void calculate() const;
        Handle<YieldTermStructure> discountCurve() const {
            return discountCurve_;
        }
      private:
        Real npv(const Leg &leg, const YieldTermStructure &discountCurve,
                 bool includeSettlementDateFlows, Date settlementDate,
                 Date npvDate) const;
        Handle<YieldTermStructure> discountCurve_;
        boost::optional<bool> includeSettlementDateFlows_;
        Date settlementDate_, npvDate_;
    };

    // inline

    inline Real DiscountingSwapEngine::npv(
        const Leg &leg, const YieldTermStructure &discountCurve,
        bool includeSettlementDateFlows, Date settlementDate, Date npvDate) const {

        Real npv = 0.0;
        if (leg.empty()) {
            return npv;
        }

        Date today = Settings::instance().evaluationDate();
        bool simulateFixings =
            SimulatedFixingsManager::instance().simulateFixings();
        bool enforceTodaysHistoricFixings =
            Settings::instance().enforcesTodaysHistoricFixings();

        for (Size i = 0; i < leg.size(); ++i) {
            
            CashFlow &cf = *leg[i];
            if (!cf.hasOccurred(settlementDate, includeSettlementDateFlows) &&
                !cf.tradingExCoupon(settlementDate)) {
                
                Real df = discountCurve.discount(cf.date());
                Real tmp = 0.0;
                boost::shared_ptr<FloatingRateCoupon> cp =
                    boost::dynamic_pointer_cast<FloatingRateCoupon>(leg[i]);
                boost::shared_ptr<CappedFlooredCoupon> cpcf =
                    boost::dynamic_pointer_cast<CappedFlooredCoupon>(leg[i]);

                // is the cashflow a floating rate coupon and do we
                // simulate fixings ? Otherwise just take the cashflow
                // amount below.
                if ((cp != NULL || cpcf != NULL) && simulateFixings) {
                    Date fixingDate = cp->fixingDate();
                    boost::shared_ptr<InterestRateIndex> index = cp->index();
                    Real fixing = 0.0;
                    // is it a past fixing ?
                    if (fixingDate < today ||
                        (fixingDate == today && enforceTodaysHistoricFixings)) {
                        Real nativeFixing = index->timeSeries()[fixingDate];
                        if (nativeFixing != Null<Real>() || !simulateFixings)
                            fixing = nativeFixing;
                        else
                            fixing =
                                SimulatedFixingsManager::instance()
                                    .simulatedFixing(index->name(), fixingDate);
                        QL_REQUIRE(fixing != Null<Real>(),
                                   "Missing " << index->name() << " fixing for "
                                              << fixingDate
                                              << " (even when considering "
                                                 "simulated fixings)");
                        // amount calculation
                        Real effFixing = 0.0;
                        if (cp != NULL) {
                            effFixing =
                                (cp->gearing() * fixing + cp->spread());
                        }
                        if (cpcf != NULL) {
                            effFixing =
                                (cpcf->gearing() * fixing + cpcf->spread());
                            if (cpcf->isFloored())
                                effFixing = std::max(cpcf->floor(), effFixing);
                            if (cpcf->isCapped())
                                effFixing = std::min(cpcf->cap(), effFixing);
                        }
                        tmp = effFixing * cp->accrualPeriod() * cp->nominal();
                    } else {
                        // no past fixing, so forecast fixing
                        fixing = index->fixing(fixingDate);
                        // add the fixing to the simulated fixing data
                        if (SimulatedFixingsManager::instance()
                                .simulateFixings()) {
                            SimulatedFixingsManager::instance()
                                .addSimulatedFixing(index->name(), fixingDate,
                                                    fixing);
                        }
                        // finally, compute the amount in the usual way
                        tmp = cf.amount();
                    }
                } else {
                    // no floating rate coupon or we do not simulate fixings
                    tmp = cf.amount();
                }
                npv += tmp * df;
            }
        }
        DiscountFactor d = discountCurve.discount(npvDate);
        npv /= d;
        return npv;
    }

} // namespace QuantExt

#endif
