/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file xassetmodel.hpp
    \brief cross asset model
*/

#ifndef quantext_xasset_model_hpp
#define quantext_xasset_model_hpp

#include <qle/math/cumulativenormaldistribution.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>
#include <qle/processes/xassetstateprocess.hpp>
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
    parametrizations are absolute and insensitive to shifts in the global
    evaluation date. The termstructures are required to be consistent with
    these times. The model does not observe anything, so it's update() method
    must be explicitly called to notify observers of changes in the
    constituting parametrizations, update these parametrizations and flushing
    the cache of the state process. The model ensures these updates during
    calibration though.
*/

class XAssetModel : public LinkableCalibratedModel {
  public:
    /*! Parametrizations must be given in the following order
        - IR (first parametrization defines the domestic currency)
        - FX (for all pairs domestic-ccy defined by the IR models)
        - INF (optionally, ccy must be a subset of the IR ccys)
        - CRD (optionally, ccy must be a subset of the IR ccys)
        - COM (optionally) ccy must be a subset of the IR ccys) */
    XAssetModel(const std::vector<boost::shared_ptr<Parametrization> >
                    &parametrizations,
                const Matrix &correlation, SalvagingAlgorithm::Type salvaging =
                                               SalvagingAlgorithm::None);

    /*! allow for time dependent correlation (2nd ctor) */

    /*! returns the state process with a given discretization */
    const boost::shared_ptr<StochasticProcess>
    stateProcess(XAssetStateProcess::discretization disc =
                     XAssetStateProcess::exact) const;
    /*! total dimension of model */
    Size dimension() const;
    /*! number of currencies including domestic */
    Size currencies() const;
    /*! total number of parameters that can be calibrated */
    Size totalNumberOfParameters() const;

    /*! observer and linked calibrated model interface */
    void update();
    void generateArguments();

    /*! LGM1F components, ccy=0 refers to the domestic currency */
    const boost::shared_ptr<IrLgm1fParametrization>
    irlgm1f(const Size ccy) const;
    Real numeraire(const Size ccy, const Time t, const Real x) const;
    Real discountBond(const Size ccy, const Time t, const Time T,
                      const Real x) const;
    Real reducedDiscountBond(const Size ccy, const Time t, const Time T,
                             const Real x) const;
    Real discountBondOption(const Size ccy, Option::Type type, const Real K,
                            const Time t, const Time S, const Time T) const;

    /*! FXBS components, ccy=0 referes to the first foreign currency,
        so it corresponds to ccy+1 if you want to get the corresponding
        irmgl1f component */
    const boost::shared_ptr<FxBsParametrization> fxbs(const Size ccy) const;

    /* ... add more components here ...*/

    /*! correlation */
    const Matrix &correlation() const;

    /*! expectations and covariances,
       notation follows Lichters, Stamm, Gallagher, 2015, i.e.
       z is the ir lgm state variable,
       x is the log spot fx */

    /*! analytic moments rely on numerical integration, which can
        be customized here */
    void setIntegrationPolicy(const boost::shared_ptr<Integrator> integrator,
                              const bool usePiecewiseIntegration = true) const;

    /*! moments, some expressions are splitted (their sum gives the result)
        in order to allow for efficient caching of results in other classes */
    Real ir_expectation_1(const Size i, const Time t0, const Real dt) const;
    Real ir_expectation_2(const Size i, const Real zi_0) const;
    Real fx_expectation_1(const Size i, const Time t0, const Real dt) const;
    Real fx_expectation_2(const Size i, const Time t0, const Real xi_0,
                          const Real zi_0, const Real z0_0,
                          const Real dt) const;
    Real ir_ir_covariance(const Size i, const Size j, const Time t0,
                          Time dt) const;
    Real ir_fx_covariance(const Size i, const Size j, const Time t0,
                          Time dt) const;
    Real fx_fx_covariance(const Size i, const Size j, const Time t0,
                          Time dt) const;

