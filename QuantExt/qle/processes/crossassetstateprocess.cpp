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
using namespace QuantLib;

namespace {
inline void setValue(Matrix& m, const Real& value, const QuantExt::CrossAssetModel* model,
                     const QuantExt::CrossAssetModelTypes::AssetType& t1, const Size& i1,
                     const QuantExt::CrossAssetModelTypes::AssetType& t2, const Size& i2, const Size& offset1 = 0,
                     const Size& offset2 = 0) {
    Size i = model->pIdx(t1, i1, offset1);
    Size j = model->pIdx(t2, i2, offset2);
    m[i][j] = m[j][i] = value;
}
inline void setValue(Array& a, const Real& value, const QuantExt::CrossAssetModel* model,
                     const QuantExt::CrossAssetModelTypes::AssetType& t, const Size& i, const Size& offset = 0) {
    a[model->pIdx(t, i, offset)] = value;
}
} // anonymous namespace

CrossAssetStateProcess::CrossAssetStateProcess(const CrossAssetModel* const model, discretization disc,
                                               SalvagingAlgorithm::Type salvaging)
    : StochasticProcess(), model_(model), disc_(disc), salvaging_(salvaging) {

    updateSqrtCorrelation();
    if (disc_ == euler) {
        discretization_ = boost::make_shared<EulerDiscretization>();
    } else {
        discretization_ = boost::make_shared<CrossAssetStateProcess::ExactDiscretization>(model, salvaging);
    }
}

Size CrossAssetStateProcess::size() const { return model_->dimension(); }

void CrossAssetStateProcess::flushCache() const {
    cache_m_.clear();
    cache_md_.clear();
    cache_v_.clear();
    cache_d_.clear();
    boost::shared_ptr<CrossAssetStateProcess::ExactDiscretization> tmp =
        boost::dynamic_pointer_cast<CrossAssetStateProcess::ExactDiscretization>(discretization_);
    if (tmp != NULL) {
        tmp->flushCache();
    }
    updateSqrtCorrelation();
}

void CrossAssetStateProcess::updateSqrtCorrelation() const {
    if (disc_ != euler)
        return;
    // build sqrt corr (for correlation matrix that covers all state variables)
    // this can be simplified once we use as many brownians as model->brownians()
    // instead of the full state vector
    Matrix corr(model_->dimension(), model_->dimension(), 1.0);
    Size brownianIndex = 0;
    std::vector<Size> brownianIndices;
    for (Size t = 0; t < crossAssetModelAssetTypes; ++t) {
        AssetType assetType = AssetType(t);
        for (Size i = 0; i < model_->components(assetType); ++i) {
            for (Size j = 0; j < model_->brownians(assetType, i); ++j) {
                for (Size ii = 0; ii < model_->stateVariables(assetType, i); ++ii) {
                    brownianIndices.push_back(brownianIndex);
                }
                ++brownianIndex;
            }
        }
    }
    for (Size i = 0; i < corr.rows(); ++i) {
        for (Size j = 0; j < i; ++j) {
            corr[i][j] = corr[j][i] = model_->correlation()(brownianIndices[i], brownianIndices[j]);
        }
    }
    sqrtCorrelation_ = pseudoSqrt(corr, salvaging_);
}

Disposable<Array> CrossAssetStateProcess::initialValues() const {
    Array res(model_->dimension(), 0.0);
    /* irlgm1f processes have initial value 0 */
    for (Size i = 0; i < model_->components(FX); ++i) {
        /* fxbs processes are in log spot */
        res[model_->pIdx(FX, i, 0)] = std::log(model_->fxbs(i)->fxSpotToday()->value());
    }
    for (Size i = 0; i < model_->components(EQ); ++i) {
        /* eqbs processes are in log spot */
        res[model_->pIdx(EQ, i, 0)] = std::log(model_->eqbs(i)->eqSpotToday()->value());
    }
    /* infdk, crlgm1f processes have initial value 0 */
    return res;
}

