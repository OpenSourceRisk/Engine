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

/*
Section 16.1 in Lichters, Stamm, Gallagher lists the analytical expectations and covariances 
implemented in this namespace:

Since $z_i$ obeys
\begin{align*}
dz_i &= \epsilon_i\,\gamma_i\,dt + \alpha^z_i\,dW^z_i, \\
\intertext{we have}
\Delta z_i &= -\int_s^t H^z_i\,(\alpha^z_i)^2\,du + \rho^{zz}_{0i} \int_s^t H^z_0\,\alpha^z_0\,\alpha^z_i\,du\\
&\qquad {}- \epsilon_i  \rho^{zx}_{ii}\int_s^t \sigma_i^x\,\alpha^z_i\,du + \int_s^t \alpha^z_i\,dW^z_i. \\
\intertext{Thus,}
\E[\Delta z_i] =& -\int_s^t H^z_i\,(\alpha^z_i)^2\,du + \rho^{zz}_{0i} \int_s^t H^z_0\,\alpha^z_0\,\alpha^z_i\,du\\
&\qquad {} - \epsilon_i  \rho^{zx}_{ii}\int_s^t \sigma_i^x\,\alpha^z_i\,du \\%\label{eq:meanZ}\\
%& \nonumber\\
\mathrm{Cov}[\Delta z_a, \Delta z_b] =& \rho^{zz}_{ab} \int_s^t \alpha^z_a\,\alpha^z_b\,du\\%\label{eq:covZZ}
\end{align*}

Since $x_i$ obeys
\[
dx_i / x_i = \mu^x_i \, dt +\sigma_i^x\,dW^x_i,
\]
we have

\begin{align}
\Delta \ln x_i =& \ln \left( \frac{P^n_0(0,s)}{P^n_0(0,t)} \frac{P^n_i(0,t)}{P^n_i(0,s)}\right) - \frac12 \int_s^t (\sigma^x_i)^2\,du + \rho^{zx}_{0i}\int_s^t H^z_0\, \alpha^z_0\, \sigma^x_i \,du\nonumber\\
&+\int_s^t \zeta^z_0\,H^z_0\, (H^z_0)^{\prime}\,du-\int_s^t \zeta^z_i\,H^z_i\, (H^z_i)^{\prime}\,du\nonumber\\
%&+\underbrace{ \int_s^t \zeta^z_0\,H^z_0\, (H^z_0)^{\prime}\,du-\int_s^t \zeta^z_i\,H^z_i\, (H^z_i)^{\prime}\,du}_{
%\begin{subarray}{l} =\frac12 \left((H^z_0)^2(t) \zeta^z_0(t) -  (H^z_0)^2(s) \zeta^z_0(s)- \int_s^t (H^z_0)^2 (\alpha^z_0)^2\,du\right)\nonumber\\
%\qquad- \frac12 \left((H^z_i)^2(t) \zeta^z_i(t) -  (H^z_i)^2(s) \zeta^z_i(s)-\int_s^t (H^z_i)^2 (\alpha^z_i)^2\,du \right)
%\end{subarray}}\\
&+ \int_s^t \left(H^z_0(t)-H^z_0\right)\alpha_0^z\,dW^z_0+ \left(H^z_0(t)-H^z_0(s)\right) z_0(s) \nonumber\\
&- \int_s^t \left(H^z_i(t)-H^z_i\right)\alpha_i^z\,dW^z_i  -\left(H^z_i(t)-H^z_i(s)\right)z_i(s) \nonumber\\
&- \int_s^t \left(H^z_i(t)-H^z_i\right)\gamma_i\,du + \int_s^t\sigma^x_i dW^x_i \nonumber
\end{align}
Integration by parts yields
\begin{align*}
& \int_s^t \zeta^z_0\,H^z_0\, (H^z_0)^{\prime}\,du-\int_s^t \zeta^z_i\,H^z_i\, (H^z_i)^{\prime}\,du\\
& = \frac12 \left((H^z_0(t))^2 \zeta^z_0(t) -  (H^z_0(s))^2 \zeta^z_0(s)- \int_s^t (H^z_0)^2 (\alpha^z_0)^2\,du\right)\nonumber\\
&\qquad {}- \frac12 \left((H^z_i(t))^2 \zeta^z_i(t) -  (H^z_i(s))^2 \zeta^z_i(s)-\int_s^t (H^z_i)^2 (\alpha^z_i)^2\,du \right)
\end{align*}
so that the expectation is
\begin{align}
\E[\Delta \ln x_i] =& \ln \left( \frac{P^n_0(0,s)}{P^n_0(0,t)} \frac{P^n_i(0,t)}{P^n_i(0,s)}\right) - \frac12 \int_s^t (\sigma^x_i)^2\,du + \rho^{zx}_{0i} \int_s^t H^z_0\, \alpha^z_0\, \sigma^x_i\,du\nonumber\\
&+\frac12 \left((H^z_0(t))^2 \zeta^z_0(t) -  (H^z_0(s))^2 \zeta^z_0(s)- \int_s^t (H^z_0)^2 (\alpha^z_0)^2\,du\right)\nonumber\\
&-\frac12 \left((H^z_i(t))^2 \zeta^z_i(t) -  (H^z_i(s))^2 \zeta^z_i(s)-\int_s^t (H^z_i)^2 (\alpha^z_i)^2\,du \right)\nonumber\\
&+ \left(H^z_0(t)-H^z_0(s)\right) z_0(s) -\left(H^z_i(t)-H^z_i(s)\right)z_i(s)\nonumber\\
&  - \int_s^t \left(H^z_i(t)-H^z_i\right)\gamma_i \,du, \label{eq:meanX} 
\end{align}
and IR-FX and FX-FX covariances are
\begin{align}
\mathrm{Cov}[\Delta \ln x_a, \Delta \ln x_b] =&  \int_s^t \left(H^z_0(t)-H^z_0\right)^2 (\alpha_0^z)^2\,du \nonumber\\
&- \rho^{zz}_{0b}\int_s^t \left(H^z_0(t)-H^z_0\right)\alpha_0^z \left(H^z_b(t)-H^z_b\right)\alpha_b^z\,du \nonumber\\
&+ \rho^{zx}_{0b}\int_s^t \left(H^z_0(t)-H^z_0\right)\alpha_0^z \sigma^x_b\,du \nonumber\\
& -\rho^{zz}_{0a} \int_s^t \left(H^z_a(t)-H^z_a\right) \alpha_a^z\left(H^z_0(t)-H^z_0\right) \alpha_0^z\,du \nonumber\\
&+ \rho^{zz}_{ab}\int_s^t \left(H^z_a(t)-H^z_a\right)\alpha_a^z \left(H^z_b(t)-H^z_b\right)\alpha_b^z\,du \nonumber\\
&- \rho^{zx}_{ab}\int_s^t \left(H^z_a(t)-H^z_a\right)\alpha_a^z \sigma^x_b,du\nonumber\\
&+ \rho^{zx}_{0a}\int_s^t \left(H^z_0(t)-H^z_0\right)\alpha_0^z\,\sigma^x_a\,du \nonumber\\
&- \rho^{zx}_{ba}\int_s^t \left(H^z_b(t)-H^z_b\right)\alpha_b^z\,\sigma^x_a\, du \nonumber\\
&+ \rho^{xx}_{ab}\int_s^t\sigma^x_a\,\sigma^x_b \,du \label{eq:covXX}\\
&\nonumber\\
\mathrm{Cov} [\Delta z_a, \Delta \ln x_b]) =& \rho^{zz}_{0a}\int_s^t \left(H^z_0(t)-H^z_0\right)  \alpha^z_0\,\alpha^z_a\,du \nonumber\\
&- \rho^{zz}_{ab}\int_s^t \alpha^z_a \left(H^z_b(t)-H^z_b\right) \alpha^z_b \,du \nonumber\\
&+\rho^{zx}_{ab}\int_s^t \alpha^z_a \, \sigma^x_b \,du. \label{eq:covZX}
\end{align}

The function definitions below split expectations into state-dependent and state-independent 
parts.

*/
    
/*! ir state expectation, part that is independent of current state */
Real ir_expectation_1(const CrossAssetModel *model, const Size i, const Time t0,
                      const Real dt);

/*! ir state expecation, part that is dependent on current state */
Real ir_expectation_2(const CrossAssetModel *model, const Size, const Real zi_0);

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
struct zeta {
    zeta(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel *x, const Real t) const {
        return x->irlgm1f(i_)->zeta(t);
    }
    const Size i_;
};

/*! FX zeta component */
struct sx {
    sx(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel *x, const Real t) const {
        return x->fxbs(i_)->sigma(t);
    }
    const Size i_;
};

/*! IR-IR correlation component */
struct rzz {
    rzz(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel *x, const Real) const {
        return x->ir_ir_correlation(i_, j_);
    }
    const Size i_, j_;
};

/*! IR-FX correlation component */
struct rzx {
    rzx(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel *x, const Real) const {
        return x->ir_fx_correlation(i_, j_);
    }
    const Size i_, j_;
};

/*! FX-FX correlation component */
struct rxx {
    rxx(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel *x, const Real) const {
        return x->fx_fx_correlation(i_, j_);
    }
    const Size i_, j_;
};


} // namespace CrossAssetAnalytics

} // namesapce QuantExt

#endif
