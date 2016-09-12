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
    Real dsc = discountCurve_->discount(c.date());
    Real a = payer_ * c.amount() * dsc;
    npv_ += a;
    Date d3 = c.date();
    Real t3 = discountCurve_->timeFromReference(d3);
    deltaDiscount_[d3] += -t3 * a;
    Date fixing = c.fixingDate();
    if (fixing > discountCurve_->referenceDate() ||
        (fixing == discountCurve_->referenceDate() && c.index()->pastFixing(fixing) == Null<Real>())) {
        Date d1 = c.index()->valueDate(fixing);
#ifdef QL_USE_INDEXED_COUPON
        Date d2 = c.index()->maturityDate(d1);
#else
        Date d2;
        // in arrears?
        if (fixing > c.accrualStartDate()) {
            d2 = c.index()->maturityDate(d1);
        } else {
            // par coupon approximation
            Date nextFixingDate =
                c.index()->fixingCalendar().advance(c.accrualEndDate(), -static_cast<Integer>(c.fixingDays()), Days);
            d2 = c.index()->fixingCalendar().advance(nextFixingDate, c.fixingDays(), Days);
        }
#endif
        Real t1 = discountCurve_->timeFromReference(d1);
        Real t2 = discountCurve_->timeFromReference(d2);
        Real r =
            payer_ * dsc * c.nominal() * c.accrualPeriod() * c.gearing() / c.index()->dayCounter().yearFraction(d1, d2);
        deltaForward_[d1] += -t1 * (a + r);
        deltaForward_[d2] += t2 * (a + r);
    }
}

} // anonymous namespace

void DiscountingSwapEngineDelta::calculate() const {
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");

    // compute npv and raw deltas

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

    // convert raw deltas to given bucketing structure

    if (deltaTimes_.empty())
        return;

    Size m = deltaTimes_.size();
    Size nd = deltaDiscountRaw.size();
    Size nf = deltaForwardRaw.size();

    Matrix dzds_d(nd, m, 0.0), dzds_f(nf, m, 0.0);

    dzds(dzds_d, deltaDiscountRaw);
    dzds(dzds_f, deltaForwardRaw);

    std::vector<Real> deltaDiscount(deltaTimes_.size(), 0.0), deltaForward(deltaTimes_.size(), 0.0);
    for (Size j = 0; j < deltaTimes_.size(); ++j) {
        Size i = 0;
        for (std::map<Date, Real>::const_iterator it = deltaDiscountRaw.begin(); it != deltaDiscountRaw.end();
             ++it, ++i) {
            deltaDiscount[j] += it->second * dzds_d[i][j];
        }
        i = 0;
        for (std::map<Date, Real>::const_iterator it = deltaForwardRaw.begin(); it != deltaForwardRaw.end();
             ++it, ++i) {
            deltaForward[j] += it->second * dzds_f[i][j];
        }
    }

    results_.additionalResults["deltaTimes"] = deltaTimes_;
    results_.additionalResults["deltaDiscount"] = deltaDiscount;
    results_.additionalResults["deltaForward"] = deltaForward;
}

void DiscountingSwapEngineDelta::dzds(Matrix& result, const std::map<Date, Real>& delta) const {
    Size row = 0;
    for (std::map<Date, Real>::const_iterator i = delta.begin(); i != delta.end(); ++i, ++row) {
        Real t = discountCurve_->timeFromReference(i->first);
        Size b = std::upper_bound(deltaTimes_.begin(), deltaTimes_.end(), t) - deltaTimes_.begin();
        if (b == 0) {
            result[row][0] = 1.0;
            continue;
        }
        if (b == deltaTimes_.size()) {
            result[row][result.columns() - 1] = 1.0;
            continue;
        }
        result[row][b - 1] = (deltaTimes_[b] - t) / (deltaTimes_[b] - deltaTimes_[b - 1]);
        result[row][b] = 1.0 - result[row][b - 1];
    }
}

} // namespace QuantExt
