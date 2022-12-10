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

#include <qle/models/crossassetanalytics.hpp>
#include <qle/pricingengines/analyticxassetlgmeqoptionengine.hpp>

#include <ql/pricingengines/blackcalculator.hpp>

namespace QuantExt {

using namespace CrossAssetAnalytics;

AnalyticXAssetLgmEquityOptionEngine::AnalyticXAssetLgmEquityOptionEngine(
    const boost::shared_ptr<CrossAssetModel>& model, const Size eqName, const Size EqCcy)
    : model_(model), eqIdx_(eqName), ccyIdx_(EqCcy) {}

Real AnalyticXAssetLgmEquityOptionEngine::value(const Time t0, const Time t,
                                                const boost::shared_ptr<StrikedTypePayoff> payoff, const Real discount,
                                                const Real eqForward) const {

    const Size& k = eqIdx_;
    const Size& i = ccyIdx_;
    const CrossAssetModel* x = model_.get();

    Real Hi_t = Hz(i).eval(x, t);

    // calculate the full variance. This is the equity analogy to eqn: 12.18 in Lichters,Stamm,Gallagher
    Real variance = 0;
    variance += (vs(k).eval(x, t) - vs(k).eval(x, t0));

    variance += Hi_t * Hi_t * (zetaz(i).eval(x, t) - zetaz(i).eval(x, t0));
    variance -= 2.0 * Hi_t * integral(x, P(Hz(i), az(i), az(i)), t0, t);
    variance += integral(x, P(Hz(i), Hz(i), az(i), az(i)), t0, t);

    variance += 2.0 * Hi_t * integral(x, P(rzs(i, k), ss(k), az(i)), t0, t);
    variance -= 2.0 * integral(x, P(Hz(i), rzs(i, k), ss(k), az(i)), t0, t);

    Real stdev = sqrt(variance);
    BlackCalculator black(payoff, eqForward, stdev, discount);

    return black.value();
}

void AnalyticXAssetLgmEquityOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "only European options are allowed");

    boost::shared_ptr<StrikedTypePayoff> payoff = boost::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    QL_REQUIRE(payoff != NULL, "only striked payoff is allowed");

    Date expiry = arguments_.exercise->lastDate();
    Time t = model_->irlgm1f(0)->termStructure()->timeFromReference(expiry);

    if (t <= 0.0) {
        // option is expired, we do not value any possibly non settled
        // flows, i.e. set the npv to zero in this case
        results_.value = 0.0;
        return;
    }

    Real divDiscount = model_->eqbs(eqIdx_)->equityDivYieldCurveToday()->discount(expiry);
    Real eqIrDiscount = model_->eqbs(eqIdx_)->equityIrCurveToday()->discount(expiry);
    Real cashflowsDiscount = model_->irlgm1f(ccyIdx_)->termStructure()->discount(expiry);

    Real eqForward = model_->eqbs(eqIdx_)->eqSpotToday()->value() * divDiscount / eqIrDiscount;

    results_.value = value(0.0, t, payoff, cashflowsDiscount, eqForward);

} // calculate()

} // namespace QuantExt
