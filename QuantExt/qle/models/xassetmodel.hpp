/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file xassetmodel.hpp
    \brief cross asset model
*/

#ifndef quantext_xasset_model_hpp
#define quantext_xasset_model_hpp

#include <qle/math/cumulativenormaldistribution.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <ql/math/matrix.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/integrals/integral.hpp>
#include <ql/models/model.hpp>
#include <ql/stochasticprocess.hpp>

#include <boost/bind.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! Cross asset model

    Reference:

    Lichters, Stamm, Gallagher: Modern Derivatives Pricing and Credit Exposure
    Analysis, Palgrave Macmillan, 2015

    The model is operated under the domestic LGM measure. There are two ways of
    calibrating the model:

    - provide a calibrated parametrization for a component
      extracted from some external model
    - do the calibration within the XAssetModel using one
      of the calibration procedures

    The inter-parametrization correlation matrix specified here can not be
    calibrated currently, but is a fixed, external input.

    The model does not own a reference date, the times given in the
   parametrizations
    are absolute and insensitive to shifts in the global evaluation date. The
    termstructures are required to be consistent with these times. The model
   does not
    observe anything, so it's update() method must be explicitly called to
   notify
    observers of changes in the constituting parametrizations. The model ensures
    the necessary updates of parametrizations during calibration though.
*/

class XAssetModel : public CalibratedModel {
  public:
    /*! Parametrizations must be given in the following order
        - IR (first parametrization defines the domestic currency)
        - FX (for all pairs domestic-ccy defined by the IR models)
        - INF (optionally, ccy must be a subset of the IR ccys)
        - CRD (optionally, ccy must be a subset of the IR ccys)
        - COM (optionally) ccy must be a subset of the IR ccys) */
    XAssetModel(const std::vector<boost::shared_ptr<Parametrization> >
                    &parametrizations,
                const Matrix &correlation);

    /*! allow for time dependent correlation (2nd ctor) */

    /*! inspectors */
    boost::shared_ptr<StochasticProcess> stateProcess() const;

    /*! LGM1F components */
    const boost::shared_ptr<IrLgm1fParametrization>
    irlgm1f(const Size ccy) const;
    Real numeraire(const Size ccy, const Time t, const Real x) const;
    Real discountBond(const Size ccy, const Time t, const Time T,
                      const Real x) const;
    Real reducedDiscountBond(const Size ccy, const Time t, const Time T,
                             const Real x) const;
    Real discountBondOption(const Size ccy, Option::Type type, const Real K,
                            const Time t, const Time S, const Time T) const;

    /*! FXBS components */
    const boost::shared_ptr<FxBsParametrization> fxbs(const Size ccy) const;

    /*! other components */

    /*! expectations and covariances,
      notation follows Lichters, Stamm, Gallagher, 2015, i.e.
      z is the ir lgm state variable,
      x is the log spot fx */

    /*! analytic moments rely on numerical integration, which can
        be customized here */
    void setIntegrationPolicy(const boost::shared_ptr<Integrator> integrator,
                               const bool usePiecewiseIntegration = true) const;

    Real ir_expectation(const Size i, const Time t0, const Real zi_0,
                        const Real z0_0, const Real dt) const;
    Real fx_expectation(const Size i, const Time t0, const Real xi_0,
                        const Real z0_0, const Real dt) const;
    Real ir_ir_covariance(const Size i, const Size j, const Time t0,
                          const Real zi_0, const Real zj_0, const Real z0_0,
                          Time dt) const;
    Real ir_fx_covariance(const Size i, const Size j, const Time t0,
                          const Real zi_0, const Real xj_0, const Real z0_0,
                          Time dt) const;
    Real fx_fx_covariance(const Size i, const Size j, const Time t0,
                          const Real xi_0, const Real xj_0, const Real z0_0,
                          Time dt) const;

    /*! calibration procedures */

    /*! calibrate irlgm1f volatilities to a sequence of ir options with
        expiry times equal to step times in the parametrization */
    void calibrateIrVolatilitiesIterative();
    /*! calibrate irlgm1f reversion to a sequence of ir options with
        maturities equal to step times in the parametrization */
    void calibrateIrReversionsIterative();
    /*! calibrate irlgm1f parameters globally to a set of ir options */
    void calibrateIrGlobally();
    /*! calibrate fx volatilities to a sequence of fx options with
        expiry times equal to step times in the parametrization */
    void calibrateFxVolatilitiesIterative();

    /* ... */

  private:
    /*! init methods */
    void initialize();
    void initializeParametrizations();
    void initializeCorrelation();
    void initializeArguments();

    /*! integral helper functions */
    Real integral(const Size hi, const Size hj, const Size alphai,
                  const Size alphaj, const Size sigmai, const Size sigmaj,
                  const Real a, const Real b) const;
    Real integral_helper(const Size hi, const Size hj, const Size alphai,
                         const Size alphaj, const Size sigmai,
                         const Size sigmaj, const Real t) const;

    /*! members */
    Size nIrLgm1f_, nFxBs_;
    const std::vector<boost::shared_ptr<Parametrization> > p_;
    const Matrix rho_;
    mutable boost::shared_ptr<Integrator> integrator_;
};

