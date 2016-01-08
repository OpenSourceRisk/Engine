/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/xassetmodel.hpp>
#include <qle/processes/xassetstateprocess.hpp>
#include <ql/math/matrixutilities/pseudosqrt.hpp>
#include <ql/processes/eulerdiscretization.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

XAssetStateProcess::XAssetStateProcess(const XAssetModel *const model,
                                       discretization disc)
    : StochasticProcess(), model_(model) {

    if (disc == euler) {
        discretization_ = boost::make_shared<EulerDiscretization>();
    } else {
        discretization_ =
            boost::make_shared<XAssetStateProcess::ExactDiscretization>(model);
    }

    QL_REQUIRE(2 * model_->currencies() - 1 == model_->dimension(),
               "this version of XAssetStateProcess is not consistent with "
               "XAssetModel, which should only be irlgm1f-fx");
}

Size XAssetStateProcess::size() const { return model_->dimension(); }

void XAssetStateProcess::flushCache() const {
    cache_m_.clear();
    cache_v_.clear();
    cache_d_.clear();
    boost::shared_ptr<XAssetStateProcess::ExactDiscretization> tmp =
        boost::dynamic_pointer_cast<XAssetStateProcess::ExactDiscretization>(
            discretization_);
    if (tmp != NULL) {
        tmp->flushCache();
    }
}

Disposable<Array> XAssetStateProcess::initialValues() const {
    Array res(model_->dimension(), 0.0);
    /* irlgm1f processes have initial value 0 */
    for (Size i = 0; i < model_->currencies() - 1; ++i) {
        /* fxbs processes are in log spot */
        res[model_->currencies() + i] =
            std::log(model_->fxbs(i)->fxSpotToday()->value());
    }
    return res;
}

