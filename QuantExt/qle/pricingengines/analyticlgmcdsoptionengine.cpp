/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/analyticlgmcdsoptionengine.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/math/solvers1d/brent.hpp>

namespace QuantExt {

AnalyticLgmCdsOptionEngine::AnalyticLgmCdsOptionEngine(const boost::shared_ptr<CrossAssetModel>& model,
                                                       const Size index, const Size ccy, const Real recoveryRate,
                                                       const Handle<YieldTermStructure>& termStructure)
    : QuantExt::CdsOption::engine(), model_(model), index_(index), ccy_(ccy), recoveryRate_(recoveryRate),
      termStructure_(termStructure) {
    registerWith(model);
    if (!termStructure.empty())
        registerWith(termStructure);
}

void AnalyticLgmCdsOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.swap->paysAtDefaultTime(), "AnalyticLgmCdsOptionEngine: pays at default time must be true");

    Real w = (arguments_.side == Protection::Buyer) ? -1.0 : 1.0;
    Rate swapSpread = arguments_.swap->runningSpread();
    const Handle<YieldTermStructure>& yts =
        termStructure_.empty() ? model_->irlgm1f(0)->termStructure() : termStructure_;

    Real riskyAnnuity = std::fabs(arguments_.swap->couponLegNPV() / swapSpread);
    results_.riskyAnnuity = riskyAnnuity;

    // incorporate upfront amount

    swapSpread -= w * arguments_.swap->upfrontNPV() / riskyAnnuity;

    Size n = arguments_.swap->coupons().size();
    t_ = Array(n + 1, 0.0);
    G_ = Array(n + 1, 0.0);
    Array C(n, 0.0), D(n, 0.0);

    if (arguments_.exercise->date(0) <= yts->referenceDate()) {
        results_.value = 0.0;
        return;
    }

    tex_ = yts->timeFromReference(arguments_.exercise->date(0));
    t_[0] = std::max(tex_, yts->timeFromReference(arguments_.swap->protectionStartDate()));

    Real accrualSettlementAmount = 0.0;
    for (Size i = 0; i < n; ++i) {
        boost::shared_ptr<FixedRateCoupon> cpn =
            boost::dynamic_pointer_cast<FixedRateCoupon>(arguments_.swap->coupons()[i]);
        QL_REQUIRE(cpn != NULL, "AnalyticLgmCdsOptionEngine: expected fixed rate coupon");
        t_[i + 1] = yts->timeFromReference(cpn->date());
        Real mid = (t_[i] + t_[i + 1]) / 2.0;
        if (arguments_.swap->settlesAccrual()) {
            Real accStartTime = i == 0 ? yts->timeFromReference(cpn->accrualStartDate()) : t_[i];
            // mid > accStartTime practically always the case?
            accrualSettlementAmount = mid > accStartTime ? swapSpread * cpn->accrualPeriod() * (mid - accStartTime) /
                                                               (t_[i + 1] - accStartTime)
                                                         : 0.0;
        }
        C[i] = ((1.0 - recoveryRate_) - accrualSettlementAmount) * yts->discount(mid) / yts->discount(tex_);
        D[i] = swapSpread * cpn->accrualPeriod() * yts->discount(t_[i + 1]) / yts->discount(tex_);
    }
    G_[0] = -C[0];
    for (Size i = 0; i < n - 1; ++i) {
        G_[i + 1] = C[i] + D[i] - C[i + 1];
    }
    G_[n] = C[n - 1] + D[n - 1];

    // if a non knock-out payer option, add front end protection value
    Real frontEndProtection = 0.0;
    if (arguments_.side == Protection::Buyer && !arguments_.knocksOut) {
        frontEndProtection = arguments_.swap->notional() * (1. - recoveryRate_) *
                             model_->crlgm1f(index_)->termStructure()->defaultProbability(tex_) * yts->discount(tex_);
    }

    Brent b;
    Real lambdaStar;
    try {
        lambdaStar = b.solve(boost::bind(&AnalyticLgmCdsOptionEngine::lambdaStarHelper, this, _1), 1.0E-6, 0.0, 0.01);
    } catch (const std::exception& e) {
        QL_FAIL("AnalyticLgmCdsOptionEngine, failed to compute lambdaStar, " << e.what());
    }

    Real sum = 0.0;
    for (Size i = 1; i < G_.size(); ++i) {
        Real strike = model_->crlgm1fS(index_, ccy_, tex_, t_[i], lambdaStar, 0.0).second /
                      model_->crlgm1fS(index_, ccy_, tex_, t_[0], lambdaStar, 0.0).second;
        sum += G_[i] * Ei(w, strike, i) * yts->discount(tex_);
    }

    results_.value = arguments_.swap->notional() * sum + frontEndProtection;

} // calculate

Real AnalyticLgmCdsOptionEngine::Ei(const Real w, const Real strike, const Size i) const {
    Real pS = model_->crlgm1f(index_)->termStructure()->survivalProbability(t_[0]);
    Real pT = model_->crlgm1f(index_)->termStructure()->survivalProbability(t_[i]);
    // slight generalization of Lichters, Stamm, Gallagher 11.2.1
    // with t < S, SSRN: https://ssrn.com/abstract=2246054
    Real sigma = sqrt(model_->crlgm1f(index_)->zeta(tex_)) *
                 (model_->crlgm1f(index_)->H(t_[i]) - model_->crlgm1f(index_)->H(t_[0]));
    Real dp = (std::log(pT / (strike * pS)) / sigma + 0.5 * sigma);
    Real dm = dp - sigma;
    CumulativeNormalDistribution N;
    return w * (pT * N(w * dp) - pS * strike * N(w * dm));
}

Real AnalyticLgmCdsOptionEngine::lambdaStarHelper(const Real lambda) const {
    Real sum = 0.0;
    for (Size i = 0; i < G_.size(); ++i) {
        Real S = model_->crlgm1fS(index_, ccy_, tex_, t_[i], lambda, 0.0).second /
                 model_->crlgm1fS(index_, ccy_, tex_, t_[0], lambda, 0.0).second;
        sum += G_[i] * S;
    }
    return sum;
}

} // namespace QuantExt
