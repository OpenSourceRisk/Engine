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

#include <iostream>

namespace QuantExt {

using namespace CrossAssetAnalytics;
using namespace QuantLib;

namespace {

inline void setValue(Matrix& m, const Real& value, const QuantLib::ext::shared_ptr<const QuantExt::CrossAssetModel>& model,
                     const QuantExt::CrossAssetModel::AssetType& t1, const Size& i1,
                     const QuantExt::CrossAssetModel::AssetType& t2, const Size& i2, const Size& offset1 = 0,
                     const Size& offset2 = 0) {
    Size i = model->pIdx(t1, i1, offset1);
    Size j = model->pIdx(t2, i2, offset2);
    m[i][j] = m[j][i] = value;
}

inline void setValue2(Matrix& m, const Real& value, const QuantLib::ext::shared_ptr<const QuantExt::CrossAssetModel>& model,
                      const QuantExt::CrossAssetModel::AssetType& t1, const Size& i1,
                      const QuantExt::CrossAssetModel::AssetType& t2, const Size& i2, const Size& offset1 = 0,
                      const Size& offset2 = 0) {
    Size i = model->pIdx(t1, i1, offset1);
    Size j = model->wIdx(t2, i2, offset2);
    m[i][j] = value;
}
} // anonymous namespace

CrossAssetStateProcess::CrossAssetStateProcess(QuantLib::ext::shared_ptr<const CrossAssetModel> model)
    : StochasticProcess(), model_(std::move(model)), cirppCount_(0) {

    if (model_->discretization() == CrossAssetModel::Discretization::Euler) {
        discretization_ = QuantLib::ext::make_shared<EulerDiscretization>();
    } else {
        discretization_ =
            QuantLib::ext::make_shared<CrossAssetStateProcess::ExactDiscretization>(model_, model_->salvagingAlgorithm());
    }

    updateSqrtCorrelation();

    // set up CR CIR++ processes, defer the euler discretisation check to evolve()
    for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::CR); ++i) {
        if (model_->modelType(CrossAssetModel::AssetType::CR, i) == CrossAssetModel::ModelType::CIRPP) {
            crCirpp_.push_back(model_->crcirppModel(i)->stateProcess());
            cirppCount_++;
        } else {
            crCirpp_.push_back(QuantLib::ext::shared_ptr<CrCirppStateProcess>());
        }
    }
}

Size CrossAssetStateProcess::size() const { return model_->dimension(); }

Size CrossAssetStateProcess::factors() const { return model_->brownians() + model_->auxBrownians(); }

void CrossAssetStateProcess::resetCache(const Size timeSteps) const {
    cacheNotReady_m_ = cacheNotReady_d_ = true;
    timeStepsToCache_m_ = timeStepsToCache_d_ = timeSteps;
    timeStepCache_m_ = timeStepCache_d_ = 0;
    cache_m_.clear();
    cache_d_.clear();
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess::ExactDiscretization>(discretization_))
        tmp->resetCache(timeSteps);
    updateSqrtCorrelation();
}

void CrossAssetStateProcess::updateSqrtCorrelation() const {
    if (model_->discretization() != CrossAssetModel::Discretization::Euler)
        return;
    sqrtCorrelation_ = pseudoSqrt(model_->correlation(), model_->salvagingAlgorithm());
}

Array CrossAssetStateProcess::initialValues() const {
    Array res(model_->dimension(), 0.0);
    /* irlgm1f / irhw processes have initial value 0 */
    for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::FX); ++i) {
        /* fxbs processes are in log spot */
        res[model_->pIdx(CrossAssetModel::AssetType::FX, i, 0)] = std::log(model_->fxbs(i)->fxSpotToday()->value());
    }
    for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::EQ); ++i) {
        /* eqbs processes are in log spot */
        res[model_->pIdx(CrossAssetModel::AssetType::EQ, i, 0)] = std::log(model_->eqbs(i)->eqSpotToday()->value());
    }
    // CR CIR++ components
    for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::CR); ++i) {
        if (model_->modelType(CrossAssetModel::AssetType::CR, i) != CrossAssetModel::ModelType::CIRPP)
            continue;
        QL_REQUIRE(crCirpp_[i], "crcirpp is null!");
        Array r = crCirpp_[i]->initialValues();
        res[model_->pIdx(CrossAssetModel::AssetType::CR, i, 0)] = r[0]; // y0
        res[model_->pIdx(CrossAssetModel::AssetType::CR, i, 1)] = r[1]; // S(0,0) = 1
    }

    for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::INF); ++i) {
        // Second component of JY model is the inflation index process.
        if (model_->modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::JY) {
            res[model_->pIdx(CrossAssetModel::AssetType::INF, i, 1)] =
                std::log(model_->infjy(i)->index()->fxSpotToday()->value());
        }
    }
    /* infdk, crlgm1f, commodity, crstate processes have initial value 0 */
    return res;
}

