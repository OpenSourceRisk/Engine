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
#include <qle/models/crossassetmodel.hpp>

#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/processes/eulerdiscretization.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

using namespace CrossAssetAnalytics;

CrossAssetStateProcess::CrossAssetStateProcess(const CrossAssetModel* const model, discretization disc,
                                               SalvagingAlgorithm::Type salvaging)
    : StochasticProcess(), model_(model), salvaging_(salvaging) {

    if (disc == euler) {
        discretization_ = boost::make_shared<EulerDiscretization>();
    } else {
        discretization_ = boost::make_shared<CrossAssetStateProcess::ExactDiscretization>(model, salvaging);
    }
}

Size CrossAssetStateProcess::size() const { return model_->dimension(); }

void CrossAssetStateProcess::flushCache() const {
    cache_m_.clear();
    cache_v_.clear();
    cache_d_.clear();
    boost::shared_ptr<CrossAssetStateProcess::ExactDiscretization> tmp =
        boost::dynamic_pointer_cast<CrossAssetStateProcess::ExactDiscretization>(discretization_);
    if (tmp != NULL) {
        tmp->flushCache();
    }
}

Disposable<Array> CrossAssetStateProcess::initialValues() const {
    Array res(model_->dimension(), 0.0);
    /* irlgm1f processes have initial value 0 */
    for (Size i = 0; i < model_->components(FX); ++i) {
        /* fxbs processes are in log spot */
        res[model_->pIdx(FX, i, 0)] = std::log(model_->fxbs(i)->fxSpotToday()->value());
    }
    return res;
}

Disposable<Array> CrossAssetStateProcess::drift(Time t, const Array& x) const {
    Array res(model_->dimension(), 0.0);
    Size n = model_->components(IR);
    Real H0 = model_->irlgm1f(0)->H(t);
    Real Hprime0 = model_->irlgm1f(0)->Hprime(t);
    Real alpha0 = model_->irlgm1f(0)->alpha(t);
    Real zeta0 = model_->irlgm1f(0)->zeta(t);
    boost::unordered_map<double, Array>::const_iterator i = cache_m_.find(t);
    if (i == cache_m_.end()) {
        /* z0 has drift 0 */
        for (Size i = 1; i < n; ++i) {
            Real Hi = model_->irlgm1f(i)->H(t);
            Real alphai = model_->irlgm1f(i)->alpha(t);
            Real sigmai = model_->fxbs(i - 1)->sigma(t);
            // ir-ir
            Real rhozz0i = model_->correlation(IR, 0, IR, i);
            // ir-fx
            Real rhozx0i = model_->correlation(IR, 0, FX, i - 1);
            Real rhozxii = model_->correlation(IR, i, FX, i - 1);
            // ir drifts
            res[model_->pIdx(IR, i, 0)] =
                -Hi * alphai * alphai + H0 * alpha0 * alphai * rhozz0i - sigmai * alphai * rhozxii;
            // log spot fx drifts (z0, zi independent parts)
            res[model_->pIdx(FX, i - 1, 0)] =
                H0 * alpha0 * sigmai * rhozx0i + model_->irlgm1f(0)->termStructure()->forwardRate(t, t, Continuous) -
                model_->irlgm1f(i)->termStructure()->forwardRate(t, t, Continuous) - 0.5 * sigmai * sigmai;
        }
        cache_m_.insert(std::make_pair(t, res));
    } else {
        res = i->second;
    }
    for (Size i = 1; i < n; ++i) {
        // log spot fx drifts (z0, zi dependent parts)
        Real Hi = model_->irlgm1f(i)->H(t);
        Real Hprimei = model_->irlgm1f(i)->Hprime(t);
        Real zetai = model_->irlgm1f(i)->zeta(t);
        res[model_->pIdx(FX, i - 1, 0)] += x[model_->pIdx(IR, 0, 0)] * Hprime0 + zeta0 * Hprime0 * H0 -
                                           x[model_->pIdx(IR, i, 0)] * Hprimei - zetai * Hprimei * Hi;
    }
    return res;
}

Disposable<Matrix> CrossAssetStateProcess::diffusion(Time t, const Array& x) const {
    boost::unordered_map<double, Matrix>::const_iterator i = cache_d_.find(t);
    if (i == cache_d_.end()) {
        Matrix tmp = pseudoSqrt(diffusionImpl(t, x), salvaging_);
        cache_d_.insert(std::make_pair(t, tmp));
        return tmp;
    } else {
        // we have to make a copy, otherwise we destroy the map entry
        // since a disposable is returned
        Matrix tmp = i->second;
        return tmp;
    }
}

Disposable<Matrix> CrossAssetStateProcess::diffusionImpl(Time t, const Array&) const {
    Matrix res(model_->dimension(), model_->dimension());
    Size n = model_->components(IR);
    Size m = model_->components(FX);
    // ir-ir
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j <= i; ++j) {
            Real alphai = model_->irlgm1f(i)->alpha(t);
            Real alphaj = model_->irlgm1f(j)->alpha(t);
            Real rhozz = model_->correlation(IR, i, IR, j, 0, 0);
            res[model_->pIdx(IR, i, 0)][model_->pIdx(IR, j, 0)] = res[model_->pIdx(IR, j, 0)][model_->pIdx(IR, i, 0)] =
                alphai * alphaj * rhozz;
        }
    }
    // ir-fx
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < m; ++j) {
            Real alphai = model_->irlgm1f(i)->alpha(t);
            Real sigmaj = model_->fxbs(j)->sigma(t);
            Real rhozx = model_->correlation(IR, i, FX, j, 0, 0);
            res[model_->pIdx(IR, i, 0)][model_->pIdx(FX, j, 0)] = res[model_->pIdx(FX, j, 0)][model_->pIdx(IR, i, 0)] =
                alphai * sigmaj * rhozx;
        }
    }
    // fx-fx
    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j <= i; ++j) {
            Real sigmai = model_->fxbs(i)->sigma(t);
            Real sigmaj = model_->fxbs(j)->sigma(t);
            Real rhoxx = model_->correlation(FX, i, FX, j, 0, 0);
            res[model_->pIdx(FX, i, 0)][model_->pIdx(FX, j, 0)] = res[model_->pIdx(FX, j, 0)][model_->pIdx(FX, i, 0)] =
                sigmai * sigmaj * rhoxx;
        }
    }
    return res;
}