    Real integral(const Size hi, const Size hj, const Size alphai,
                  const Size alphaj, const Size sigmai, const Size sigmaj,
                  const Real a, const Real b) const;

    /* ... */

    /*! calibration procedures */

    /*! calibrate irlgm1f volatilities to a sequence of ir options with
        expiry times equal to step times in the parametrization */
    void calibrateIrLgm1fVolatilitiesIterative(
        const Size ccy,
        const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
        OptimizationMethod &method, const EndCriteria &endCriteria,
        const Constraint &constraint = Constraint(),
        const std::vector<Real> &weights = std::vector<Real>());

    /*! calibrate irlgm1f reversion to a sequence of ir options with
        maturities equal to step times in the parametrization */
    void calibrateIrLgm1fReversionsIterative(
        const Size ccy,
        const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
        OptimizationMethod &method, const EndCriteria &endCriteria,
        const Constraint &constraint = Constraint(),
        const std::vector<Real> &weights = std::vector<Real>());

    /*! calibrate irlgm1f parameters for one ccy globally to a set
        of ir options */
    void calibrateIrLgm1fGlobal(
        const Size ccy,
        const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
        OptimizationMethod &method, const EndCriteria &endCriteria,
        const Constraint &constraint = Constraint(),
        const std::vector<Real> &weights = std::vector<Real>());

    /*! calibrate fx volatilities to a sequence of fx options with
            expiry times equal to step times in the parametrization */
    void calibrateFxBsVolatilitiesIterative(
        const Size ccy,
        const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
        OptimizationMethod &method, const EndCriteria &endCriteria,
        const Constraint &constraint = Constraint(),
        const std::vector<Real> &weights = std::vector<Real>());

    /* ... add more calibration procedures here ... */

  private:
    /*! init methods */
    void initialize();
    void initializeParametrizations();
    void initializeCorrelation();
    void initializeArguments();

    /*! integral helper function */
    Real integral_helper(const Size hi, const Size hj, const Size alphai,
                         const Size alphaj, const Size sigmai,
                         const Size sigmaj, const Real t) const;

    /*! members */
    Size nIrLgm1f_, nFxBs_;
    Size totalNumberOfParameters_;
    const std::vector<boost::shared_ptr<Parametrization> > p_;
    const Matrix rho_;
    SalvagingAlgorithm::Type salvaging_;
    mutable boost::shared_ptr<Integrator> integrator_;
    boost::shared_ptr<XAssetStateProcess> stateProcessExact_,
        stateProcessEuler_;

    /*! calibration constraints */

    Disposable<std::vector<bool> > MoveIrLgm1fVolatility(const Size ccy,
                                                         const Size i) {
        QL_REQUIRE(i < irlgm1f(ccy)->parameter(0)->size(),
                   "irlgm1f volatility index ("
                       << i << ") for ccy " << ccy << " out of bounds 0..."
                       << irlgm1f(ccy)->parameter(0)->size() - 1);
        std::vector<bool> res(0);
        for (Size j = 0; j < nIrLgm1f_; ++j) {
            std::vector<bool> tmp1(irlgm1f(j)->parameter(0)->size(), true);
            if (ccy == j) {
                tmp1[i] = false;
            }
            std::vector<bool> tmp2(irlgm1f(j)->parameter(1)->size(), true);
            res.insert(res.end(), tmp1.begin(), tmp1.end());
            res.insert(res.end(), tmp2.begin(), tmp2.end());
        }
        for (Size j = 0; j < nFxBs_; ++j) {
            std::vector<bool> tmp(fxbs(j)->parameter(0)->size(), true);
            res.insert(res.end(), tmp.begin(), tmp.end());
        }
        return res;
    }