Array CrossAssetStateProcess::drift(Time t, const Array& x) const {
    Array res(model_->dimension(), 0.0);
    Size n = model_->components(CrossAssetModel::AssetType::IR);
    Size n_eq = model_->components(CrossAssetModel::AssetType::EQ);
    Real H0 = model_->irlgm1f(0)->H(t);
    Real Hprime0 = model_->irlgm1f(0)->Hprime(t);
    Real alpha0 = model_->irlgm1f(0)->alpha(t);
    Real zeta0 = model_->irlgm1f(0)->zeta(t);
    if (cacheNotReady_m_) {
        /* z0 has drift 0 in the LGM measure but non-zero drift in the bank account measure, so start loop at i = 0 */
        for (Size i = 0; i < n; ++i) {
            Real Hi = model_->irlgm1f(i)->H(t);
            Real alphai = model_->irlgm1f(i)->alpha(t);
            if (i == 0 && model_->measure() == IrModel::Measure::BA) {
                // ADD z0 drift in the BA measure
                res[model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)] = -Hi * alphai * alphai;
                // the auxiliary state variable is drift-free
                res[model_->pIdx(CrossAssetModel::AssetType::IR, i, 1)] = 0.0;
            }
            if (i > 0) {
                Real sigmai = model_->fxbs(i - 1)->sigma(t);
                // ir-ir
                Real rhozz0i =
                    model_->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::IR, i);
                // ir-fx
                Real rhozx0i =
                    model_->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::FX, i - 1);
                Real rhozxii =
                    model_->correlation(CrossAssetModel::AssetType::IR, i, CrossAssetModel::AssetType::FX, i - 1);
                // ir drifts
                res[model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)] =
                    -Hi * alphai * alphai + H0 * alpha0 * alphai * rhozz0i - sigmai * alphai * rhozxii;
                // log spot fx drifts (z0, zi independent parts)
                res[model_->pIdx(CrossAssetModel::AssetType::FX, i - 1, 0)] =
                    H0 * alpha0 * sigmai * rhozx0i +
                    model_->irlgm1f(0)->termStructure()->forwardRate(t, t, Continuous) -
                    model_->irlgm1f(i)->termStructure()->forwardRate(t, t, Continuous) - 0.5 * sigmai * sigmai;
                if (model_->measure() == IrModel::Measure::BA) {
                    // REMOVE the LGM measure drift contributions above
                    res[model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)] -= H0 * alpha0 * alphai * rhozz0i;
                    res[model_->pIdx(CrossAssetModel::AssetType::FX, i - 1, 0)] -= H0 * alpha0 * sigmai * rhozx0i;
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
            // Real rhozsik = model_->correlation(EQ, k, CrossAssetModel::AssetType::IR, i); // eq cur
            Real rhozs0k =
                model_->correlation(CrossAssetModel::AssetType::EQ, k, CrossAssetModel::AssetType::IR, 0); // base cur
            // fx-eq corr
            Real rhoxsik =
                (i == 0) ? 0.0 : // no fx process for base-ccy
                    model_->correlation(CrossAssetModel::AssetType::FX, i - 1, CrossAssetModel::AssetType::EQ, k);
            // ir instantaneous forward rate (from curve used for eq forward projection)
            Real fr_i = model_->eqbs(k)->equityIrCurveToday()->forwardRate(t, t, Continuous);
            // div yield instantaneous forward rate
            Real fq_k = model_->eqbs(k)->equityDivYieldCurveToday()->forwardRate(t, t, Continuous);
            res[model_->pIdx(CrossAssetModel::AssetType::EQ, k, 0)] = fr_i - fq_k + (rhozs0k * H0 * alpha0 * sigmask) -
                                                                      (eps_ccy * rhoxsik * sigmaxi * sigmask) -
                                                                      (0.5 * sigmask * sigmask);
        }

        // State independent pieces of JY inflation model, if there is a CAM JY component.
        for (Size j = 0; j < model_->components(CrossAssetModel::AssetType::INF); ++j) {

            if (model_->modelType(CrossAssetModel::AssetType::INF, j) == CrossAssetModel::ModelType::JY) {

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
                Real rho_zy_0j =
                    model_->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::INF, j, 0, 0);
                Real rho_yc_ij =
                    model_->correlation(CrossAssetModel::AssetType::INF, j, CrossAssetModel::AssetType::INF, j, 0, 1);
                Real rho_zc_0j =
                    model_->correlation(CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::INF, j, 0, 1);

                // JY real rate drift. It is state independent
                auto rrDrift = -alpha_y_j * alpha_y_j * H_y_j + rho_zy_0j * alpha0 * alpha_y_j * H_y_j -
                               rho_yc_ij * alpha_y_j * sigma_c_j;

                if (i_j > 0) {
                    Real sigma_x_i_j = model_->fxbs(i_j - 1)->sigma(t);
                    Real rho_yx_j_i_j = model_->correlation(CrossAssetModel::AssetType::INF, j,
                                                            CrossAssetModel::AssetType::FX, i_j - 1, 0, 0);
                    rrDrift -= rho_yx_j_i_j * alpha_y_j * sigma_x_i_j;
                }

                res[model_->pIdx(CrossAssetModel::AssetType::INF, j, 0)] = rrDrift;

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
                    Real rho_cx_j_i_j = model_->correlation(CrossAssetModel::AssetType::INF, j,
                                                            CrossAssetModel::AssetType::FX, i_j - 1, 1, 0);
                    indexDrift -= rho_cx_j_i_j * sigma_c_j * sigma_x_i_j;
                }

                res[model_->pIdx(CrossAssetModel::AssetType::INF, j, 1)] = indexDrift;
            }
        }

        if (timeStepsToCache_m_ > 0) {
            cache_m_.push_back(res);
            if (cache_m_.size() == timeStepsToCache_m_)
                cacheNotReady_m_ = false;
        }
    } else {
        res = cache_m_[timeStepCache_m_++];
        if (timeStepCache_m_ == timeStepsToCache_m_)
            timeStepCache_m_ = 0;
    }
    // non-cacheable sections of drifts
    for (Size i = 1; i < n; ++i) {
        // log spot fx drifts (z0, zi dependent parts)
        Real Hi = model_->irlgm1f(i)->H(t);
        Real Hprimei = model_->irlgm1f(i)->Hprime(t);
        Real zetai = model_->irlgm1f(i)->zeta(t);
        res[model_->pIdx(CrossAssetModel::AssetType::FX, i - 1, 0)] +=
            x[model_->pIdx(CrossAssetModel::AssetType::IR, 0, 0)] * Hprime0 + zeta0 * Hprime0 * H0 -
            x[model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)] * Hprimei - zetai * Hprimei * Hi;
    }
    for (Size k = 0; k < n_eq; ++k) {
        // log equity spot drifts (path-dependent parts)
        // notice the assumption in below that dividend yield curve is static
        Size i = model_->ccyIndex(model_->eqbs(k)->currency());
        // ir params (for equity currency)
        Real Hi = model_->irlgm1f(i)->H(t);
        Real Hprimei = model_->irlgm1f(i)->Hprime(t);
        Real zetai = model_->irlgm1f(i)->zeta(t);
        res[model_->pIdx(CrossAssetModel::AssetType::EQ, k, 0)] +=
            (x[model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)] * Hprimei) + (zetai * Hprimei * Hi);
    }

    // Non-cacheable portion of inflation JY drift, if there is a CAM JY component.
    for (Size j = 0; j < model_->components(CrossAssetModel::AssetType::INF); ++j) {
        if (model_->modelType(CrossAssetModel::AssetType::INF, j) == CrossAssetModel::ModelType::JY) {

            auto p = model_->infjy(j);
            Size i_j = model_->ccyIndex(p->currency());

            // JY inflation parameter values.
            Real Hp_y_j = p->realRate()->Hprime(t);

            // Inflation nominal currency parameter values
            Real Hp_i_j = model_->irlgm1f(i_j)->Hprime(t);

            res[model_->pIdx(CrossAssetModel::AssetType::INF, j, 1)] +=
                x[model_->pIdx(CrossAssetModel::AssetType::IR, i_j, 0)] * Hp_i_j -
                x[model_->pIdx(CrossAssetModel::AssetType::INF, j, 0)] * Hp_y_j;
        }
    }

    // COM drift
    Size n_com = model_->components(CrossAssetModel::AssetType::COM);
    for (Size k = 0; k < n_com; ++k) {
        auto cm = QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzModel>(model_->comModel(k));
        QL_REQUIRE(cm, "CommoditySchwartzModel not set");
        if (!cm->parametrization()->driftFreeState()) {
            // Ornstein-Uhlenbeck drift
            Real kap = cm->parametrization()->kappaParameter();
            res[model_->pIdx(CrossAssetModel::AssetType::COM, k, 0)] -=
                kap * x[model_->pIdx(CrossAssetModel::AssetType::COM, k, 0)];
        } else {
            // zero drift
        }
    }

    /* no drift for infdk, crlgm1f, crstate components */
    return res;
}