CrossAssetStateProcess::ExactDiscretization::ExactDiscretization(const CrossAssetModel* const model,
                                                                 SalvagingAlgorithm::Type salvaging)
    : model_(model), salvaging_(salvaging) {}

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::drift(const StochasticProcess& p, Time t0,
                                                                     const Array& x0, Time dt) const {
    Array res;
    cache_key k = { t0, dt };
    boost::unordered_map<cache_key, Array>::const_iterator i = cache_m_.find(k);
    if (i == cache_m_.end()) {
        res = driftImpl1(p, t0, x0, dt);
        cache_m_.insert(std::make_pair(k, res));
    } else {
        res = i->second;
    }
    Array res2 = driftImpl2(p, t0, x0, dt);
    for (Size i = 0; i < res.size(); ++i) {
        res[i] += res2[i];
    }
    return res - x0;
}

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::diffusion(const StochasticProcess& p, Time t0,
                                                                          const Array& x0, Time dt) const {
    cache_key k = { t0, dt };
    boost::unordered_map<cache_key, Matrix>::const_iterator i = cache_d_.find(k);
    if (i == cache_d_.end()) {
        Matrix res = pseudoSqrt(covariance(p, t0, x0, dt), salvaging_);
        // note that covariance actually does not depend on x0
        cache_d_.insert(std::make_pair(k, res));
        return res;
    } else {
        // see above about the copy
        Matrix tmp = i->second;
        return tmp;
    }
}

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::covariance(const StochasticProcess& p, Time t0,
                                                                           const Array& x0, Time dt) const {
    cache_key k = { t0, dt };
    boost::unordered_map<cache_key, Matrix>::const_iterator i = cache_v_.find(k);
    if (i == cache_v_.end()) {
        Matrix res = covarianceImpl(p, t0, x0, dt);
        cache_v_.insert(std::make_pair(k, res));
        return res;
    } else {
        // see above about the copy
        Matrix tmp = i->second;
        return tmp;
    }
}

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::driftImpl1(const StochasticProcess&, Time t0,
                                                                          const Array&, Time dt) const {
    Size n = model_->components(IR);
    Size m = model_->components(FX);
    Array res(model_->dimension(), 0.0);
    for (Size i = 0; i < n; ++i) {
        res[model_->pIdx(IR, i, 0)] = ir_expectation_1(model_, i, t0, dt);
    }
    for (Size j = 0; j < m; ++j) {
        res[model_->pIdx(FX, j, 0)] = fx_expectation_1(model_, j, t0, dt);
    }
    return res;
}

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::driftImpl2(const StochasticProcess&, Time t0,
                                                                          const Array& x0, Time dt) const {
    Size n = model_->components(IR);
    Size m = model_->components(FX);
    Array res(model_->dimension(), 0.0);
    for (Size i = 0; i < n; ++i) {
        res[model_->pIdx(IR, i, 0)] += ir_expectation_2(model_, i, x0[model_->pIdx(IR, i, 0)]);
    }
    for (Size j = 0; j < m; ++j) {
        res[model_->pIdx(FX, j, 0)] += fx_expectation_2(model_, j, t0, x0[model_->pIdx(FX, j, 0)],
                                                        x0[model_->pIdx(IR, j + 1, 0)], x0[model_->pIdx(IR, 0, 0)], dt);
    }
    return res;
}

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::covarianceImpl(const StochasticProcess&, Time t0,
                                                                               const Array&, Time dt) const {
    Matrix res(model_->dimension(), model_->dimension());
    Size n = model_->components(IR);
    Size m = model_->components(FX);
    // ir-ir
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j <= i; ++j) {
            res[model_->pIdx(IR, i, 0)][model_->pIdx(IR, j, 0)] = res[model_->pIdx(IR, j, 0)][model_->pIdx(IR, i, 0)] =
                ir_ir_covariance(model_, i, j, t0, dt);
        }
    }
    // ir-fx
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < m; ++j) {
            res[model_->pIdx(IR, i, 0)][model_->pIdx(FX, j, 0)] = res[model_->pIdx(FX, j, 0)][model_->pIdx(IR, i, 0)] =
                ir_fx_covariance(model_, i, j, t0, dt);
        }
    }
    // fx-fx
    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j <= i; ++j) {
            res[model_->pIdx(FX, i, 0)][model_->pIdx(FX, j, 0)] = res[model_->pIdx(FX, j, 0)][model_->pIdx(FX, i, 0)] =
                fx_fx_covariance(model_, i, j, t0, dt);
        }
    }
    return res;
}

void CrossAssetStateProcess::ExactDiscretization::flushCache() const {
    cache_m_.clear();
    cache_v_.clear();
    cache_d_.clear();
}

} // namespace QuantExt