Disposable<Array> XAssetStateProcess::drift(Time t, const Array &x) const {
    Array res(model_->dimension(), 0.0);
    Size n = model_->currencies();
    Real H0 = model_->irlgm1f(0)->H(t);
    Real Hprime0 = model_->irlgm1f(0)->Hprime(t);
    Real alpha0 = model_->irlgm1f(0)->alpha(t);
    Real zeta0 = model_->irlgm1f(0)->zeta(t);
    boost::unordered_map<double, Array>::iterator i = cache_m_.find(t);
    if (i == cache_m_.end()) {
        /* z0 has drift 0 */
        for (Size i = 1; i < n; ++i) {
            Real Hi = model_->irlgm1f(i)->H(t);
            Real alphai = model_->irlgm1f(i)->alpha(t);
            Real sigmai = model_->fxbs(i - 1)->sigma(t);
            // ir-ir
            Real rhozz0i = model_->correlation()[0][i];
            // ir-fx
            Real rhozx0i = model_->correlation()[0][n + i - 1];
            Real rhozxii = model_->correlation()[i][n + i - 1];
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

Disposable<Matrix> XAssetStateProcess::diffusion(Time t, const Array &) const {
    boost::unordered_map<double, Matrix>::iterator i = cache_d_.find(t);
    if (i == cache_d_.end()) {
        Matrix res(model_->dimension(), model_->dimension());
        Size n = model_->currencies();
        for (Size i = 0; i < 2 * n - 1; ++i) {
            for (Size j = 0; j <= i; ++j) {
                if (i < n) {
                    Real alphai = model_->irlgm1f(i)->alpha(t);
                    Real alphaj = model_->irlgm1f(j)->alpha(t);
                    Real rhozz = model_->correlation()[i][j];
                    // ir-ir
                    res[i][j] = res[j][i] = alphai * alphaj * rhozz;
                } else {
                    Real sigmai = model_->fxbs(i - n)->sigma(t);
                    if (j < n) {
                        // ir-fx
                        Real alphaj = model_->irlgm1f(j)->alpha(t);
                        Real rhozx = model_->correlation()[i][j];
                        res[i][j] = res[j][i] = alphaj * sigmai * rhozx;
                    } else {
                        // fx-fx
                        Real sigmaj = model_->fxbs(j - n)->sigma(t);
                        Real rhoxx = model_->correlation()[i][j];
                        res[i][j] = res[j][i] = sigmai * sigmaj * rhoxx;
                    }
                }
            }
        }
        Matrix tmp = pseudoSqrt(res, SalvagingAlgorithm::Spectral);
        cache_d_.insert(std::make_pair(t, tmp));
        return tmp;
    } else {
        // we have to make a copy, otherwise we destroy the map entry
        // since a disposable is returned
        Matrix tmp = i->second;
        return tmp;
    }
}

XAssetStateProcess::ExactDiscretization::ExactDiscretization(
    const XAssetModel *const model)
    : model_(model) {}

Disposable<Array> XAssetStateProcess::ExactDiscretization::drift(
    const StochasticProcess &, Time t0, const Array &x0, Time dt) const {
    Size n = model_->currencies();
    Array res(model_->dimension(), 0.0);
    cache_key k = {t0, dt};
    boost::unordered_map<cache_key, Array>::iterator i = cache_m_.find(k);
    if (i == cache_m_.end()) {
        for (Size i = 0; i < model_->currencies(); ++i) {
            res[i] = model_->ir_expectation_1(i, t0, dt);
            if (i > 0) {
                res[n + i - 1] = model_->fx_expectation_1(i - 1, t0, dt);
            }
        }
        cache_m_.insert(std::make_pair(k, res));
    } else {
        res = i->second;
    }
    for (Size i = 0; i < n; ++i) {
        res[i] += model_->ir_expectation_2(i, x0[i]);
        if (i > 0) {
            res[n + i - 1] += model_->fx_expectation_2(i - 1, t0, x0[n + i - 1],
                                                       x0[i], x0[0], dt);
        }
    }
    return res - x0;
}

Disposable<Matrix> XAssetStateProcess::ExactDiscretization::diffusion(
    const StochasticProcess &p, Time t0, const Array &x0, Time dt) const {
    cache_key k = {t0, dt};
    boost::unordered_map<cache_key, Matrix>::iterator i = cache_d_.find(k);
    if (i == cache_d_.end()) {
        Matrix res =
            pseudoSqrt(covariance(p, t0, x0, dt), SalvagingAlgorithm::Spectral);
        // note that covariance actually does not depend on x0
        cache_d_.insert(std::make_pair(k, res));
        return res;
    } else {
        // see above about the copy
        Matrix tmp = i->second;
        return tmp;
    }
}

Disposable<Matrix> XAssetStateProcess::ExactDiscretization::covariance(
    const StochasticProcess &, Time t0, const Array &, Time dt) const {
    cache_key k = {t0, dt};
    boost::unordered_map<cache_key, Matrix>::iterator i = cache_v_.find(k);
    if (i == cache_v_.end()) {
        Matrix res(model_->dimension(), model_->dimension());
        Size n = model_->currencies();
        for (Size i = 0; i < 2 * n - 1; ++i) {
            for (Size j = 0; j <= i; ++j) {
                if (i < n) {
                    res[i][j] = res[j][i] =
                        model_->ir_ir_covariance(i, j, t0, dt);
                } else {
                    if (j < n) {
                        res[i][j] = res[j][i] =
                            model_->ir_fx_covariance(j, i - n, t0, dt);
                    } else {
                        res[j][i] = res[i][j] =
                            model_->fx_fx_covariance(i - n, j - n, t0, dt);
                    }
                }
            }
        }
        cache_v_.insert(std::make_pair(k, res));
        return res;
    } else {
        // see above about the copy
        Matrix tmp = i->second;
        return tmp;
    }
}

void XAssetStateProcess::ExactDiscretization::flushCache() const {
    cache_m_.clear();
    cache_v_.clear();
    cache_d_.clear();
}

} // namespace QuantExt