    Disposable<std::vector<bool> > MoveIrLgm1fReversion(const Size ccy,
                                                        const Size i) {
        QL_REQUIRE(i < irlgm1f(ccy)->parameter(1)->size(),
                   "irlgm1f reversion index ("
                       << i << ") for ccy " << ccy << " out of bounds 0..."
                       << irlgm1f(ccy)->parameter(1)->size() - 1);
        std::vector<bool> res(0);
        for (Size j = 0; j < nIrLgm1f_; ++j) {
            std::vector<bool> tmp1(irlgm1f(j)->parameter(0)->size(), true);
            std::vector<bool> tmp2(irlgm1f(j)->parameter(1)->size(), true);
            if (ccy == j) {
                tmp2[i] = false;
            }
            res.insert(res.end(), tmp1.begin(), tmp1.end());
            res.insert(res.end(), tmp2.begin(), tmp2.end());
        }
        for (Size j = 0; j < nFxBs_; ++j) {
            std::vector<bool> tmp(fxbs(j)->parameter(0)->size(), true);
            res.insert(res.end(), tmp.begin(), tmp.end());
        }
        return res;
    }

    Disposable<std::vector<bool> > MoveFxBsVolatility(const Size ccy,
                                                      const Size i) {
        QL_REQUIRE(i < fxbs(ccy)->parameter(0)->size(),
                   "fxbs volatility index ("
                       << i << ") for ccy " << ccy << " out of bounds 0..."
                       << fxbs(ccy)->parameter(0)->size() - 1);
        std::vector<bool> res(0);
        for (Size j = 0; j < nIrLgm1f_; ++j) {
            std::vector<bool> tmp1(irlgm1f(j)->parameter(0)->size(), true);
            std::vector<bool> tmp2(irlgm1f(j)->parameter(1)->size(), true);
            res.insert(res.end(), tmp1.begin(), tmp1.end());
            res.insert(res.end(), tmp2.begin(), tmp2.end());
        }
        for (Size j = 0; j < nFxBs_; ++j) {
            std::vector<bool> tmp(fxbs(j)->parameter(0)->size(), true);
            if (ccy == j) {
                tmp[i] = false;
            }
            res.insert(res.end(), tmp.begin(), tmp.end());
        }
        return res;
    }

    Disposable<std::vector<bool> > IrLgm1fGlobal(const Size ccy) {
        QL_REQUIRE(ccy < nIrLgm1f_, "irlgm1f ccy (" << ccy
                                                    << ") out of range 0..."
                                                    << (nIrLgm1f_ - 1));
        std::vector<bool> res(0);
        for (Size i = 0; i < nIrLgm1f_; ++i) {
            std::vector<bool> tmp1(irlgm1f(i)->parameter(0)->size(),
                                   ccy == i ? false : true);
            std::vector<bool> tmp2(irlgm1f(i)->parameter(1)->size(),
                                   ccy == i ? false : true);
            res.insert(res.end(), tmp1.begin(), tmp1.end());
            res.insert(res.end(), tmp2.begin(), tmp2.end());
        }
        for (Size i = 0; i < nFxBs_; ++i) {
            std::vector<bool> tmp(fxbs(i)->parameter(0)->size(), true);
            res.insert(res.end(), tmp.begin(), tmp.end());
        }
        return res;
    }
};

// inline

inline const boost::shared_ptr<StochasticProcess>
XAssetModel::stateProcess(XAssetStateProcess::discretization disc) const {
    return disc == XAssetStateProcess::exact ? stateProcessExact_
                                             : stateProcessEuler_;
}

inline Size XAssetModel::dimension() const {
    return nIrLgm1f_ * 1 + nFxBs_ * 1;
}

inline Size XAssetModel::currencies() const { return nIrLgm1f_; }

inline Size XAssetModel::totalNumberOfParameters() const {
    return totalNumberOfParameters_;
}

inline void XAssetModel::update() {
    for (Size i = 0; i < p_.size(); ++i) {
        p_[i]->update();
    }
    stateProcessExact_->flushCache();
    stateProcessEuler_->flushCache();
    notifyObservers();
}

inline void XAssetModel::generateArguments() { update(); }

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