// inline

inline const boost::shared_ptr<IrLgm1fParametrization>
XAssetModel::irlgm1f(const Size ccy) const {
    QL_REQUIRE(ccy < nIrLgm1f_, "irlgm1f index (" << ccy << ") must be in 0..."
                                                  << (nIrLgm1f_ - 1));
    return boost::dynamic_pointer_cast<IrLgm1fParametrization>(p_[ccy]);
}

inline const boost::shared_ptr<FxBsParametrization>
XAssetModel::fxbs(const Size ccy) const {
    QL_REQUIRE(ccy < nFxBs_, "fxbs index (" << ccy << ") must be in 0..."
                                            << (nFxBs_ - 1));
    return boost::dynamic_pointer_cast<FxBsParametrization>(
        p_[nIrLgm1f_ + ccy]);
}

inline Real XAssetModel::numeraire(const Size ccy, const Time t,
                                   const Real x) const {
    const boost::shared_ptr<IrLgm1fParametrization> p = irlgm1f(ccy);
    Real Ht = p->H(t);
    return std::exp(Ht * x + 0.5 * Ht * Ht * p->zeta(t)) /
           p->termStructure()->discount(t);
}

inline Real XAssetModel::discountBond(const Size ccy, const Time t,
                                      const Time T, const Real x) const {
    QL_REQUIRE(T >= t, "T(" << T << ") >= t(" << t
                            << ") required in irlgm1f discountBond");
    const boost::shared_ptr<IrLgm1fParametrization> p = irlgm1f(ccy);
    Real Ht = p->H(t);
    Real HT = p->H(T);
    return p->termStructure()->discount(T) / p->termStructure()->discount(t) *
           std::exp(-(HT - Ht) * x - 0.5 * (HT * HT - Ht * Ht) * p->zeta(t));
}

inline Real XAssetModel::reducedDiscountBond(const Size ccy, const Time t,
                                             const Time T, const Real x) const {
    QL_REQUIRE(T >= t, "T(" << T << ") >= t(" << t
                            << ") required in irlgm1f reducedDiscountBond");
    const boost::shared_ptr<IrLgm1fParametrization> p = irlgm1f(ccy);
    Real HT = p->H(T);
    return p->termStructure()->discount(T) *
           std::exp(-HT * x - 0.5 * HT * HT * p->zeta(t));
}

inline Real XAssetModel::discountBondOption(const Size ccy, Option::Type type,
                                            const Real K, const Time t,
                                            const Time S, const Time T) const {
    QL_REQUIRE(T > S && S >= t,
               "T(" << T << ") > S(" << S << ") >= t(" << t
                    << ") required in irlgm1f discountBondOption");
    const boost::shared_ptr<IrLgm1fParametrization> p = irlgm1f(ccy);
    Real w = (type == Option::Call ? 1.0 : -1.0);
    Real pS = p->termStructure()->discount(S);
    Real pT = p->termStructure()->discount(T);
    // slight generalization of Lichters, Stamm, Gallagher 11.2.1
    // with t < S only resulting in a different time at which zeta
    // has to be taken
    Real sigma = sqrt(p->zeta(t)) * (p->H(T) - p->H(S));
    Real dp = (std::log(pT / (K * pS)) / sigma + 0.5 * sigma);
    Real dm = dp - sigma;
    QuantExt::CumulativeNormalDistribution N;
    return w * (pT * N(w * dp) - pS * K * N(w * dm));
}

inline Real XAssetModel::integral(const Size hi, const Size hj,
                                  const Size alphai, const Size alphaj,
                                  const Size sigmai, const Size sigmaj,
                                  const Real a, const Real b) const {
    return integrator_->operator()(boost::bind(&XAssetModel::integral_helper,
                                               this, hi, hj, alphai, alphaj,
                                               sigmai, sigmaj, _1),
                                   a, b);
}

inline Real XAssetModel::integral_helper(const Size hi, const Size hj,
                                         const Size alphai, const Size alphaj,
                                         const Size sigmai, const Size sigmaj,
                                         const Real t) const {
    const Size na = Null<Size>();
    Real res = 1.0;
    if (hi != na)
        res *= irlgm1f(hi)->H(t);
    if (hj != na)
        res *= irlgm1f(hj)->H(t);
    if (alphai != na)
        res *= irlgm1f(alphai)->alpha(t);
    if (alphaj != na)
        res *= irlgm1f(alphaj)->alpha(t);
    if (sigmai != na)
        res *= fxbs(sigmai)->sigma(t);
    if (sigmaj != na)
        res *= fxbs(sigmai)->sigma(t);
    return res;
}

inline Real XAssetModel::ir_expectation(const Size i, const Time t0,
                                        const Real zi_0, const Real z0_0,
                                        const Real dt) const {
    const Size na = Null<Size>();
    Real res = 0.0;
    if (i > 0) {
        res += -integral(i, na, i, i, na, na, t0, t0 + dt) -
               integral(na, na, i, na, i - 1, na, t0, t0 + dt) +
               integral(0, na, 0, i, na, na, t0, t0 + dt);
    }
    res += zi_0;
    return res;
}

} // namespace QuantExt

#endif
