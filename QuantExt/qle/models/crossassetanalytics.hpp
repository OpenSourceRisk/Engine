/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file crossassetanalytics.hpp
    \brief analytics for the cross asset model
*/

#ifndef quantext_crossasset_analytics_hpp
#define quantext_crossasset_analytics_hpp

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/crossassetanalyticsbase.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace CrossAssetAnalytics {

/*! ir state expectation, part that is independent of current state */
Real ir_expectation_1(const CrossAssetModel *model, const Size i, const Time t0,
                      const Real dt);

/*! ir state expecation, part that is dependent on current state */
Real ir_expectation_2(const CrossAssetModel *model, const Size,
                      const Real zi_0);

/*! fx state expectation, part that is independent of current state */
Real fx_expectation_1(const CrossAssetModel *model, const Size i, const Time t0,
                      const Real dt);

/*! fx state expectation, part that is dependent on current state */
Real fx_expectation_2(const CrossAssetModel *model, const Size i, const Time t0,
                      const Real xi_0, const Real zi_0, const Real z0_0,
                      const Real dt);

/*! ir-ir covariance */
Real ir_ir_covariance(const CrossAssetModel *model, const Size i, const Size j,
                      const Time t0, const Time dt);

/*! ir-fx covariance */
Real ir_fx_covariance(const CrossAssetModel *model, const Size i, const Size j,
                      const Time t0, const Time dt);

/*! fx-fx covariance */
Real fx_fx_covariance(const CrossAssetModel *model, const Size i, const Size j,
                      const Time t0, const Time dt);

/*! IR H component */
struct Hz {
    Hz(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel *x, const Real t) const {
        return x->irlgm1f(i_)->H(t);
    }
    const Size i_;
};

/*! IR alpha component */
struct az {
    az(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel *x, const Real t) const {
        return x->irlgm1f(i_)->alpha(t);
    }
    const Size i_;
};

/*! IR zeta component */
struct zetaz {
    zetaz(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel *x, const Real t) const {
        return x->irlgm1f(i_)->zeta(t);
    }
    const Size i_;
};

/*! FX sigma component */
struct sx {
    sx(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel *x, const Real t) const {
        return x->fxbs(i_)->sigma(t);
    }
    const Size i_;
};

/*! FX variance component */
struct vx {
    vx(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel *x, const Real t) const {
        return x->fxbs(i_)->variance(t);
    }
    const Size i_;
};

/*! IR-IR correlation component */
struct rzz {
    rzz(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel *x, const Real) const {
        return x->correlation(IR, i_, IR, j_, 0, 0);
    }
    const Size i_, j_;
};

/*! IR-FX correlation component */
struct rzx {
    rzx(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel *x, const Real) const {
        return x->correlation(IR, i_, FX, j_, 0, 0);
    }
    const Size i_, j_;
};

/*! FX-FX correlation component */
struct rxx {
    rxx(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel *x, const Real) const {
        return x->correlation(FX, i_, FX, j_, 0, 0);
    }
    const Size i_, j_;
};

} // namespace CrossAssetAnalytics

} // namesapce QuantExt

#endif