inline const Matrix &XAssetModel::correlation() const { return rho_; }

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
    Size i1 = Null<Size>(), j1 = Null<Size>();
    Size i2 = Null<Size>(), j2 = Null<Size>();
    Real res = 1.0;
    if (hi != na) {
        res *= irlgm1f(hi)->H(t);
        i1 = hi;
    }
    if (hj != na) {
        res *= irlgm1f(hj)->H(t);
        j1 = hj;
    }
    if (alphai != na) {
        res *= irlgm1f(alphai)->alpha(t);
        i1 = alphai;
    }
    if (alphaj != na) {
        res *= irlgm1f(alphaj)->alpha(t);
        j1 = alphaj;
    }
    if (sigmai != na) {
        res *= fxbs(sigmai)->sigma(t);
        i2 = sigmai;
    }
    if (sigmaj != na) {
        res *= fxbs(sigmaj)->sigma(t);
        j2 = sigmaj;
    }
    Size i = i1 != Null<Size>() ? i1 : i2 + nIrLgm1f_;
    Size j = j1 != Null<Size>() ? j1 : j2 + nIrLgm1f_;
    res *= rho_[i][j];
    return res;
}

inline Real XAssetModel::ir_expectation_1(const Size i, const Time t0,
                                          const Real dt) const {
    const Size na = Null<Size>();
    Real res = 0.0;
    if (i > 0) {
        res += -integral(i, na, i, i, na, na, t0, t0 + dt) -
               integral(na, na, i, na, na, i - 1, t0, t0 + dt) +
               integral(0, na, 0, i, na, na, t0, t0 + dt);
    }
    return res;
}

inline Real XAssetModel::ir_expectation_2(const Size, const Real zi_0) const {
    return zi_0;
}

inline Real XAssetModel::fx_expectation_1(const Size i, const Time t0,
                                          const Real dt) const {
    const Size na = Null<Size>();
    Real res = std::log(irlgm1f(i + 1)->termStructure()->discount(t0 + dt) /
                        irlgm1f(i + 1)->termStructure()->discount(t0) *
                        irlgm1f(0)->termStructure()->discount(t0) /
                        irlgm1f(0)->termStructure()->discount(t0 + dt));
    res -= 0.5 * integral(na, na, na, na, i, i, t0, t0 + dt);
    res += 0.5 * (irlgm1f(0)->H(t0 + dt) * irlgm1f(0)->H(t0 + dt) *
                      irlgm1f(0)->zeta(t0 + dt) -
                  irlgm1f(0)->H(t0) * irlgm1f(0)->H(t0) * irlgm1f(0)->zeta(t0) -
                  integral(0, 0, 0, 0, na, na, t0, t0 + dt));
    res -= 0.5 * (irlgm1f(i + 1)->H(t0 + dt) * irlgm1f(i + 1)->H(t0 + dt) *
                      irlgm1f(i + 1)->zeta(t0 + dt) -
                  irlgm1f(i + 1)->H(t0) * irlgm1f(i + 1)->H(t0) *
                      irlgm1f(i + 1)->zeta(t0) -
                  integral(i + 1, i + 1, i + 1, i + 1, na, na, t0, t0 + dt));
    res += integral(0, na, 0, na, na, i, t0, t0 + dt);
    res -= irlgm1f(i + 1)->H(t0 + dt) *
           (-integral(i + 1, na, i + 1, i + 1, na, na, t0, t0 + dt) +
            integral(0, na, 0, i + 1, na, na, t0, t0 + dt) -
            integral(na, na, i + 1, na, na, i, t0, t0 + dt));
    res += -integral(i + 1, i + 1, i + 1, i + 1, na, na, t0, t0 + dt) +
           integral(0, i + 1, 0, i + 1, na, na, t0, t0 + dt) -
           integral(i + 1, na, i + 1, na, na, i, t0, t0 + dt);
    return res;
}

