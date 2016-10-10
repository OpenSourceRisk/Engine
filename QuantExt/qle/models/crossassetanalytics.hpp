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

/*! \file crossassetanalytics.hpp
    \brief analytics for the cross asset model
    \ingroup crossassetmodel
*/

#ifndef quantext_crossasset_analytics_hpp
#define quantext_crossasset_analytics_hpp

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/crossassetanalyticsbase.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace CrossAssetAnalytics {

/*! \addtogroup crossassetmodel
    @{
*/

/*! IR state expectation

  This function evaluates part of the expectation \f$ \mathbb{E}_{t_0}[z_i(t_0+dt)]\f$.

  Using the results above for factor moves \f$\Delta z\f$ over time interval \f$(s,t)\f$,
  we have

  \f{eqnarray}{
  \mathbb{E}_{t_0}[z_i(t_0+\Delta t)] &=& z_i(t_0) + \mathbb{E}_{t_0}[\Delta z_i],
  \qquad\mbox{with}\quad \Delta z_i = z_i(t_0+\Delta t) - z_i(t_0) \\
  &=& z_i(t_0) -\int_{t_0}^{t_0+\Delta t} H^z_i\,(\alpha^z_i)^2\,du + \rho^{zz}_{0i} \int_{t_0}^{t_0+\Delta t}
  H^z_0\,\alpha^z_0\,\alpha^z_i\,du
      - \epsilon_i  \rho^{zx}_{ii}\int_{t_0}^{t_0+\Delta t} \sigma_i^x\,\alpha^z_i\,du
  \f}

  This function covers the latter three integrals, the state-independent part.

*/
Real ir_expectation_1(const CrossAssetModel* model, const Size i, const Time t0, const Real dt);

/*! IR state expecation
  This function evaluates the state-dependent part of the expectation

  \f{eqnarray}{
  \mathbb{E}_{t_0}[z_i(t_0+\Delta t)]
  &=& z_i(t_0) -\int_{t_0}^{t_0+\Delta t} H^z_i\,(\alpha^z_i)^2\,du + \rho^{zz}_{0i} \int_{t_0}^{t_0+\Delta t}
  H^z_0\,\alpha^z_0\,\alpha^z_i\,du
      - \epsilon_i  \rho^{zx}_{ii}\int_{t_0}^{t_0+\Delta t} \sigma_i^x\,\alpha^z_i\,du
  \f}

  i.e. simply the first contribution \f$ z_i(t_0) \f$.

*/
Real ir_expectation_2(const CrossAssetModel* model, const Size i, const Real zi_0);

/*! FX state expectation

  This function evaluates part of the expectation \f$ \mathbb{E}_{t_0}[\ln x_i(t_0+dt)]\f$.

  Using the results above for factor moves \f$\Delta \ln x\f$ over time interval \f$(s,t)\f$,
  we have

    \f{eqnarray}{
  \mathbb{E}_{t_0}[\ln x_i(t_0+\Delta t)] &=& \ln x_i(t_0) +  \mathbb{E}_{t_0}[\Delta \ln x_i],
  \qquad\mbox{with}\quad \Delta \ln x_i = \ln x_i(t_0+\Delta t) - \ln x_i(t_0) \\
  &=& \ln x_i(t_0) + \left(H^z_0(t)-H^z_0(s)\right) z_0(s) -\left(H^z_i(t)-H^z_i(s)\right)z_i(s)\\
  &&+ \ln \left( \frac{P^n_0(0,s)}{P^n_0(0,t)} \frac{P^n_i(0,t)}{P^n_i(0,s)}\right) \\
  && - \frac12 \int_s^t (\sigma^x_i)^2\,du \\
  &&+\frac12 \left((H^z_0(t))^2 \zeta^z_0(t) -  (H^z_0(s))^2 \zeta^z_0(s)- \int_s^t (H^z_0)^2
  (\alpha^z_0)^2\,du\right)\\
  &&-\frac12 \left((H^z_i(t))^2 \zeta^z_i(t) -  (H^z_i(s))^2 \zeta^z_i(s)-\int_s^t (H^z_i)^2 (\alpha^z_i)^2\,du
  \right)\\
  && + \rho^{zx}_{0i} \int_s^t H^z_0\, \alpha^z_0\, \sigma^x_i\,du \\
  &&  - \int_s^t \left(H^z_i(t)-H^z_i\right)\gamma_i \,du, \qquad\mbox{with}\quad s = t_0, \quad t = t_0+\Delta t
  \f}

  where we rearranged terms so that the state-dependent terms are listed on the first line
  (containing \f$\ln x_i(t_0), z_i(t_0), z_0(t_0) \f$)
  and all following terms are state-independent (deterministic, just dependent on initial market data and model
  parameters).

  The last integral above contains \f$\gamma_i\f$ which is (see documentation of the CrossAssetModel)
  \f[
  \gamma_i = -H^z_i\,(\alpha^z_i)^2  + H^z_0\,\alpha^z_0\,\alpha^z_i\,\rho^{zz}_{0i} - \sigma_i^x\,\alpha^z_i\,
  \rho^{zx}_{ii}.
  \f]
  The very last last integral above is therefore broken up into six terms which we do not list
  here, but which show up in this function's imlementation.

  This function covers the latter state-independent part of the FX expectation.
*/
Real fx_expectation_1(const CrossAssetModel* model, const Size i, const Time t0, const Real dt);

/*! FX state expectation.

  This function covers the state-dependent part of the FX expectation.
*/
Real fx_expectation_2(const CrossAssetModel* model, const Size i, const Time t0, const Real xi_0, const Real zi_0,
                      const Real z0_0, const Real dt);

/*! IR-IR Covariance

  This function evaluates the covariance term

      \f{eqnarray*}{
      \mathrm{Cov}[\Delta z_a, \Delta z_b] &=& \rho^{zz}_{ab} \int_s^t \alpha^z_a\,\alpha^z_b\,du
      \f}

      on the time interval from \f$ s= t_0\f$ to \f$ t=t_0+\Delta t\f$.
*/
Real ir_ir_covariance(const CrossAssetModel* model, const Size i, const Size j, const Time t0, const Time dt);

/*! IR-FX Covariance

  This function evaluates the covariance term

  \f{eqnarray}{
      \mathrm{Cov} [\Delta z_a, \Delta \ln x_b]) &=& \rho^{zz}_{0a}\int_s^t \left(H^z_0(t)-H^z_0\right)
  \alpha^z_0\,\alpha^z_a\,du \nonumber\\
      &&- \rho^{zz}_{ab}\int_s^t \alpha^z_a \left(H^z_b(t)-H^z_b\right) \alpha^z_b \,du \nonumber\\
      &&+\rho^{zx}_{ab}\int_s^t \alpha^z_a \, \sigma^x_b \,du.
      \f}

      on the time interval from \f$ s= t_0\f$ to \f$ t=t_0+\Delta t\f$.

*/
Real ir_fx_covariance(const CrossAssetModel* model, const Size i, const Size j, const Time t0, const Time dt);

/*! FX-FX covariance

  This function evaluates the covariance term

  \f{eqnarray}{
      \mathrm{Cov}[\Delta \ln x_a, \Delta \ln x_b] &=&
      \int_s^t \left(H^z_0(t)-H^z_0\right)^2 (\alpha_0^z)^2\,du \nonumber\\
      && -\rho^{zz}_{0a} \int_s^t \left(H^z_a(t)-H^z_a\right) \alpha_a^z\left(H^z_0(t)-H^z_0\right) \alpha_0^z\,du
  \nonumber\\
      &&- \rho^{zz}_{0b}\int_s^t \left(H^z_0(t)-H^z_0\right)\alpha_0^z \left(H^z_b(t)-H^z_b\right)\alpha_b^z\,du
  \nonumber\\
      &&+ \rho^{zx}_{0b}\int_s^t \left(H^z_0(t)-H^z_0\right)\alpha_0^z \sigma^x_b\,du \nonumber\\
      &&+ \rho^{zx}_{0a}\int_s^t \left(H^z_0(t)-H^z_0\right)\alpha_0^z\,\sigma^x_a\,du \nonumber\\
      &&- \rho^{zx}_{ab}\int_s^t \left(H^z_a(t)-H^z_a\right)\alpha_a^z \sigma^x_b,du\nonumber\\
      &&- \rho^{zx}_{ba}\int_s^t \left(H^z_b(t)-H^z_b\right)\alpha_b^z\,\sigma^x_a\, du \nonumber\\
      &&+ \rho^{zz}_{ab}\int_s^t \left(H^z_a(t)-H^z_a\right)\alpha_a^z \left(H^z_b(t)-H^z_b\right)\alpha_b^z\,du
  \nonumber\\
      &&+ \rho^{xx}_{ab}\int_s^t\sigma^x_a\,\sigma^x_b \,du
      \f}

      on the time interval from \f$ s= t_0\f$ to \f$ t=t_0+\Delta t\f$.

      The implementation of this FX-FX covariance in this function further decomposes all terms
      in order to separate simple and more complex integrations and to allow for tailored
      efficient numerical integration schemes. Line by line, the covariance above can be written:

      \f{eqnarray}{
      \mathrm{Cov}[\Delta \ln x_a, \Delta \ln x_b]
      &=&
      (H^z_0)^2(t)\int_s^t (\alpha_0^z)^2\,du
      - 2 \,H^z_0(t)\int_s^t H^z_0 (\alpha_0^z)^2\,du
      + \int_s^t (H^z_0\,\alpha_0^z)^2\,du \\
      &&
      - \rho^{zz}_{0a} H^z_0(t) \,H^z_a(t)\int_s^t \alpha_0^z\,\alpha_a^z\,du
      + \rho^{zz}_{0a} H^z_a(t) \int_s^t H^z_0\,\alpha_0^z\,\alpha_a^z\,du
      + \rho^{zz}_{0a} H^z_0(t) \int_s^t H^z_a\,\alpha_0^z\,\alpha_a^z\,du
      - \rho^{zz}_{0a}\int_s^t H^z_0\,H^z_a\,\alpha_0^z\,\alpha_a^z\,du \\
      &&
      - \rho^{zz}_{0b} H^z_0(t) \,H^z_b(t)\int_s^t \alpha_0^z\,\alpha_b^z\,du
      + \rho^{zz}_{0b} H^z_b(t) \int_s^t H^z_0\,\alpha_0^z\,\alpha_b^z\,du
      + \rho^{zz}_{0b} H^z_0(t) \int_s^t H^z_b\,\alpha_0^z\,\alpha_b^z\,du
      - \rho^{zz}_{0b}\int_s^t H^z_0\,H^z_b\,\alpha_0^z\,\alpha_b^z\,du \\
      &&
      + \rho^{zx}_{0b} H^z_0(t) \int_s^t \alpha_0^z \,\sigma^x_b\,du
      - \rho^{zx}_{0b} \int_s^t  H^z_0 \,\alpha_0^z \,\sigma^x_b\,du \\
      &&
      + \rho^{zx}_{0a} H^z_0(t) \int_s^t \alpha_0^z \,\sigma^x_a\,du
      - \rho^{zx}_{0a} \int_s^t  H^z_0 \,\alpha_0^z \,\sigma^x_a\,du \\
      &&
      - \rho^{zx}_{ab} H^z_a(t) \int_s^t \alpha_a^z \sigma^x_b,du
      + \rho^{zx}_{ab} \int_s^t H^z_a\,\alpha_a^z \sigma^x_b,du \\
      &&
      - \rho^{zx}_{ba} H^z_b(t) \int_s^t \alpha_b^z \sigma^x_a,du
      + \rho^{zx}_{ba} \int_s^t H^z_b\,\alpha_b^z \sigma^x_a,du \\
      &&
      + \rho^{zz}_{ab} H^z_a(t) H^z_b(t) \int_s^t \alpha_a^z \alpha_b^z \,du
      - \rho^{zz}_{ab} H^z_b(t) \int_s^t H^z_a \alpha_a^z \alpha_b^z \,du
      - \rho^{zz}_{ab} H^z_a(t) \int_s^t H^z_b \alpha_a^z \alpha_b^z \,du
      + \rho^{zz}_{ab} \int_s^t H^z_a H^z_b \alpha_a^z \alpha_b^z \,du \\
      &&
      + \rho^{xx}_{ab} \int_s^t\sigma^x_a\,\sigma^x_b \,du
      \f}

      When comparing with the code, also note that the integral on line one can be
      simplified to
      \f[
      \int_s^t (\alpha_0^z)^2\,du = \zeta_0(t) - \zeta_0(s)
      \f]

*/

Real fx_fx_covariance(const CrossAssetModel* model, const Size i, const Size j, const Time t0, const Time dt);

/*! IR H component */
struct Hz {
    Hz(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel* x, const Real t) const { return x->irlgm1f(i_)->H(t); }
    const Size i_;
};

/*! IR alpha component */
struct az {
    az(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel* x, const Real t) const { return x->irlgm1f(i_)->alpha(t); }
    const Size i_;
};

/*! IR zeta component */
struct zetaz {
    zetaz(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel* x, const Real t) const { return x->irlgm1f(i_)->zeta(t); }
    const Size i_;
};

/*! FX sigma component */
struct sx {
    sx(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel* x, const Real t) const { return x->fxbs(i_)->sigma(t); }
    const Size i_;
};

/*! FX variance component */
struct vx {
    vx(const Size i) : i_(i) {}
    Real eval(const CrossAssetModel* x, const Real t) const { return x->fxbs(i_)->variance(t); }
    const Size i_;
};

/*! IR-IR correlation component */
struct rzz {
    rzz(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel* x, const Real) const { return x->correlation(IR, i_, IR, j_, 0, 0); }
    const Size i_, j_;
};

/*! IR-FX correlation component */
struct rzx {
    rzx(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel* x, const Real) const { return x->correlation(IR, i_, FX, j_, 0, 0); }
    const Size i_, j_;
};

/*! FX-FX correlation component */
struct rxx {
    rxx(const Size i, const Size j) : i_(i), j_(j) {}
    Real eval(const CrossAssetModel* x, const Real) const { return x->correlation(FX, i_, FX, j_, 0, 0); }
    const Size i_, j_;
};

/*! @} */

} // namespace CrossAssetAnalytics

} // namesapce QuantExt

#endif