Disposable<Array> CrossAssetStateProcess::drift(Time t, const Array& x) const {
    Array res(model_->dimension(), 0.0);
    Size n = model_->components(IR);
    Size n_eq = model_->components(EQ);
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
        /* log equity spot drifts (the cache-able parts) */
        for (Size k = 0; k < n_eq; ++k) {
            Size i = model_->ccyIndex(model_->eqbs(k)->currency());
            // ir params (for equity currency)
            Real eps_ccy = (i == 0) ? 0.0 : 1.0;
            // Real Hi = model_->irlgm1f(i)->H(t);
            // Real alphai = model_->irlgm1f(i)->alpha(t);
            // eq vol
            Real sigmask = model_->eqbs(k)->sigma(t);
            // fx vol (eq ccy / base ccy)
            Real sigmaxi = (i == 0) ? 0.0 : model_->fxbs(i - 1)->sigma(t);
            // ir-eq corr
            // Real rhozsik = model_->correlation(EQ, k, IR, i); // eq cur
            Real rhozs0k = model_->correlation(EQ, k, IR, 0); // base cur
            // fx-eq corr
            Real rhoxsik = (i == 0) ? 0.0 : // no fx process for base-ccy
                               model_->correlation(FX, i - 1, EQ, k);
            // ir instantaneous forward rate (from curve used for eq forward projection)
            Real fr_i = model_->eqbs(k)->equityIrCurveToday()->forwardRate(t, t, Continuous);
            // div yield instantaneous forward rate
            Real fq_k = model_->eqbs(k)->equityDivYieldCurveToday()->forwardRate(t, t, Continuous);
            res[model_->pIdx(EQ, k, 0)] = fr_i - fq_k + (rhozs0k * H0 * alpha0 * sigmask) -
                                          (eps_ccy * rhoxsik * sigmaxi * sigmask) - (0.5 * sigmask * sigmask);
        }
        cache_m_.insert(std::make_pair(t, res));
    } else {
        res = i->second;
    }
    // non-cacheable sections of drifts
    for (Size i = 1; i < n; ++i) {
        // log spot fx drifts (z0, zi dependent parts)
        Real Hi = model_->irlgm1f(i)->H(t);
        Real Hprimei = model_->irlgm1f(i)->Hprime(t);
        Real zetai = model_->irlgm1f(i)->zeta(t);
        res[model_->pIdx(FX, i - 1, 0)] += x[model_->pIdx(IR, 0, 0)] * Hprime0 + zeta0 * Hprime0 * H0 -
                                           x[model_->pIdx(IR, i, 0)] * Hprimei - zetai * Hprimei * Hi;
    }
    for (Size k = 0; k < n_eq; ++k) {
        // log equity spot drifts (path-dependent parts)
        // notice the assumption in below that dividend yield curve is static
        Size i = model_->ccyIndex(model_->eqbs(k)->currency());
        // ir params (for equity currency)
        Real Hi = model_->irlgm1f(i)->H(t);
        Real Hprimei = model_->irlgm1f(i)->Hprime(t);
        Real zetai = model_->irlgm1f(i)->zeta(t);
        res[model_->pIdx(EQ, k, 0)] += (x[model_->pIdx(IR, i, 0)] * Hprimei) + (zetai * Hprimei * Hi);
    }
    /* no drift for infdk, crlgm1f components */
    return res;
}

Disposable<Matrix> CrossAssetStateProcess::diffusion(Time t, const Array& x) const {
    boost::unordered_map<double, Matrix>::const_iterator i = cache_d_.find(t);
    if (i == cache_d_.end()) {
        Matrix tmp = diffusionImpl(t, x);
        cache_d_.insert(std::make_pair(t, tmp));
        return tmp;
    } else {
        // we have to make a copy, otherwise we destroy the map entry
        // since a disposable is returned
        Matrix tmp = i->second;
        return tmp;
    }
}

