/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/brlcdicouponpricer.hpp>
#include <qle/cashflows/couponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscouponpricer.hpp>

#include <ql/cashflows/overnightindexedcoupon.hpp>

namespace QuantExt {

namespace {

class PricerSetter : public AcyclicVisitor,
                     public Visitor<CashFlow>,
                     public Visitor<Coupon>,
                     public Visitor<QuantLib::OvernightIndexedCoupon>,
                     public Visitor<QuantExt::OvernightIndexedCoupon>,
                     public Visitor<CappedFlooredOvernightIndexedCoupon>,
                     public Visitor<AverageONIndexedCoupon>,
                     public Visitor<QuantExt::SubPeriodsCoupon1> {
private:
    const boost::shared_ptr<FloatingRateCouponPricer> pricer_;

public:
    PricerSetter(const boost::shared_ptr<FloatingRateCouponPricer>& pricer) : pricer_(pricer) {}

    void visit(CashFlow& c) override;
    void visit(Coupon& c) override;
    void visit(QuantLib::OvernightIndexedCoupon& c) override;
    void visit(QuantExt::OvernightIndexedCoupon& c) override;
    void visit(CappedFlooredOvernightIndexedCoupon& c) override;
    void visit(AverageONIndexedCoupon& c) override;
    void visit(QuantExt::SubPeriodsCoupon1& c) override;
};

void PricerSetter::visit(CashFlow&) {
    // nothing to do
}

void PricerSetter::visit(Coupon&) {
    // nothing to do
}

void PricerSetter::visit(QuantLib::OvernightIndexedCoupon& c) {
    // Special pricer for BRL CDI
    boost::shared_ptr<BRLCdi> brlCdiIndex = boost::dynamic_pointer_cast<BRLCdi>(c.index());
    if (brlCdiIndex) {
        const boost::shared_ptr<BRLCdiCouponPricer> brlCdiCouponPricer =
            boost::dynamic_pointer_cast<BRLCdiCouponPricer>(pricer_);
        QL_REQUIRE(brlCdiCouponPricer, "Pricer not compatible with BRL CDI coupon");
        c.setPricer(brlCdiCouponPricer);
    } else {
        c.setPricer(pricer_);
    }
}

void PricerSetter::visit(QuantExt::OvernightIndexedCoupon& c) {
    // Special pricer for BRL CDI
    boost::shared_ptr<BRLCdi> brlCdiIndex = boost::dynamic_pointer_cast<BRLCdi>(c.index());
    if (brlCdiIndex) {
        const boost::shared_ptr<BRLCdiCouponPricer> brlCdiCouponPricer =
            boost::dynamic_pointer_cast<BRLCdiCouponPricer>(pricer_);
        QL_REQUIRE(brlCdiCouponPricer, "Pricer not compatible with BRL CDI coupon");
        c.setPricer(brlCdiCouponPricer);
    } else {
        c.setPricer(pricer_);
    }
}

void PricerSetter::visit(CappedFlooredOvernightIndexedCoupon& c) {
    const boost::shared_ptr<CappedFlooredOvernightIndexedCouponPricer> p =
        boost::dynamic_pointer_cast<CappedFlooredOvernightIndexedCouponPricer>(pricer_);
    // we can set a pricer for the capped floored on coupon or the underlying on coupon
    if (p)
        c.setPricer(p);
    else
        c.underlying()->accept(*this);
}

void PricerSetter::visit(AverageONIndexedCoupon& c) {
    const boost::shared_ptr<AverageONIndexedCouponPricer> averageONIndexedCouponPricer =
        boost::dynamic_pointer_cast<AverageONIndexedCouponPricer>(pricer_);
    QL_REQUIRE(averageONIndexedCouponPricer, "Pricer not compatible with Average ON Indexed coupon");
    c.setPricer(averageONIndexedCouponPricer);
}

void PricerSetter::visit(QuantExt::SubPeriodsCoupon1& c) {
    const boost::shared_ptr<QuantExt::SubPeriodsCouponPricer1> subPeriodsCouponPricer =
        boost::dynamic_pointer_cast<QuantExt::SubPeriodsCouponPricer1>(pricer_);
    QL_REQUIRE(subPeriodsCouponPricer, "Pricer not compatible with sub-periods coupon");
    c.setPricer(subPeriodsCouponPricer);
}
} // namespace

void setCouponPricer(const Leg& leg, const boost::shared_ptr<FloatingRateCouponPricer>& pricer) {
    PricerSetter setter(pricer);
    for (Size i = 0; i < leg.size(); ++i) {
        leg[i]->accept(setter);
    }
}

void setCouponPricers(const Leg& leg, const std::vector<boost::shared_ptr<FloatingRateCouponPricer> >& pricers) {

    Size nCashFlows = leg.size();
    QL_REQUIRE(nCashFlows > 0, "No cashflows");

    Size nPricers = pricers.size();
    QL_REQUIRE(nCashFlows >= nPricers,
               "Mismatch between leg size (" << nCashFlows << ") and number of pricers (" << nPricers << ")");

    for (Size i = 0; i < nCashFlows; ++i) {
        PricerSetter setter(i < nPricers ? pricers[i] : pricers[nPricers - 1]);
        leg[i]->accept(setter);
    }
}
} // namespace QuantExt
