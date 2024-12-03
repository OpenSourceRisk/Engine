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
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>

#include <ql/pricingengines/blackcalculator.hpp>

namespace QuantExt {

using namespace CrossAssetAnalytics;

AnalyticCcLgmFxOptionEngine::AnalyticCcLgmFxOptionEngine(const QuantLib::ext::shared_ptr<CrossAssetModel>& model,
                                                         const Size foreignCurrency)
    : model_(model), foreignCurrency_(foreignCurrency), cacheEnabled_(false), cacheDirty_(true) {}

Real AnalyticCcLgmFxOptionEngine::value(const Time t0, const Time t,
                                        const QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff,
                                        const Real domesticDiscount, const Real fxForward) const {

    const auto lgm0 = model_->irlgm1f(0);
    const auto lgmi1 = model_->irlgm1f(foreignCurrency_ + 1);

    Real H0 = lgm0->H(t);
    Real Hi1 = lgmi1->H(t);
    Real rzz0i1 = model_->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::IR,
                                      foreignCurrency_ + 1, 0, 0);
    Real rzx0i =
        model_->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::FX, foreignCurrency_, 0, 0);
    Real rzxi1i = model_->correlation(CrossAssetModel::AssetType::IR, foreignCurrency_ + 1,
                                      CrossAssetModel::AssetType::FX, foreignCurrency_, 0, 0);

    if (cacheDirty_ || !cacheEnabled_ || !(close_enough(cachedT0_, t0) && close_enough(cachedT_, t))) {

        cachedIntegrals_ =
            // first term (pt 1 of 2)
            H0 * H0 * (lgm0->zeta(t) - lgm0->zeta(t0)) +
            // second term (pt 1 of 2)
            Hi1 * Hi1 * (lgmi1->zeta(t) - lgmi1->zeta(t0)) +
            model_->integrator()->operator()(
                [&lgm0, &lgmi1, rzz0i1, H0, Hi1](const Real t) {
                    Real a0 = lgm0->alpha(t);
                    Real ai1 = lgmi1->alpha(t);
                    Real H0t = lgm0->H(t);
                    Real Hi1t = lgmi1->H(t);
                    return
                        // first term (pt 2 of 2)
                        -2.0 * H0 * H0t * a0 * a0 + H0t * H0t * a0 * a0 -
                        // second term (pt 2 of 2)
                        2.0 * Hi1 * Hi1t * ai1 * ai1 + Hi1t * Hi1t * ai1 * ai1 -
                        // third term
                        2.0 * (H0 * Hi1 * a0 * ai1 * rzz0i1 - H0 * Hi1t * ai1 * a0 * rzz0i1 -
                               Hi1 * H0t * a0 * ai1 * rzz0i1 + H0t * Hi1t * a0 * ai1 * rzz0i1);
                },
                t0, t);

        cacheDirty_ = false;
        cachedT0_ = t0;
        cachedT_ = t;
    }

    const auto fxi = model_->fxbs(foreignCurrency_);

    Real variance = cachedIntegrals_ + (fxi->variance(t) - fxi->variance(t0)) +
                    model_->integrator()->operator()(
                        [&lgm0, &lgmi1, &fxi, rzx0i, rzxi1i, H0, Hi1](const Real t) {
                            Real a0 = lgm0->alpha(t);
                            Real ai1 = lgmi1->alpha(t);
                            Real si = fxi->sigma(t);
                            Real H0t = lgm0->H(t);
                            Real Hi1t = lgmi1->H(t);
                            return
                                // forth term
                                2.0 * (H0 * a0 * si * rzx0i - H0t * a0 * si * rzx0i) -
                                // fifth term
                                2.0 * (Hi1 * ai1 * si * rzxi1i - Hi1t * ai1 * si * rzxi1i);
                        },
                        t0, t);

    std::cout << cachedIntegrals_ << " " << variance-cachedIntegrals_ << std::endl;

    BlackCalculator black(payoff, fxForward, std::sqrt(variance), domesticDiscount);

    return black.value();
}

void AnalyticCcLgmFxOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "only European options are allowed");

    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff =
        QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    QL_REQUIRE(payoff != NULL, "only striked payoff is allowed");

    Date expiry = arguments_.exercise->lastDate();
    Time t = model_->irlgm1f(0)->termStructure()->timeFromReference(expiry);

    if (t <= 0.0) {
        // option is expired, we do not value any possibly non settled
        // flows, i.e. set the npv to zero in this case
        results_.value = 0.0;
        return;
    }

    Real foreignDiscount = model_->irlgm1f(foreignCurrency_ + 1)->termStructure()->discount(expiry);
    Real domesticDiscount = model_->irlgm1f(0)->termStructure()->discount(expiry);

    Real fxForward = model_->fxbs(foreignCurrency_)->fxSpotToday()->value() * foreignDiscount / domesticDiscount;

    results_.value = value(0.0, t, payoff, domesticDiscount, fxForward);

} // calculate()

} // namespace QuantExt
