/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <qle/pricingengines/discountingswapenginedelta.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/instruments/vanillaswap.hpp>

namespace QuantExt {

DiscountingSwapEngineDelta::DiscountingSwapEngineDelta(const Handle<YieldTermStructure>& discountCurve,
                                                       const std::vector<Time>& deltaTimes)
    : discountCurve_(discountCurve), deltaTimes_(deltaTimes) {
    registerWith(discountCurve_);
}

namespace {

class NpvDeltaCalculator : public AcyclicVisitor,
                           public Visitor<CashFlow>,
                           public Visitor<FixedRateCoupon>,
                           public Visitor<IborCoupon> {
private:
    Handle<YieldTermStructure> discountCurve_;
    const Real payer_;
    Real& npv_;
    std::map<Date, Real> &deltaDiscount_, &deltaForward_;

public:
    NpvDeltaCalculator(Handle<YieldTermStructure> discountCurve, const Real payer, Real& npv,
                       std::map<Date, Real>& deltaDiscount, std::map<Date, Real>& deltaForward)
        : discountCurve_(discountCurve), payer_(payer), npv_(npv), deltaDiscount_(deltaDiscount),
          deltaForward_(deltaForward) {}
    void visit(CashFlow& c);
    void visit(FixedRateCoupon& c);
    void visit(IborCoupon& c);
};

void NpvDeltaCalculator::visit(CashFlow& c) { npv_ += c.amount() * discountCurve_->discount(c.date()); }

void NpvDeltaCalculator::visit(FixedRateCoupon& c) {
    Real a = payer_ * c.amount() * discountCurve_->discount(c.date());
    Real t = discountCurve_->timeFromReference(c.date());
    npv_ += a;
    deltaDiscount_[c.date()] += -t * a;
}

void NpvDeltaCalculator::visit(IborCoupon& c) {
    Real a = payer_ * c.amount() * discountCurve_->discount(c.date());
    npv_ += a;
    Date d3 = c.date();
    Real t3 = discountCurve_->timeFromReference(d3);
    deltaDiscount_[d3] = -t3 * a;
    Date fixing = c.fixingDate();
    if (fixing > discountCurve_->referenceDate()) {
        Date d1 = c.index()->valueDate(fixing);
        Date d2 = c.index()->maturityDate(d1);
        Real t1 = discountCurve_->timeFromReference(d1);
        Real t2 = discountCurve_->timeFromReference(d2);
        deltaForward_[d1] += -t1 * a;
        deltaForward_[d2] += -t2 * a;
    }
}

} // anonymous namespace

void DiscountingSwapEngineDelta::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");

    results_.value = 0.0;
    results_.errorEstimate = Null<Real>();
    results_.legNPV.resize(arguments_.legs.size());

    std::map<Date, Real> deltaDiscountRaw, deltaForwardRaw;

    for (Size j = 0; j < arguments_.legs.size(); ++j) {
        Real npv = 0.0;
        NpvDeltaCalculator calc(discountCurve_, arguments_.payer[j], npv, deltaDiscountRaw, deltaForwardRaw);
        Leg& leg = arguments_.legs[j];
        for (Size i = 0; i < leg.size(); ++i) {
            CashFlow& cf = *leg[i];
            if (cf.date() <= discountCurve_->referenceDate()) {
                continue;
            }
            cf.accept(calc);
        }
        results_.legNPV[j] = npv;
        results_.value += npv;
    }

    results_.additionalResults["deltaDiscountRaw"] = deltaDiscountRaw;
    results_.additionalResults["deltaForwardRaw"] = deltaForwardRaw;
}

} // namespace QuantExt
