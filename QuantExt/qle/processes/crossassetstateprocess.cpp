/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/crossassetanalytics.hpp>

#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/processes/eulerdiscretization.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

using namespace CrossAssetAnalytics;

CrossAssetStateProcess::CrossAssetStateProcess(
    const CrossAssetModel *const model, discretization disc,
    SalvagingAlgorithm::Type salvaging)
    : StochasticProcess(), model_(model), salvaging_(salvaging) {

    if (disc == euler) {
        discretization_ = boost::make_shared<EulerDiscretization>();
    } else {
        discretization_ =
            boost::make_shared<CrossAssetStateProcess::ExactDiscretization>(
                model, salvaging);
    }
}

Size CrossAssetStateProcess::size() const { return model_->dimension(); }

void CrossAssetStateProcess::flushCache() const {
    cache_m_.clear();
    cache_v_.clear();
    cache_d_.clear();
    boost::shared_ptr<CrossAssetStateProcess::ExactDiscretization> tmp =
        boost::dynamic_pointer_cast<
            CrossAssetStateProcess::ExactDiscretization>(discretization_);
    if (tmp != NULL) {
        tmp->flushCache();
    }
}

Disposable<Array> CrossAssetStateProcess::initialValues() const {
    Array res(model_->dimension(), 0.0);
    /* irlgm1f processes have initial value 0 */
    for (Size i = 0; i < model_->currencies() - 1; ++i) {
        /* fxbs processes are in log spot */
        res[model_->currencies() + i] =
            std::log(model_->fxbs(i)->fxSpotToday()->value());
    }
    return res;
}

Disposable<Array> CrossAssetStateProcess::drift(Time t, const Array &x) const {
    Array res(model_->dimension(), 0.0);
    Size n = model_->currencies();
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
            Real rhozz0i = model_->ir_ir_correlation(0, i);
            // ir-fx
            Real rhozx0i = model_->ir_fx_correlation(0, i - 1);
            Real rhozxii = model_->ir_fx_correlation(i, i - 1);
            // ir drifts
            res[i] = -Hi * alphai * alphai + H0 * alpha0 * alphai * rhozz0i -
                     sigmai * alphai * rhozxii;
            // log spot fx drifts (z0, zi independent parts)
            res[n + i - 1] = H0 * alpha0 * sigmai * rhozx0i +
                             model_->irlgm1f(0)->termStructure()->forwardRate(
                                 t, t, Continuous) -
                             model_->irlgm1f(i)->termStructure()->forwardRate(
                                 t, t, Continuous) -
                             0.5 * sigmai * sigmai;
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
        res[n + i - 1] += x[0] * Hprime0 + zeta0 * Hprime0 * H0 -
                          x[i] * Hprimei - zetai * Hprimei * Hi;
    }
    return res;
}

Disposable<Matrix> CrossAssetStateProcess::diffusion(Time t,
                                                     const Array &x) const {
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

Disposable<Matrix> CrossAssetStateProcess::diffusionImpl(Time t,
                                                         const Array &) const {
    Matrix res(model_->dimension(), model_->dimension());
    Size n = model_->currencies();
    for (Size i = 0; i < 2 * n - 1; ++i) {
        for (Size j = 0; j <= i; ++j) {
            if (i < n) {
                Real alphai = model_->irlgm1f(i)->alpha(t);
                Real alphaj = model_->irlgm1f(j)->alpha(t);
                Real rhozz = model_->ir_ir_correlation(i, j);
                // ir-ir
                res[i][j] = res[j][i] = alphai * alphaj * rhozz;
            } else {
                Real sigmai = model_->fxbs(i - n)->sigma(t);
                if (j < n) {
                    // ir-fx
                    Real alphaj = model_->irlgm1f(j)->alpha(t);
                    Real rhozx = model_->ir_fx_correlation(j, i - n);
                    res[i][j] = res[j][i] = alphaj * sigmai * rhozx;
                } else {
                    // fx-fx
                    Real sigmaj = model_->fxbs(j - n)->sigma(t);
                    Real rhoxx = model_->fx_fx_correlation(i - n, j - n);
                    res[i][j] = res[j][i] = sigmai * sigmaj * rhoxx;
                }
            }
        }
    }
    return res;
}

CrossAssetStateProcess::ExactDiscretization::ExactDiscretization(
    const CrossAssetModel *const model, SalvagingAlgorithm::Type salvaging)
    : model_(model), salvaging_(salvaging) {}

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::drift(
    const StochasticProcess &p, Time t0, const Array &x0, Time dt) const {
    Array res;
    cache_key k = {t0, dt};
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

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::diffusion(
    const StochasticProcess &p, Time t0, const Array &x0, Time dt) const {
    cache_key k = {t0, dt};
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

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::covariance(
    const StochasticProcess &p, Time t0, const Array &x0, Time dt) const {
    cache_key k = {t0, dt};
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

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::driftImpl1(
    const StochasticProcess &, Time t0, const Array &, Time dt) const {
    Size n = model_->currencies();
    Array res(model_->dimension(), 0.0);
    for (Size i = 0; i < model_->currencies(); ++i) {
        res[i] = ir_expectation_1(model_, i, t0, dt);
        if (i > 0) {
            res[n + i - 1] = fx_expectation_1(model_, i - 1, t0, dt);
        }
    }
    return res;
}

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::driftImpl2(
    const StochasticProcess &, Time t0, const Array &x0, Time dt) const {
    Size n = model_->currencies();
    Array res(model_->dimension(), 0.0);
    for (Size i = 0; i < n; ++i) {
        res[i] += ir_expectation_2(model_, i, x0[i]);
        if (i > 0) {
            res[n + i - 1] += fx_expectation_2(model_, i - 1, t0, x0[n + i - 1],
                                               x0[i], x0[0], dt);
        }
    }
    return res;
}

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::covarianceImpl(
    const StochasticProcess &, Time t0, const Array &, Time dt) const {
    Matrix res(model_->dimension(), model_->dimension());
    Size n = model_->currencies();
    for (Size i = 0; i < 2 * n - 1; ++i) {
        for (Size j = 0; j <= i; ++j) {
            if (i < n) {
                res[i][j] = res[j][i] = ir_ir_covariance(model_, i, j, t0, dt);
            } else {
                if (j < n) {
                    res[i][j] = res[j][i] =
                        ir_fx_covariance(model_, j, i - n, t0, dt);
                } else {
                    res[j][i] = res[i][j] =
                        fx_fx_covariance(model_, i - n, j - n, t0, dt);
                }
            }
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