Matrix CrossAssetStateProcess::diffusion(Time t, const Array& x) const {
    return diffusionOnCorrelatedBrownians(t, x) * sqrtCorrelation_;
}

Matrix CrossAssetStateProcess::diffusionOnCorrelatedBrownians(Time t, const Array& x) const {
    if (cacheNotReady_d_) {
        Matrix tmp = diffusionOnCorrelatedBrowniansImpl(t, x);
        if (timeStepsToCache_d_ > 0) {
            cache_d_.push_back(tmp);
            if (cache_d_.size() == timeStepsToCache_d_)
                cacheNotReady_d_ = false;
        }
        return tmp;
    } else {
        Matrix tmp = cache_d_[timeStepCache_d_++];
        if (timeStepCache_d_ == timeStepsToCache_d_)
            timeStepCache_d_ = 0;
        return tmp;
    }
}

Matrix CrossAssetStateProcess::diffusionOnCorrelatedBrowniansImpl(Time t, const Array&) const {
    Matrix res(model_->dimension(), model_->brownians(), 0.0);
    Size n = model_->components(CrossAssetModel::AssetType::IR);
    Size m = model_->components(CrossAssetModel::AssetType::FX);
    Size d = model_->components(CrossAssetModel::AssetType::INF);
    Size c = model_->components(CrossAssetModel::AssetType::CR);
    Size e = model_->components(CrossAssetModel::AssetType::EQ);
    Size com = model_->components(CrossAssetModel::AssetType::COM);
    Size crstates = model_->components(CrossAssetModel::AssetType::CrState);
    // ir-ir
    for (Size i = 0; i < n; ++i) {
        Real alphai = model_->irlgm1f(i)->alpha(t);
        setValue2(res, alphai, model_, CrossAssetModel::AssetType::IR, i, CrossAssetModel::AssetType::IR, i, 0, 0);
    }
    // fx-fx
    for (Size i = 0; i < m; ++i) {
        Real sigmai = model_->fxbs(i)->sigma(t);
        setValue2(res, sigmai, model_, CrossAssetModel::AssetType::FX, i, CrossAssetModel::AssetType::FX, i, 0, 0);
    }
    // inf-inf
    for (Size i = 0; i < d; ++i) {
        if (model_->modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::DK) {
            Real alphai = model_->infdk(i)->alpha(t);
            Real Hi = model_->infdk(i)->H(t);
            // DK z diffusion coefficient
            setValue2(res, alphai, model_, CrossAssetModel::AssetType::INF, i, CrossAssetModel::AssetType::INF, i, 0,
                      0);
            // DK y diffusion coefficient
            setValue2(res, alphai * Hi, model_, CrossAssetModel::AssetType::INF, i, CrossAssetModel::AssetType::INF, i,
                      1, 0);
        } else {
            auto p = model_->infjy(i);
            // JY z diffusion coefficient
            setValue2(res, p->realRate()->alpha(t), model_, CrossAssetModel::AssetType::INF, i,
                      CrossAssetModel::AssetType::INF, i, 0, 0);
            // JY I diffusion coefficient
            setValue2(res, p->index()->sigma(t), model_, CrossAssetModel::AssetType::INF, i,
                      CrossAssetModel::AssetType::INF, i, 1, 1);
        }
    }
    for (Size i = 0; i < c; ++i) {
        // Skip CR components that are not LGM
        if (model_->modelType(CrossAssetModel::AssetType::CR, i) != CrossAssetModel::ModelType::LGM1F)
            continue;
        Real alphai = model_->crlgm1f(i)->alpha(t);
        Real Hi = model_->crlgm1f(i)->H(t);
        // crz-crz
        setValue2(res, alphai, model_, CrossAssetModel::AssetType::CR, i, CrossAssetModel::AssetType::CR, i, 0, 0);
        // cry-cry
        setValue2(res, alphai * Hi, model_, CrossAssetModel::AssetType::CR, i, CrossAssetModel::AssetType::CR, i, 1, 0);
    }
    // eq-eq
    for (Size i = 0; i < e; ++i) {
        Real sigmai = model_->eqbs(i)->sigma(t);
        setValue2(res, sigmai, model_, CrossAssetModel::AssetType::EQ, i, CrossAssetModel::AssetType::EQ, i, 0, 0);
    }
    // com-com
    for (Size i = 0; i < com; ++i) {
        Real sigmai = model_->combs(i)->sigma(t);
        setValue2(res, sigmai, model_, CrossAssetModel::AssetType::COM, i, CrossAssetModel::AssetType::COM, i, 0, 0);
    }
    // creditstate-creditstate
    for (Size i = 0; i < crstates; ++i) {
        setValue2(res, 1.0, model_, CrossAssetModel::AssetType::CrState, i, CrossAssetModel::AssetType::CrState, i, 0,
                  0);
    }

    if (model_->measure() == IrModel::Measure::BA) {
        // aux-aux
        Real H0 = model_->irlgm1f(0)->H(t);
        Real alpha0 = model_->irlgm1f(0)->alpha(t);
        setValue2(res, alpha0 * H0, model_, CrossAssetModel::AssetType::IR, 0, CrossAssetModel::AssetType::IR, 0, 1, 0);
    }

    return res;
}