inline Real XAssetModel::fx_expectation_2(const Size i, const Time t0,
                                          const Real xi_0, const Real zi_0,
                                          const Real z0_0,
                                          const Real dt) const {
    Real res = xi_0 + (irlgm1f(0)->H(t0 + dt) - irlgm1f(0)->H(t0)) * z0_0 -
               (irlgm1f(i + 1)->H(t0 + dt) - irlgm1f(i + 1)->H(t0)) * zi_0;
    return res;
}

inline Real XAssetModel::ir_ir_covariance(const Size i, const Size j,
                                          const Time t0, Time dt) const {
    const Size na = Null<Size>();
    Real res = integral(na, na, i, j, na, na, t0, t0 + dt);
    return res;
}

inline Real XAssetModel::ir_fx_covariance(const Size i, const Size j,
                                          const Time t0, Time dt) const {
    const Size na = Null<Size>();
    Real res =
        irlgm1f(0)->H(t0 + dt) * integral(na, na, 0, i, na, na, t0, t0 + dt) -
        integral(0, na, 0, i, na, na, t0, t0 + dt) -
        irlgm1f(j + 1)->H(t0 + dt) *
            integral(na, na, j + 1, i, na, na, t0, t0 + dt) +
        integral(j + 1, na, j + 1, i, na, na, t0, t0 + dt) +
        integral(na, na, i, na, na, j, t0, t0 + dt);
    return res;
}

inline Real XAssetModel::fx_fx_covariance(const Size i, const Size j,
                                          const Time t0, Time dt) const {
    const Size na = Null<Size>();
    Real H0 = irlgm1f(0)->H(t0 + dt);
    Real Hi = irlgm1f(i + 1)->H(t0 + dt);
    Real Hj = irlgm1f(j + 1)->H(t0 + dt);
    Real res =
        // row 1
        H0 * H0 * integral(na, na, 0, 0, na, na, t0, t0 + dt) -
        2.0 * H0 * integral(0, na, 0, 0, na, na, t0, t0 + dt) +
        integral(0, 0, 0, 0, na, na, t0, t0 + dt) -
        // row 2
        H0 * Hj * integral(na, na, 0, j + 1, na, na, t0, t0 + dt) +
        Hj * integral(0, na, 0, j + 1, na, na, t0, t0 + dt) +
        H0 * integral(j + 1, na, j + 1, 0, na, na, t0, t0 + dt) -
        integral(0, j + 1, 0, j + 1, na, na, t0, t0 + dt) -
        // row 3
        H0 * Hi * integral(na, na, 0, i + 1, na, na, t0, t0 + dt) +
        Hi * integral(0, na, 0, i + 1, na, na, t0, t0 + dt) +
        H0 * integral(i + 1, na, i + 1, 0, na, na, t0, t0 + dt) -
        integral(0, i + 1, 0, i + 1, na, na, t0, t0 + dt) +
        // row 4
        H0 * integral(na, na, 0, na, na, j, t0, t0 + dt) -
        integral(0, na, 0, na, na, j, t0, t0 + dt) +
        // row 5
        H0 * integral(na, na, 0, na, na, i, t0, t0 + dt) -
        integral(0, na, 0, na, na, i, t0, t0 + dt) -
        // row 6
        Hi * integral(na, na, i + 1, na, na, j, t0, t0 + dt) +
        integral(i + 1, na, i + 1, na, na, j, t0, t0 + dt) -
        // row 7
        Hj * integral(na, na, j + 1, na, na, i, t0, t0 + dt) +
        integral(j + 1, na, j + 1, na, na, i, t0, t0 + dt) +
        // row 8
        Hi * Hj * integral(na, na, i + 1, j + 1, na, na, t0, t0 + dt) -
        Hj * integral(i + 1, na, i + 1, j + 1, na, na, t0, t0 + dt) -
        Hi * integral(j + 1, na, j + 1, i + 1, na, na, t0, t0 + dt) +
        integral(i + 1, j + 1, i + 1, j + 1, na, na, t0, t0 + dt) +
        // row 9
        integral(na, na, na, na, i, j, t0, t0 + dt);
    return res;
}

} // namespace QuantExt

#endif
