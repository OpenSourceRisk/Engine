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

void XAssetStateProcess::flushCache() {}

Disposable<Array> XAssetStateProcess::initialValues() const {
    Array res(model_->dimension(), 0.0);
    /*! irlgm1f processes have initial value 0 */
    for (Size i = 0; i < model_->currencies() - 1; ++i) {
        /*! fxbs processes are in log spot */
        res[model_->currencies() + i] =
            std::log(model_->fxbs(i)->fxSpotToday()->value());
    }
    return res;
}

Disposable<Array> XAssetStateProcess::drift(Time t, const Array &x) const {
    Array res(model_->dimension(), 0.0);
    Real H0 = model_->irlgm1f(0)->H(t);
    Real Hprime0 = model_->irlgm1f(0)->Hprime(t);
    Real alpha0 = model_->irlgm1f(0)->alpha(t);
    Real zeta0 = model_->irlgm1f(0)->zeta(t);
    /*! z0 has drift 0 */
    for (Size i = 1; i < model_->currencies(); ++i) {
        Real Hi = model_->irlgm1f(i)->H(t);
        Real Hprimei = model_->irlgm1f(i)->Hprime(t);
        Real alphai = model_->irlgm1f(i)->alpha(t);
        Real zetai = model_->irlgm1f(0)->zeta(t);
        Real sigmai = model_->fxbs(i - 1)->sigma(t);
        // ir-ir
        Real rho0i = model_->correlation()[0][i];
        // ir-fx
        Real rhoii = model_->correlation()[i][model_->currencies() + i - 1];
        // ir drifts
        res[i] = -Hi * alphai * alphai + H0 * alpha0 * alphai * rho0i -
                 sigmai * alphai * rhoii;
        // log spot fx drifts
        res[model_->currencies() + i - 1] =
            H0 * alpha0 * sigmai * rho0i +
            model_->irlgm1f(0)->termStructure()->forwardRate(t, t, Continuous) +
            x[0] * Hprime0 + zeta0 * Hprime0 * H0 -
            model_->irlgm1f(i)->termStructure()->forwardRate(t, t, Continuous) +
            x[i] * Hprimei + zetai * Hprimei * Hi - -0.5 * sigmai * sigmai;
    }
    return res;
}

Disposable<Matrix> XAssetStateProcess::diffusion(Time t, const Array &) const {
    Matrix res(model_->dimension(), model_->dimension());
    Size n = model_->currencies();
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j <= i; ++j) {
            Real alphai = model_->irlgm1f(i)->alpha(t);
            Real alphaj = model_->irlgm1f(j)->alpha(t);
            Real rhozz = model_->correlation()[i][j];
            // ir-ir
            res[i][j] = res[j][i] = alphai * alphaj * rhozz;
            // ir-fx
            if (i > 0) {
                Real sigmai = model_->fxbs(i - 1)->sigma(t);
                Real rhozx = model_->correlation()[j][n + i - 1];
                res[j][n + i - 1] = res[n + i - 1][j] = alphaj * sigmai * rhozx;
                // fx-fx
                if (j > 0) {
                    Real sigmaj = model_->fxbs(j - 1)->sigma(t);
                    Real rhoxx = model_->correlation()[n + i - 1][n + j - 1];
                    res[n + i - 1][n + j - 1] = res[n + j - 1][n + i - 1] =
                        sigmai * sigmaj * rhoxx;
                }
            }
        }
    }
    Matrix tmp = pseudoSqrt(res, SalvagingAlgorithm::Spectral);
    return tmp;
}

XAssetStateProcess::ExactDiscretization::ExactDiscretization(
    const XAssetModel *const model)
    : model_(model) {}

Disposable<Array> XAssetStateProcess::ExactDiscretization::drift(
    const StochasticProcess &p, Time t0, const Array &x0, Time dt) const {
    Size n = model_->currencies();
    Array res(model_->dimension(), 0.0);
    for (Size i = 0; i < model_->currencies(); ++i) {
        res[i] = model_->ir_expectation(i, t0, x0[i], dt);
        if (i > 0) {
            res[n + i - 1] = model_->fx_expectation(i - 1, t0, x0[n + i - 1],
                                                    x0[i], x0[0], dt);
        }
    }
    return res;
}

Disposable<Matrix> XAssetStateProcess::ExactDiscretization::diffusion(
    const StochasticProcess &p, Time t0, const Array &x0, Time dt) const {
    Matrix res =
        pseudoSqrt(covariance(p, t0, x0, dt), SalvagingAlgorithm::Spectral);
    return res;
}

Disposable<Matrix> XAssetStateProcess::ExactDiscretization::covariance(
    const StochasticProcess &p, Time t0, const Array &x0, Time dt) const {
    Matrix res(model_->dimension(), model_->dimension());
    Size n = model_->currencies();
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j <= i; ++j) {
            res[i][j] = res[j][i] = model_->ir_ir_covariance(i, j, t0, dt);
            if (i > 0) {
                res[j][n + i - 1] = res[n + i - 1][j] =
                    model_->ir_fx_covariance(j, i - 1, t0, dt);
                if (j > 0) {
                    res[n + i - 1][n + j - 1] = res[n + j - 1][n + i - 1] =
                        model_->fx_fx_covariance(i - 1, j - 1, t0, dt);
                }
            }
        }
    }
    return res;
}

} // namespace QuantExt