namespace {
Array getProjectedArray(const Array& source, Size start, Size length) {
    QL_REQUIRE(source.size() >= start + length, "getProjectedArray(): internal errors: source size "
                                                    << source.size() << ", start" << start << ", length " << length);
    return Array(std::next(source.begin(), start), std::next(source.begin(), start + length));
}

void applyFxDriftAdjustment(Array& state, const QuantLib::ext::shared_ptr<const CrossAssetModel>& model, Size i, Time t0,
                            Time dt) {

    // the specifics depend on the ir and fx model types and their discretizations

    if (model->modelType(CrossAssetModel::AssetType::IR, i) == CrossAssetModel::ModelType::HW &&
        model->modelType(CrossAssetModel::AssetType::FX, i - 1) == CrossAssetModel::ModelType::BS) {

        QL_REQUIRE(model->discretization() == CrossAssetModel::Discretization::Euler,
                   "applyFxDrifAdjustment(): can only handle discretization Euler at the moment.");
        Matrix corrTmp(model->irModel(i)->m(), 1);
        for (Size k = 0; k < model->irModel(i)->m(); ++k) {
            corrTmp(k, 0) =
                model->correlation(CrossAssetModel::AssetType::IR, i, CrossAssetModel::AssetType::FX, i - 1, k, 0);
        }
        Matrix driftAdj = dt * model->fxbs(i - 1)->sigma(t0) * (transpose(model->irhw(i)->sigma_x(t0)) * corrTmp);
        auto s = std::next(state.begin(), model->pIdx(CrossAssetModel::AssetType::IR, i, 0));
        for (auto c = driftAdj.column_begin(0); c != driftAdj.column_end(0); ++c, ++s) {
            *s += *c;
        }

    } else {
        QL_FAIL("applyFxDriftAdjustment(): can only handle ir model type HW and fx model type BS currently.");
    }
}
} // namespace

