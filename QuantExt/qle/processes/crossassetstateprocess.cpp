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
    : StochasticProcess(), model_(model), disc_(disc), salvaging_(salvaging), cirppCount_(0) {

    updateSqrtCorrelation();
    if (disc_ == euler) {
        discretization_ = boost::make_shared<EulerDiscretization>();
    } else {
        discretization_ = boost::make_shared<CrossAssetStateProcess::ExactDiscretization>(model, salvaging);
    }

    // set up CR CIR++ processes, defer the euler discretisation check to evolve()
    for (Size i = 0; i < model_->components(CR); ++i) {
        if (model_->modelType(CR, i) == CIRPP) {
            crCirpp_.push_back(model->crcirppModel(i)->stateProcess());
            cirppCount_++;
        } else {
            crCirpp_.push_back(boost::shared_ptr<CrCirppStateProcess>());
        }
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

            // 3 possibilities for number of state variables vs. the number of Brownian motions for the i-th component
            // within the current asset type.
            // 1) They are equal. Make the assumption that there is a 1-1 correspondence. This is essentially what has
            //    been happening until now outside of DK model i.e. always 1 state var and 1 Brownian motion.
            // 2) number of state vars > number of Brownian motions. Happens with DK model. Think the code below will
            //    only work when number of Brownian motions equals 1. Not changing it as not sure what was intended.
            // 3) number of state vars < number of Brownian motions. Not covered below.
            auto brownians = model_->brownians(assetType, i);
            auto stateVars = model_->stateVariables(assetType, i);

            if (brownians == stateVars) {
                for (Size k = 0; k < brownians; ++k) {
                    brownianIndices.push_back(brownianIndex++);
                }
            } else {
                for (Size j = 0; j < brownians; ++j) {
                    for (Size k = 0; k < stateVars; ++k) {
                        brownianIndices.push_back(brownianIndex);
                    }
                    ++brownianIndex;
                }
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
    // CR CIR++ components
    for (Size i = 0; i < model_->components(CR); ++i) {
        if (model_->modelType(CR, i) != CIRPP)
            continue;
        QL_REQUIRE(crCirpp_[i], "crcirpp is null!");
        Array r = crCirpp_[i]->initialValues();
        res[model_->pIdx(CR, i, 0)] = r[0]; // y0
        res[model_->pIdx(CR, i, 1)] = r[1]; // S(0,0) = 1
    }

    for (Size i = 0; i < model_->components(INF); ++i) {
        // Second component of JY model is the inflation index process.
        if (model_->modelType(INF, i) == JY) {
            res[model_->pIdx(INF, i, 1)] = std::log(model_->infjy(i)->index()->fxSpotToday()->value());
        }
    }
    /* infdk, crlgm1f, aux processes have initial value 0 */
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
        /* z0 has drift 0 in the LGM measure but non-zero drift in the bank account measure, so start loop at i = 0 */
        for (Size i = 0; i < n; ++i) {
            Real Hi = model_->irlgm1f(i)->H(t);
            Real alphai = model_->irlgm1f(i)->alpha(t);
            if (i == 0 && model_->measure() == Measure::BA) {
                // ADD z0 drift in the BA measure
                res[model_->pIdx(IR, i, 0)] = -Hi * alphai * alphai;
                // the auxiliary state variable is drift-free
                res[model_->pIdx(AUX, i, 0)] = 0.0;
            }
            if (i > 0) {
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
                res[model_->pIdx(FX, i - 1, 0)] = H0 * alpha0 * sigmai * rhozx0i +
                                                  model_->irlgm1f(0)->termStructure()->forwardRate(t, t, Continuous) -
                                                  model_->irlgm1f(i)->termStructure()->forwardRate(t, t, Continuous) -
                                                  0.5 * sigmai * sigmai;
                if (model_->measure() == Measure::BA) {
                    // REMOVE the LGM measure drift contributions above
                    res[model_->pIdx(IR, i, 0)] -= H0 * alpha0 * alphai * rhozz0i;
                    res[model_->pIdx(FX, i - 1, 0)] -= H0 * alpha0 * sigmai * rhozx0i;
                }
            }
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

        // State independent pieces of JY inflation model, if there is a CAM JY component.
        for (Size j = 0; j < model_->components(INF); ++j) {

            if (model_->modelType(INF, j) == JY) {

                auto p = model_->infjy(j);
                Size i_j = model_->ccyIndex(p->currency());

                // JY inflation parameter values.
                Real H_y_j = p->realRate()->H(t);
                Real Hp_y_j = p->realRate()->Hprime(t);
                Real zeta_y_j = p->realRate()->zeta(t);
                Real alpha_y_j = p->realRate()->alpha(t);
                Real sigma_c_j = p->index()->sigma(t);

                // Inflation nominal currency parameter values
                Real H_i_j = model_->irlgm1f(i_j)->H(t);
                Real Hp_i_j = model_->irlgm1f(i_j)->Hprime(t);
                Real zeta_i_j = model_->irlgm1f(i_j)->zeta(t);

                // Correlations
                Real rho_zy_0j = model_->correlation(IR, 0, INF, j, 0, 0);
                Real rho_yc_ij = model_->correlation(INF, j, INF, j, 0, 1);
                Real rho_zc_0j = model_->correlation(IR, 0, INF, j, 0, 1);

                // JY real rate drift. It is state independent
                auto rrDrift = -alpha_y_j * alpha_y_j * H_y_j + rho_zy_0j * alpha0 * alpha_y_j * H_y_j -
                               rho_yc_ij * alpha_y_j * sigma_c_j;

                if (i_j > 0) {
                    Real sigma_x_i_j = model_->fxbs(i_j - 1)->sigma(t);
                    Real rho_yx_j_i_j = model_->correlation(INF, j, FX, i_j - 1, 0, 0);
                    rrDrift -= rho_yx_j_i_j * alpha_y_j * sigma_x_i_j;
                }

                res[model_->pIdx(INF, j, 0)] = rrDrift;

                // JY log inflation index drift (state independent piece).
                auto indexDrift = rho_zc_0j * alpha0 * sigma_c_j * H0 - 0.5 * sigma_c_j * sigma_c_j +
                                  zeta_i_j * Hp_i_j * H_i_j - zeta_y_j * Hp_y_j * H_y_j;

                // Add on the f_n(0, t) - f_r(0, t) piece using the initial zero inflation term structure.
                // Use the same dt below that is used in yield forward rate calculations.
                auto ts = p->realRate()->termStructure();
                Time dt = 0.0001;
                Time t1 = std::max(t - dt / 2.0, 0.0);
                Time t2 = t1 + dt;
                auto z_t = ts->zeroRate(t);
                auto z_t1 = ts->zeroRate(t1);
                auto z_t2 = ts->zeroRate(t2);
                indexDrift += std::log(1 + z_t) + (t / (1 + z_t)) * ((z_t2 - z_t1) / dt);

                if (i_j > 0) {
                    Real sigma_x_i_j = model_->fxbs(i_j - 1)->sigma(t);
                    Real rho_cx_j_i_j = model_->correlation(INF, j, FX, i_j - 1, 1, 0);
                    indexDrift -= rho_cx_j_i_j * sigma_c_j * sigma_x_i_j;
                }

                res[model_->pIdx(INF, j, 1)] = indexDrift;
            }
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

    // Non-cacheable portion of inflation JY drift, if there is a CAM JY component.
    for (Size j = 0; j < model_->components(INF); ++j) {
        if (model_->modelType(INF, j) == JY) {

            auto p = model_->infjy(j);
            Size i_j = model_->ccyIndex(p->currency());

            // JY inflation parameter values.
            Real Hp_y_j = p->realRate()->Hprime(t);

            // Inflation nominal currency parameter values
            Real Hp_i_j = model_->irlgm1f(i_j)->Hprime(t);

            res[model_->pIdx(INF, j, 1)] += x[model_->pIdx(IR, i_j, 0)] * Hp_i_j - x[model_->pIdx(INF, j, 0)] * Hp_y_j;
        }
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
        if (model_->modelType(INF, i) == DK) {
            Real alphai = model_->infdk(i)->alpha(t);
            Real Hi = model_->infdk(i)->H(t);
            // DK z diffusion coefficient
            setValue(res, alphai, model_, INF, i, 0);
            // DK y diffusion coefficient
            setValue(res, alphai * Hi, model_, INF, i, 1);
        } else {
            auto p = model_->infjy(i);
            // JY z diffusion coefficient
            setValue(res, p->realRate()->alpha(t), model_, INF, i, 0);
            // JY I diffusion coefficient
            setValue(res, p->index()->sigma(t), model_, INF, i, 1);
        }
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

    if (model_->measure() == Measure::BA) {
        // aux-aux
        Real H0 = model_->irlgm1f(0)->H(t);
        Real alpha0 = model_->irlgm1f(0)->alpha(t);
        setValue(res, alpha0 * H0, model_, AUX, 0, 0);
    }

    return res;
}

Disposable<Array> CrossAssetStateProcess::evolve(Time t0, const Array& x0, Time dt, const Array& dw) const {

    Array res;
    if (disc_ == euler) {
        const Array dz = sqrtCorrelation_ * dw;
        const Array df = marginalDiffusion(t0, x0);
        res = apply(expectation(t0, x0, dt), df * dz * std::sqrt(dt));

        // CR CIRPP components
        if (cirppCount_ > 0) {
            for (Size i = 0; i < model_->components(CR); ++i) {
                if (!crCirpp_[i])
                    continue; // ignore non-cir cr model
                Size idx1 = model_->pIdx(CR, i, 0);
                Size idx2 = model_->pIdx(CR, i, 1);
                Array x0Tmp(2), dwTmp(2);
                x0Tmp[0] = x0[idx1];
                x0Tmp[1] = x0[idx2];
                dwTmp[0] = dz[idx1];
                dwTmp[1] = 0.0; // not used
                // evolve original process
                auto r = crCirpp_[i]->evolve(t0, x0Tmp, dt, dwTmp);

                // set result
                res[idx1] = r[0]; // y
                res[idx2] = r[1]; // S(0,T)
            }
        }
    } else {
        QL_REQUIRE(cirppCount_ == 0, "only euler discretization is supported for CIR++");
        res = StochasticProcess::evolve(t0, x0, dt, dw);
    }

    return res;
}

CrossAssetStateProcess::ExactDiscretization::ExactDiscretization(const CrossAssetModel* const model,
                                                                 SalvagingAlgorithm::Type salvaging)
    : model_(model), salvaging_(salvaging) {}

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::drift(const StochasticProcess& p, Time t0,
                                                                     const Array& x0, Time dt) const {
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

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::diffusion(const StochasticProcess& p, Time t0,
                                                                          const Array& x0, Time dt) const {
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

Disposable<Matrix> CrossAssetStateProcess::ExactDiscretization::covariance(const StochasticProcess& p, Time t0,
                                                                           const Array& x0, Time dt) const {
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

    // If inflation is JY, need to take account of the drift.
    for (Size i = 0; i < model_->components(INF); ++i) {
        if (model_->modelType(INF, i) == JY) {
            std::tie(res[model_->pIdx(INF, i, 0)], res[model_->pIdx(INF, i, 1)]) =
                inf_jy_expectation_1(model_, i, t0, dt);
        }
    }

    return res;
}

Disposable<Array> CrossAssetStateProcess::ExactDiscretization::driftImpl2(const StochasticProcess&, Time t0,
                                                                          const Array& x0, Time dt) const {
    Size n = model_->components(IR);
    Size m = model_->components(FX);
    Size e = model_->components(EQ);
    Array res(model_->dimension(), 0.0);

    if (model_->measure() == Measure::BA) {
        // zero AUX state drift, i.e. conditional expectation equal to the previous level as for z_i
        res[model_->pIdx(AUX, 0, 0)] += x0[model_->pIdx(AUX, 0, 0)];
    }

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

    // Inflation: JY is state dependent. DK is not. Even for DK, still need to return the conditional expected value.
    for (Size i = 0; i < model_->components(INF); ++i) {
        if (model_->modelType(INF, i) == JY) {
            auto i_i = model_->ccyIndex(model_->infjy(i)->currency());
            auto zi_i_0 = x0[model_->pIdx(IR, i_i, 0)];
            auto state_0 = std::make_pair(x0[model_->pIdx(INF, i, 0)], x0[model_->pIdx(INF, i, 1)]);
            std::tie(res[model_->pIdx(INF, i, 0)], res[model_->pIdx(INF, i, 1)]) =
                inf_jy_expectation_2(model_, i, t0, state_0, zi_i_0, dt);
        } else {
            res[model_->pIdx(INF, i, 0)] = x0[model_->pIdx(INF, i, 0)];
            res[model_->pIdx(INF, i, 1)] = x0[model_->pIdx(INF, i, 1)];
        }
    }

    /*! cr components have integrated drift 0, we have to return the conditional
        expected value though, since x0 is subtracted later */
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

    if (model_->measure() == Measure::BA) {
        // aux-aux
        setValue(res, aux_aux_covariance(model_, t0, dt), model_, AUX, 0, AUX, 0, 0, 0);
        // aux-ir
        for (Size j = 0; j < n; ++j) {
            setValue(res, aux_ir_covariance(model_, j, t0, dt), model_, AUX, 0, IR, j, 0, 0);
        }
        // aux-fx
        for (Size j = 0; j < m; ++j) {
            setValue(res, aux_fx_covariance(model_, j, t0, dt), model_, AUX, 0, FX, j, 0, 0);
        }
    }
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