Disposable<Array> CrossAssetStateProcess::marginalDiffusion(Time t, const Array& x) const {
    boost::unordered_map<double, Array>::const_iterator i = cache_md_.find(t);
    if (i == cache_md_.end()) {
        Array tmp = marginalDiffusionImpl(t, x);
        cache_md_.insert(std::make_pair(t, tmp));
        return tmp;
    } else {
        // we have to make a copy, otherwise we destroy the map entry
        // since a disposable is returned
        Array tmp = i->second;
        return tmp;
    }
}

Disposable<Matrix> CrossAssetStateProcess::diffusionImpl(Time t, const Array& x) const {
    Matrix res(model_->dimension(), model_->dimension(), 0.0);
    Array diag = marginalDiffusion(t, x);
    for (Size i = 0; i < x.size(); ++i) {
        res[i][i] = diag[i];
    }
    return res * sqrtCorrelation_;
} // namespace QuantExt

Disposable<Array> CrossAssetStateProcess::marginalDiffusionImpl(Time t, const Array&) const {
    Array res(model_->dimension(), 0.0);
    Size n = model_->components(IR);
    Size m = model_->components(FX);
    Size d = model_->components(INF);
    Size c = model_->components(CR);
    Size e = model_->components(EQ);
    // ir-ir
    for (Size i = 0; i < n; ++i) {
        Real alphai = model_->irlgm1f(i)->alpha(t);
        setValue(res, alphai, model_, IR, i, 0);
    }
    // fx-fx
    for (Size i = 0; i < m; ++i) {
        Real sigmai = model_->fxbs(i)->sigma(t);
        setValue(res, sigmai, model_, FX, i, 0);
    }
    // inf-inf
    for (Size i = 0; i < d; ++i) {
        Real alphai = model_->infdk(i)->alpha(t);
        Real Hi = model_->infdk(i)->H(t);
        // infz-infz
        setValue(res, alphai, model_, INF, i, 0);
        // infy-infy
        setValue(res, alphai * Hi, model_, INF, i, 1);
    }
    for (Size i = 0; i < c; ++i) {
        // Skip CR components that are not LGM
        if (model_->modelType(CR, i) != LGM1F)
            continue;
        Real alphai = model_->crlgm1f(i)->alpha(t);
        Real Hi = model_->crlgm1f(i)->H(t);
        // crz-crz
        setValue(res, alphai, model_, CR, i, 0);
        // cry-cry
        setValue(res, alphai * Hi, model_, CR, i, 1);
    }
    // eq-eq
    for (Size i = 0; i < e; ++i) {
        Real sigmai = model_->eqbs(i)->sigma(t);
        setValue(res, sigmai, model_, EQ, i, 0);
    }
    return res;
}