Array CrossAssetStateProcess::evolve(Time t0, const Array& x0, Time dt, const Array& dw) const {

    Array res;

    // handle HW based model

    if (model_->modelType(CrossAssetModel::AssetType::IR, 0) == CrossAssetModel::ModelType::HW) {

        QL_REQUIRE(model_->discretization() == CrossAssetModel::Discretization::Euler,
                   "CrossAssetStateProcess::evolve(): hw-based model only supports Euler discretization.");

        const Array dz = sqrtCorrelation_ * dw;

        res = Array(model_->dimension());

        // eolve ir processes and store current short rates which are needed to evolve the fx components below

        Array shortRates(model_->components(CrossAssetModel::AssetType::IR));
        for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::IR); ++i) {
            auto p = model_->irModel(i)->stateProcess();
            auto r = p->evolve(t0,
                               getProjectedArray(x0, model_->pIdx(CrossAssetModel::AssetType::IR, i, 0),
                                                 model_->irModel(i)->n() + model_->irModel(i)->n_aux()),
                               dt,
                               getProjectedArray(dz, model_->wIdx(CrossAssetModel::AssetType::IR, i, 0),
                                                 model_->irModel(i)->m() + model_->irModel(i)->m_aux()));
            std::copy(r.begin(), r.end(), std::next(res.begin(), model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)));
            shortRates[i] = model_->irModel(i)->shortRate(
                t0, getProjectedArray(x0, model_->pIdx(CrossAssetModel::AssetType::IR, i, 0), model_->irModel(i)->n()));
        }

        // apply drift adjustment to ir processes in non-domestic currency

        for (Size i = 1; i < model_->components(CrossAssetModel::AssetType::IR); ++i) {
            applyFxDriftAdjustment(res, model_, i, t0, dt);
        }

        // eolve fx processes

        for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::FX); ++i) {
            auto r = model_->fxModel(i)->eulerStep(
                t0, getProjectedArray(x0, model_->pIdx(CrossAssetModel::AssetType::FX, i, 0), model_->fxModel(i)->n()),
                dt, getProjectedArray(dz, model_->wIdx(CrossAssetModel::AssetType::FX, i, 0), model_->fxModel(i)->m()),
                shortRates[0], shortRates[i + 1]);
            std::copy(r.begin(), r.end(), std::next(res.begin(), model_->pIdx(CrossAssetModel::AssetType::FX, i, 0)));
        }

        // evolve com processes

        for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::COM); ++i) {
            auto p = model_->comModel(i)->stateProcess();
            auto r = p->evolve(
                t0,
                getProjectedArray(x0, model_->pIdx(CrossAssetModel::AssetType::COM, i, 0), model_->comModel(i)->n()),
                dt,
                getProjectedArray(dz, model_->wIdx(CrossAssetModel::AssetType::COM, i, 0), model_->comModel(i)->m()));
            std::copy(r.begin(), r.end(), std::next(res.begin(), model_->pIdx(CrossAssetModel::AssetType::COM, i, 0)));
        }

        // TODO other components ...
        QL_REQUIRE(model_->components(CrossAssetModel::AssetType::IR) +
                           model_->components(CrossAssetModel::AssetType::FX) +
                           model_->components(CrossAssetModel::AssetType::COM) ==
                       model_->parametrizations().size(),
                   "CrossAssetStateProcess::evolve(): currently only IR, FX, COM supported.");

        return res;
    }

    // handle LGM1F based model

    if (model_->discretization() == CrossAssetModel::Discretization::Euler) {
        const Array dz = sqrtCorrelation_ * dw;
        const Matrix df = diffusionOnCorrelatedBrownians(t0, x0);
        res = apply(expectation(t0, x0, dt), df * dz * std::sqrt(dt));

        // CR CIRPP components
        if (cirppCount_ > 0) {
            for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::CR); ++i) {
                if (!crCirpp_[i])
                    continue; // ignore non-cir cr model
                Size idx1 = model_->pIdx(CrossAssetModel::AssetType::CR, i, 0);
                Size idx2 = model_->pIdx(CrossAssetModel::AssetType::CR, i, 1);
                Size idxw = model_->wIdx(CrossAssetModel::AssetType::CR, i, 0);
                Array x0Tmp(2), dwTmp(2);
                x0Tmp[0] = x0[idx1];
                x0Tmp[1] = x0[idx2];
                dwTmp[0] = dz[idxw];
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

CrossAssetStateProcess::ExactDiscretization::ExactDiscretization(QuantLib::ext::shared_ptr<const CrossAssetModel> model,
                                                                 SalvagingAlgorithm::Type salvaging)
    : model_(std::move(model)), salvaging_(salvaging) {

    QL_REQUIRE(model_->modelType(CrossAssetModel::AssetType::IR, 0) == CrossAssetModel::ModelType::LGM1F,
               "CrossAssetStateProces::ExactDiscretization is only supported by LGM1F IR model types.");
}

Array CrossAssetStateProcess::ExactDiscretization::drift(const StochasticProcess& p, Time t0, const Array& x0,
                                                         Time dt) const {
    Array res;
    if (cacheNotReady_m_) {
        res = driftImpl1(p, t0, x0, dt);
        if (timeStepsToCache_m_ > 0) {
            cache_m_.push_back(res);
            if (cache_m_.size() == timeStepsToCache_m_)
                cacheNotReady_m_ = false;
        }
    } else {
        res = cache_m_[timeStepCache_m_++];
        if (timeStepCache_m_ == timeStepsToCache_m_)
            timeStepCache_m_ = 0;
    }
    Array res2 = driftImpl2(p, t0, x0, dt);
    for (Size i = 0; i < res.size(); ++i) {
        res[i] += res2[i];
    }
    return res - x0;
}

Matrix CrossAssetStateProcess::ExactDiscretization::diffusion(const StochasticProcess& p, Time t0, const Array& x0,
                                                              Time dt) const {
    if (cacheNotReady_d_) {
        Matrix res = pseudoSqrt(covariance(p, t0, x0, dt), salvaging_);
        // note that covariance actually does not depend on x0
        if (timeStepsToCache_d_ > 0) {
            cache_d_.push_back(res);
            if (cache_d_.size() == timeStepsToCache_d_)
                cacheNotReady_d_ = false;
        }
        return res;
    } else {
        Matrix res = cache_d_[timeStepCache_d_++];
        if (timeStepCache_d_ == timeStepsToCache_d_)
            timeStepCache_d_ = 0;
        return res;
    }
}

Matrix CrossAssetStateProcess::ExactDiscretization::covariance(const StochasticProcess& p, Time t0, const Array& x0,
                                                               Time dt) const {
    if (cacheNotReady_v_) {
        Matrix res = covarianceImpl(p, t0, x0, dt);
        if (timeStepsToCache_v_ > 0) {
            cache_v_.push_back(res);
            if (cache_v_.size() == timeStepsToCache_v_)
                cacheNotReady_v_ = false;
        }
        return res;
    } else {
        Matrix res = cache_v_[timeStepCache_v_++];
        if (timeStepCache_v_ == timeStepsToCache_v_)
            timeStepCache_v_ = 0;
        return res;
    }
}

Array CrossAssetStateProcess::ExactDiscretization::driftImpl1(const StochasticProcess&, Time t0, const Array&,
                                                              Time dt) const {
    Size n = model_->components(CrossAssetModel::AssetType::IR);
    Size m = model_->components(CrossAssetModel::AssetType::FX);
    Size e = model_->components(CrossAssetModel::AssetType::EQ);
    Array res(model_->dimension(), 0.0);
    for (Size i = 0; i < n; ++i) {
        res[model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)] = ir_expectation_1(*model_, i, t0, dt);
    }
    for (Size j = 0; j < m; ++j) {
        res[model_->pIdx(CrossAssetModel::AssetType::FX, j, 0)] = fx_expectation_1(*model_, j, t0, dt);
    }
    for (Size k = 0; k < e; ++k) {
        res[model_->pIdx(CrossAssetModel::AssetType::EQ, k, 0)] = eq_expectation_1(*model_, k, t0, dt);
    }

    // If inflation is JY, need to take account of the drift.
    for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::INF); ++i) {
        if (model_->modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::JY) {
            std::tie(res[model_->pIdx(CrossAssetModel::AssetType::INF, i, 0)],
                     res[model_->pIdx(CrossAssetModel::AssetType::INF, i, 1)]) =
                inf_jy_expectation_1(*model_, i, t0, dt);
        }
    }

    /* no COM driftImpl1 contribution for one-factor non mean-reverting commodity case,
       no crstate contribution */

    return res;
}

