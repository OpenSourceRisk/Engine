/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/cashflows/couponpricer.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/subperiodscouponpricer.hpp>

namespace QuantExt {

    namespace {

        class PricerSetter : public AcyclicVisitor,
            public Visitor<CashFlow>,
            public Visitor<Coupon>,
            public Visitor<AverageONIndexedCoupon>,
            public Visitor<SubPeriodsCoupon> {
          private:
            const boost::shared_ptr<FloatingRateCouponPricer> pricer_;
          public:
            PricerSetter(const
                boost::shared_ptr<FloatingRateCouponPricer>& pricer)
            : pricer_(pricer) {}

            void visit(CashFlow& c);
            void visit(Coupon& c);
            void visit(AverageONIndexedCoupon& c);
            void visit(SubPeriodsCoupon& c);
        };

        void PricerSetter::visit(CashFlow&) {
            // nothing to do
        }

        void PricerSetter::visit(Coupon&) {
            // nothing to do
        }

        void PricerSetter::visit(AverageONIndexedCoupon& c) {
            const boost::shared_ptr<AverageONIndexedCouponPricer>
                averageONIndexedCouponPricer =
                boost::dynamic_pointer_cast<AverageONIndexedCouponPricer>(
                    pricer_);
            QL_REQUIRE(averageONIndexedCouponPricer,
                "Pricer not compatible with Average ON Indexed coupon");
            c.setPricer(averageONIndexedCouponPricer);
        }

        void PricerSetter::visit(SubPeriodsCoupon& c) {
            const boost::shared_ptr<SubPeriodsCouponPricer>
                subPeriodsCouponPricer =
                boost::dynamic_pointer_cast<SubPeriodsCouponPricer>(
                    pricer_);
            QL_REQUIRE(subPeriodsCouponPricer,
                "Pricer not compatible with sub-periods coupon");
            c.setPricer(subPeriodsCouponPricer);
        }

    } // anonymous namespace

    void setCouponPricer(const Leg& leg,
        const boost::shared_ptr<FloatingRateCouponPricer>& pricer) {
        PricerSetter setter(pricer);
        for (Size i = 0; i < leg.size(); ++i) {
            leg[i]->accept(setter);
        }
    }

    void setCouponPricers(const Leg& leg,
        const std::vector<boost::shared_ptr<FloatingRateCouponPricer> >&
        pricers) {

        Size nCashFlows = leg.size();
        QL_REQUIRE(nCashFlows > 0, "No cashflows");

        Size nPricers = pricers.size();
        QL_REQUIRE(nCashFlows >= nPricers,
            "Mismatch between leg size (" << nCashFlows <<
            ") and number of pricers (" << nPricers << ")");

        for (Size i = 0; i < nCashFlows; ++i) {
            PricerSetter setter(i < nPricers ?
                pricers[i] : pricers[nPricers-1]);
            leg[i]->accept(setter);
        }
    }

} // namesapce QuantExt