Disposable<Array> CrossAssetStateProcess::evolve(Time t0, const Array& x0, Time dt, const Array& dw) const {
    if (disc_ == euler) {
        const Array dz = sqrtCorrelation_ * dw;
        const Array df = marginalDiffusion(t0, x0);
        return apply(expectation(t0, x0, dt), df * dz * std::sqrt(dt));
    } else
        return StochasticProcess::evolve(t0, x0, dt, dw);
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
    Size e = model_->components(EQ);
    Array res(model_->dimension(), 0.0);
    for (Size i = 0; i < n; ++i) {
        res[model_->pIdx(IR, i, 0)] = ir_expectation_1(model_, i, t0, dt);
    }
    for (Size j = 0; j < m; ++j) {
        res[model_->pIdx(FX, j, 0)] = fx_expectation_1(model_, j, t0, dt);
    }
    for (Size k = 0; k < e; ++k) {
        res[model_->pIdx(EQ, k, 0)] = eq_expectation_1(model_, k, t0, dt);
    }
    return res;
}

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::driftImpl2(const StochasticProcess&, Time t0,
                                                                          const Array& x0, Time dt) const {
    Size n = model_->components(IR);
    Size m = model_->components(FX);
    Size e = model_->components(EQ);
    Array res(model_->dimension(), 0.0);
    for (Size i = 0; i < n; ++i) {
        res[model_->pIdx(IR, i, 0)] += ir_expectation_2(model_, i, x0[model_->pIdx(IR, i, 0)]);
    }
    for (Size j = 0; j < m; ++j) {
        res[model_->pIdx(FX, j, 0)] += fx_expectation_2(model_, j, t0, x0[model_->pIdx(FX, j, 0)],
                                                        x0[model_->pIdx(IR, j + 1, 0)], x0[model_->pIdx(IR, 0, 0)], dt);
    }
    for (Size k = 0; k < e; ++k) {
        Size eqCcyIdx = model_->ccyIndex(model_->eqbs(k)->currency());
        res[model_->pIdx(EQ, k, 0)] +=
            eq_expectation_2(model_, k, t0, x0[model_->pIdx(EQ, k, 0)], x0[model_->pIdx(IR, eqCcyIdx, 0)], dt);
    }
    /*! inf, cr components have integrated drift 0, we have to return the conditional
        expected value though, since x0 is subtracted later */
    Size d = model_->components(INF);
    for (Size i = 0; i < d; ++i) {
        res[model_->pIdx(INF, i, 0)] = x0[model_->pIdx(INF, i, 0)];
        res[model_->pIdx(INF, i, 1)] = x0[model_->pIdx(INF, i, 1)];
    }
    Size c = model_->components(CR);
    for (Size i = 0; i < c; ++i) {
        res[model_->pIdx(CR, i, 0)] = x0[model_->pIdx(CR, i, 0)];
        res[model_->pIdx(CR, i, 1)] = x0[model_->pIdx(CR, i, 1)];
    }
    return res;
}

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::covarianceImpl(const StochasticProcess&, Time t0,
                                                                               const Array&, Time dt) const {
    Matrix res(model_->dimension(), model_->dimension());
    Size n = model_->components(IR);
    Size m = model_->components(FX);
    Size d = model_->components(INF);
    Size c = model_->components(CR);
    Size e = model_->components(EQ);
    // ir-ir
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j <= i; ++j) {
            setValue(res, ir_ir_covariance(model_, i, j, t0, dt), model_, IR, i, IR, j, 0, 0);
        }
    }
    // ir-fx
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < m; ++j) {
            setValue(res, ir_fx_covariance(model_, i, j, t0, dt), model_, IR, i, FX, j, 0, 0);
        }
    }
    // fx-fx
    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j <= i; ++j) {
            setValue(res, fx_fx_covariance(model_, i, j, t0, dt), model_, FX, i, FX, j);
        }
    }
    // ir,fx,inf - inf
    for (Size j = 0; j < d; ++j) {
        for (Size i = 0; i <= j; ++i) {
            // infz-infz
            setValue(res, infz_infz_covariance(model_, i, j, t0, dt), model_, INF, i, INF, j, 0, 0);
            // infz-infy
            setValue(res, infz_infy_covariance(model_, i, j, t0, dt), model_, INF, i, INF, j, 0, 1);
            setValue(res, infz_infy_covariance(model_, j, i, t0, dt), model_, INF, i, INF, j, 1, 0);
            // infy-infy
            setValue(res, infy_infy_covariance(model_, i, j, t0, dt), model_, INF, i, INF, j, 1, 1);
        }
        for (Size i = 0; i < n; ++i) {
            // ir-inf
            setValue(res, ir_infz_covariance(model_, i, j, t0, dt), model_, IR, i, INF, j, 0, 0);
            setValue(res, ir_infy_covariance(model_, i, j, t0, dt), model_, IR, i, INF, j, 0, 1);
        }
        for (Size i = 0; i < (n - 1); ++i) {
            // fx-inf
            setValue(res, fx_infz_covariance(model_, i, j, t0, dt), model_, FX, i, INF, j, 0, 0);
            setValue(res, fx_infy_covariance(model_, i, j, t0, dt), model_, FX, i, INF, j, 0, 1);
        }
    }
    // ir,fx,inf,cr - cr
    for (Size j = 0; j < c; ++j) {
        // Skip CR components that are not LGM
        if (model_->modelType(CR, j) != LGM1F)
            continue;
        for (Size i = 0; i <= j; ++i) {
            // Skip CR components that are not LGM
            if (model_->modelType(CR, i) != LGM1F)
                continue;
            // crz-crz
            setValue(res, crz_crz_covariance(model_, i, j, t0, dt), model_, CR, i, CR, j, 0, 0);
            // crz-cry
            setValue(res, crz_cry_covariance(model_, i, j, t0, dt), model_, CR, i, CR, j, 0, 1);
            setValue(res, crz_cry_covariance(model_, j, i, t0, dt), model_, CR, i, CR, j, 1, 0);
            // cry-cry
            setValue(res, cry_cry_covariance(model_, i, j, t0, dt), model_, CR, i, CR, j, 1, 1);
        }
        for (Size i = 0; i < n; ++i) {
            // ir-cr
            setValue(res, ir_crz_covariance(model_, i, j, t0, dt), model_, IR, i, CR, j, 0, 0);
            setValue(res, ir_cry_covariance(model_, i, j, t0, dt), model_, IR, i, CR, j, 0, 1);
        }
        for (Size i = 0; i < (n - 1); ++i) {
            // fx-cr
            setValue(res, fx_crz_covariance(model_, i, j, t0, dt), model_, FX, i, CR, j, 0, 0);
            setValue(res, fx_cry_covariance(model_, i, j, t0, dt), model_, FX, i, CR, j, 0, 1);
        }
        for (Size i = 0; i < d; ++i) {
            // inf-cr
            setValue(res, infz_crz_covariance(model_, i, j, t0, dt), model_, INF, i, CR, j, 0, 0);
            setValue(res, infy_crz_covariance(model_, i, j, t0, dt), model_, INF, i, CR, j, 1, 0);
            setValue(res, infz_cry_covariance(model_, i, j, t0, dt), model_, INF, i, CR, j, 0, 1);
            setValue(res, infy_cry_covariance(model_, i, j, t0, dt), model_, INF, i, CR, j, 1, 1);
        }
    }
    // ir,fx,inf,cr,eq - eq
    for (Size j = 0; j < e; ++j) {
        for (Size i = 0; i <= j; ++i) {
            // eq-eq
            setValue(res, eq_eq_covariance(model_, i, j, t0, dt), model_, EQ, i, EQ, j, 0, 0);
        }
        for (Size i = 0; i < n; ++i) {
            // ir-eq
            setValue(res, ir_eq_covariance(model_, i, j, t0, dt), model_, IR, i, EQ, j, 0, 0);
        }
        for (Size i = 0; i < (n - 1); ++i) {
            // fx-eq
            setValue(res, fx_eq_covariance(model_, i, j, t0, dt), model_, FX, i, EQ, j, 0, 0);
        }
        for (Size i = 0; i < d; ++i) {
            // inf-eq
            setValue(res, infz_eq_covariance(model_, i, j, t0, dt), model_, INF, i, EQ, j, 0, 0);
            setValue(res, infy_eq_covariance(model_, i, j, t0, dt), model_, INF, i, EQ, j, 1, 0);
        }
        for (Size i = 0; i < c; ++i) {
            // Skip CR components that are not LGM
            if (model_->modelType(CR, i) != LGM1F)
                continue;
            // cr-eq
            setValue(res, crz_eq_covariance(model_, i, j, t0, dt), model_, CR, i, EQ, j, 0, 0);
            setValue(res, cry_eq_covariance(model_, i, j, t0, dt), model_, CR, i, EQ, j, 1, 0);
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