Array CrossAssetStateProcess::ExactDiscretization::driftImpl2(const StochasticProcess&, Time t0, const Array& x0,
                                                              Time dt) const {
    Size n = model_->components(CrossAssetModel::AssetType::IR);
    Size m = model_->components(CrossAssetModel::AssetType::FX);
    Size e = model_->components(CrossAssetModel::AssetType::EQ);
    Array res(model_->dimension(), 0.0);

    if (model_->measure() == IrModel::Measure::BA) {
        // zero AUX state drift, i.e. conditional expectation equal to the previous level as for z_i
        res[model_->pIdx(CrossAssetModel::AssetType::IR, 0, 1)] +=
            x0[model_->pIdx(CrossAssetModel::AssetType::IR, 0, 1)];
    }

    for (Size i = 0; i < n; ++i) {
        res[model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)] +=
            ir_expectation_2(*model_, i, x0[model_->pIdx(CrossAssetModel::AssetType::IR, i, 0)]);
    }
    for (Size j = 0; j < m; ++j) {
        res[model_->pIdx(CrossAssetModel::AssetType::FX, j, 0)] +=
            fx_expectation_2(*model_, j, t0, x0[model_->pIdx(CrossAssetModel::AssetType::FX, j, 0)],
                             x0[model_->pIdx(CrossAssetModel::AssetType::IR, j + 1, 0)],
                             x0[model_->pIdx(CrossAssetModel::AssetType::IR, 0, 0)], dt);
    }
    for (Size k = 0; k < e; ++k) {
        Size eqCcyIdx = model_->ccyIndex(model_->eqbs(k)->currency());
        res[model_->pIdx(CrossAssetModel::AssetType::EQ, k, 0)] +=
            eq_expectation_2(*model_, k, t0, x0[model_->pIdx(CrossAssetModel::AssetType::EQ, k, 0)],
                             x0[model_->pIdx(CrossAssetModel::AssetType::IR, eqCcyIdx, 0)], dt);
    }

    // Inflation: JY is state dependent. DK is not. Even for DK, still need to return the conditional expected value.
    for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::INF); ++i) {
        if (model_->modelType(CrossAssetModel::AssetType::INF, i) == CrossAssetModel::ModelType::JY) {
            auto i_i = model_->ccyIndex(model_->infjy(i)->currency());
            auto zi_i_0 = x0[model_->pIdx(CrossAssetModel::AssetType::IR, i_i, 0)];
            auto state_0 = std::make_pair(x0[model_->pIdx(CrossAssetModel::AssetType::INF, i, 0)],
                                          x0[model_->pIdx(CrossAssetModel::AssetType::INF, i, 1)]);
            std::tie(res[model_->pIdx(CrossAssetModel::AssetType::INF, i, 0)],
                     res[model_->pIdx(CrossAssetModel::AssetType::INF, i, 1)]) =
                inf_jy_expectation_2(*model_, i, t0, state_0, zi_i_0, dt);
        } else {
            res[model_->pIdx(CrossAssetModel::AssetType::INF, i, 0)] =
                x0[model_->pIdx(CrossAssetModel::AssetType::INF, i, 0)];
            res[model_->pIdx(CrossAssetModel::AssetType::INF, i, 1)] =
                x0[model_->pIdx(CrossAssetModel::AssetType::INF, i, 1)];
        }
    }

    /*! cr components have integrated drift 0, we have to return the conditional
        expected value though, since x0 is subtracted later */
    Size c = model_->components(CrossAssetModel::AssetType::CR);
    for (Size i = 0; i < c; ++i) {
        res[model_->pIdx(CrossAssetModel::AssetType::CR, i, 0)] =
            x0[model_->pIdx(CrossAssetModel::AssetType::CR, i, 0)];
        res[model_->pIdx(CrossAssetModel::AssetType::CR, i, 1)] =
            x0[model_->pIdx(CrossAssetModel::AssetType::CR, i, 1)];
    }

    /* commodity components are drift-free in the one-factor non mean-reverting case */
    Size com = model_->components(CrossAssetModel::AssetType::COM);
    for (Size i = 0; i < com; ++i) {
        // res[model_->pIdx(CrossAssetModel::AssetType::COM, i, 0)] =
        //     x0[model_->pIdx(CrossAssetModel::AssetType::COM, i, 0)];
        auto cm = QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzModel>(model_->comModel(i));
        QL_REQUIRE(cm, "CommoditySchwartzModel not set");
        Real com0 = x0[model_->pIdx(CrossAssetModel::AssetType::COM, i, 0)];
        if (cm->parametrization()->driftFreeState()) {
            res[model_->pIdx(CrossAssetModel::AssetType::COM, i, 0)] = com0;
        } else {
            Real kap = cm->parametrization()->kappaParameter();
            res[model_->pIdx(CrossAssetModel::AssetType::COM, i, 0)] = com0 * std::exp(-kap * dt);
        }
    }

    for (Size j = 0; j < model_->components(CrossAssetModel::AssetType::CrState); ++j) {
        Size idx = model_->pIdx(CrossAssetModel::AssetType::CrState, j, 0);
        res[idx] += x0[idx]; // drift free
    }

    return res;
}

Matrix CrossAssetStateProcess::ExactDiscretization::covarianceImpl(const StochasticProcess&, Time t0, const Array&,
                                                                   Time dt) const {
    Matrix res(model_->dimension(), model_->dimension(), 0.0);
    Size n = model_->components(CrossAssetModel::AssetType::IR);
    Size m = model_->components(CrossAssetModel::AssetType::FX);
    Size d = model_->components(CrossAssetModel::AssetType::INF);
    Size c = model_->components(CrossAssetModel::AssetType::CR);
    Size e = model_->components(CrossAssetModel::AssetType::EQ);
    Size com = model_->components(CrossAssetModel::AssetType::COM);
    Size u = model_->components(CrossAssetModel::AssetType::CrState);

    if (model_->measure() == IrModel::Measure::BA) {
        // aux-aux
        setValue(res, aux_aux_covariance(*model_, t0, dt), model_, CrossAssetModel::AssetType::IR, 0,
                 CrossAssetModel::AssetType::IR, 0, 1, 1);
        // aux-ir
        for (Size j = 0; j < n; ++j) {
            setValue(res, aux_ir_covariance(*model_, j, t0, dt), model_, CrossAssetModel::AssetType::IR, 0,
                     CrossAssetModel::AssetType::IR, j, 1, 0);
        }
        // aux-fx
        for (Size j = 0; j < m; ++j) {
            setValue(res, aux_fx_covariance(*model_, j, t0, dt), model_, CrossAssetModel::AssetType::IR, 0,
                     CrossAssetModel::AssetType::FX, j, 1, 0);
        }
    }
    // ir-ir
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j <= i; ++j) {
            setValue(res, ir_ir_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::IR, j, 0, 0);
        }
    }
    // ir-fx
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < m; ++j) {
            setValue(res, ir_fx_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::FX, j, 0, 0);
        }
    }
    // fx-fx
    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j <= i; ++j) {
            setValue(res, fx_fx_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::FX, i,
                     CrossAssetModel::AssetType::FX, j);
        }
    }
    // ir,fx,inf - inf
    for (Size j = 0; j < d; ++j) {
        for (Size i = 0; i <= j; ++i) {
            // infz-infz
            setValue(res, infz_infz_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::INF, j, 0, 0);
            // infz-infy
            setValue(res, infz_infy_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::INF, j, 0, 1);
            setValue(res, infz_infy_covariance(*model_, j, i, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::INF, j, 1, 0);
            // infy-infy
            setValue(res, infy_infy_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::INF, j, 1, 1);
        }
        for (Size i = 0; i < n; ++i) {
            // ir-inf
            setValue(res, ir_infz_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::INF, j, 0, 0);
            setValue(res, ir_infy_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::INF, j, 0, 1);
        }
        for (Size i = 0; i < (n - 1); ++i) {
            // fx-inf
            setValue(res, fx_infz_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::FX, i,
                     CrossAssetModel::AssetType::INF, j, 0, 0);
            setValue(res, fx_infy_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::FX, i,
                     CrossAssetModel::AssetType::INF, j, 0, 1);
        }
    }
    // ir,fx,inf,cr - cr
    for (Size j = 0; j < c; ++j) {
        // Skip CR components that are not LGM
        if (model_->modelType(CrossAssetModel::AssetType::CR, j) != CrossAssetModel::ModelType::LGM1F)
            continue;
        for (Size i = 0; i <= j; ++i) {
            // Skip CR components that are not LGM
            if (model_->modelType(CrossAssetModel::AssetType::CR, i) != CrossAssetModel::ModelType::LGM1F)
                continue;
            // crz-crz
            setValue(res, crz_crz_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::CR, i,
                     CrossAssetModel::AssetType::CR, j, 0, 0);
            // crz-cry
            setValue(res, crz_cry_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::CR, i,
                     CrossAssetModel::AssetType::CR, j, 0, 1);
            setValue(res, crz_cry_covariance(*model_, j, i, t0, dt), model_, CrossAssetModel::AssetType::CR, i,
                     CrossAssetModel::AssetType::CR, j, 1, 0);
            // cry-cry
            setValue(res, cry_cry_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::CR, i,
                     CrossAssetModel::AssetType::CR, j, 1, 1);
        }
        for (Size i = 0; i < n; ++i) {
            // ir-cr
            setValue(res, ir_crz_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::CR, j, 0, 0);
            setValue(res, ir_cry_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::CR, j, 0, 1);
        }
        for (Size i = 0; i < (n - 1); ++i) {
            // fx-cr
            setValue(res, fx_crz_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::FX, i,
                     CrossAssetModel::AssetType::CR, j, 0, 0);
            setValue(res, fx_cry_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::FX, i,
                     CrossAssetModel::AssetType::CR, j, 0, 1);
        }
        for (Size i = 0; i < d; ++i) {
            // inf-cr
            setValue(res, infz_crz_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::CR, j, 0, 0);
            setValue(res, infy_crz_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::CR, j, 1, 0);
            setValue(res, infz_cry_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::CR, j, 0, 1);
            setValue(res, infy_cry_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::CR, j, 1, 1);
        }
    }

    // ir,fx,inf,cr,eq - eq
    for (Size j = 0; j < e; ++j) {
        for (Size i = 0; i <= j; ++i) {
            // eq-eq
            setValue(res, eq_eq_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::EQ, i,
                     CrossAssetModel::AssetType::EQ, j, 0, 0);
        }
        for (Size i = 0; i < n; ++i) {
            // ir-eq
            setValue(res, ir_eq_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::EQ, j, 0, 0);
        }
        for (Size i = 0; i < (n - 1); ++i) {
            // fx-eq
            setValue(res, fx_eq_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::FX, i,
                     CrossAssetModel::AssetType::EQ, j, 0, 0);
        }
        for (Size i = 0; i < d; ++i) {
            // inf-eq
            setValue(res, infz_eq_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::EQ, j, 0, 0);
            setValue(res, infy_eq_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::EQ, j, 1, 0);
        }
        for (Size i = 0; i < c; ++i) {
            // Skip CR components that are not LGM
            if (model_->modelType(CrossAssetModel::AssetType::CR, i) != CrossAssetModel::ModelType::LGM1F)
                continue;
            // cr-eq
            setValue(res, crz_eq_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::CR, i,
                     CrossAssetModel::AssetType::EQ, j, 0, 0);
            setValue(res, cry_eq_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::CR, i,
                     CrossAssetModel::AssetType::EQ, j, 1, 0);
        }
    }

    // ir,fx,inf,cr,eq, com - com
    for (Size j = 0; j < com; ++j) {
        for (Size i = 0; i <= j; ++i) {
            // com-com
            setValue(res, com_com_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::COM, i,
                     CrossAssetModel::AssetType::COM, j, 0, 0);
        }
        for (Size i = 0; i < n; ++i) {
            // ir-com
            setValue(res, ir_com_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::COM, j, 0, 0);
        }
        for (Size i = 0; i < (n - 1); ++i) {
            // fx-com
            setValue(res, fx_com_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::FX, i,
                     CrossAssetModel::AssetType::COM, j, 0, 0);
        }
        for (Size i = 0; i < d; ++i) {
            // inf-com
            setValue(res, infz_com_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::COM, j, 0, 0);
            setValue(res, infy_com_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::INF, i,
                     CrossAssetModel::AssetType::COM, j, 1, 0);
        }
        for (Size i = 0; i < c; ++i) {
            // Skip CR components that are not LGM
            if (model_->modelType(CrossAssetModel::AssetType::CR, i) != CrossAssetModel::ModelType::LGM1F)
                continue;
            // cr-com
            setValue(res, crz_com_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::CR, i,
                     CrossAssetModel::AssetType::COM, j, 0, 0);
            setValue(res, cry_com_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::CR, i,
                     CrossAssetModel::AssetType::COM, j, 1, 0);
        }
        for (Size i = 0; i < e; ++i) {
            // eq-com
            setValue(res, eq_com_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::EQ, i,
                     CrossAssetModel::AssetType::COM, j, 0, 0);
        }
    }

    // ir, fx, creditstate - creditstate
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < u; ++j) {
            setValue(res, ir_crstate_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::IR, i,
                     CrossAssetModel::AssetType::CrState, j, 0, 0);
        }
    }
    for (Size i = 0; i < m; ++i) {
        for (Size j = 0; j < u; ++j) {
            setValue(res, fx_crstate_covariance(*model_, i, j, t0, dt), model_, CrossAssetModel::AssetType::FX, i,
                     CrossAssetModel::AssetType::CrState, j, 0, 0);
        }
    }
    for (Size i = 0; i < u; ++i) {
        for (Size j = 0; j <= i; ++j) {
            setValue(res, crstate_crstate_covariance(*model_, i, j, t0, dt), model_,
                     CrossAssetModel::AssetType::CrState, i, CrossAssetModel::AssetType::CrState, j, 0, 0);
        }
    }

    return res;
}

void CrossAssetStateProcess::ExactDiscretization::resetCache(const Size timeSteps) const {
    cacheNotReady_m_ = cacheNotReady_d_ = cacheNotReady_v_ = true;
    timeStepsToCache_m_ = timeStepsToCache_d_ = timeStepsToCache_v_ = timeSteps;
    timeStepCache_m_ = timeStepCache_d_ = timeStepCache_v_ = 0;
    cache_m_.clear();
    cache_v_.clear();
    cache_d_.clear();
}

} // namespace QuantExt
