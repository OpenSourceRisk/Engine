/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file crossassetmodel.hpp
    \brief cross asset model
*/

#ifndef quantext_crossasset_model_hpp
#define quantext_crossasset_model_hpp

#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/lgm.hpp>
#include <qle/processes/crossassetstateprocess.hpp>

#include <ql/models/model.hpp>
#include <ql/math/matrix.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/integrals/integral.hpp>

#include <boost/bind.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! Cross asset model

    Reference:

    Lichters, Stamm, Gallagher: Modern Derivatives Pricing and Credit Exposure
    Analysis, Palgrave Macmillan, 2015

    The model is operated under the domestic LGM measure. There are two ways of
    calibrating the model:

    - provide an already calibrated parametrization for a component
      extracted from some external model
    - do the calibration within the CrossAssetModel using one
      of the calibration procedures

    The inter-parametrization correlation matrix specified here can not be
    calibrated currently, but is a fixed, external input.

    The model does not own a reference date, the times given in the
    parametrizations are absolute and insensitive to shifts in the global
    evaluation date. The termstructures are required to be consistent with
    these times, i.e. should all have the same reference date and day counter.
    The model does not observe anything, so its update() method must be
    explicitly called to notify observers of changes in the constituting
    parametrizations, update these parametrizations and flushing the cache
    of the state process. The model ensures these updates during
    calibration though.

    The cross asset model for \f$n\f$ currencies is specified by the following SDE

    \f{eqnarray*}{
    dz_0(t) &=& \alpha^z_0(t)\,dW^z_0(t) \\
    dz_i(t) &=& \gamma_i(t)\,dt + \alpha^z_i(t)\,dW^z_i(t), \qquad i = 1,\dots, n-1 \\
    dx_i(t) / x_i(t) &=& \mu^x_i(t)\, dt +\sigma_i^x(t)\,dW^x_i(t), \qquad i=1, \dots, n-1 \\
    dW^a_i\,dW^b_j &=& \rho^{ab}_{ij}\,dt, \qquad a, b \in \{z, x\} 
    \f}
    Factors \f$z_i\f$ drive the LGM interest rate processes (index \f$i=0\f$ denotes the domestic currency),
    and factors \f$x_i\f$ the foreign exchange rate processes. 

    The no-arbitrage drift terms are 
    \f{eqnarray*}
    \gamma_i &=& -H^z_i\,(\alpha^z_i)^2  + H^z_0\,\alpha^z_0\,\alpha^z_i\,\rho^{zz}_{0i} - \sigma_i^x\,\alpha^z_i\, \rho^{zx}_{ii}\\
    \mu^x_i &=& r_0-r_i +H^z_0\,\alpha^z_0\,\sigma^x_i\,\rho^{zx}_{0i}
    \f}
    where we have dropped time-dependencies to lighten notation.  

    The short rate \f$r_i(t)\f$ in currency \f$i\f$ is connected with the instantaneous forward curve \f$f_i(0,t)\f$ 
    and model parameters \f$H_i(t)\f$ and \f$\alpha_i(t)\f$ via
    \f{eqnarray*}{
    r_i(t) &=& f_i(0,t) + z_i(t)\,H'_i(t) + \zeta_i(t)\,H_i(t)\,H'_i(t), \quad \zeta_i(t) = \int_0^t \alpha_i^2(s)\,ds  \\ \\ 
    \f}

    Parameters \f$H_i(t)\f$ and \f$\alpha_i(t)\f$ (or alternatively \f$\zeta_i(t)\f$) are LGM model parameters 
    which determine, together with the stochastic factor \f$z_i(t)\f$, the evolution of numeraire and 
    zero bond prices in the LGM model:
    \f{eqnarray*}{
    N(t) &=& \frac{1}{P(0,t)}\exp\left\{H_t\, z_t + \frac{1}{2}H^2_t\,\zeta_t \right\} \\
    P(t,T,z_t) &=& \frac{P(0,T)}{P(0,t)}\:\exp\left\{ -(H_T-H_t)\,z_t - \frac{1}{2} \left(H^2_T-H^2_t\right)\,\zeta_t\right\}. 
    \f}

*/

namespace CrossAssetModelTypes {
enum AssetType { IR, FX };
}

using namespace CrossAssetModelTypes;

class CrossAssetModel : public LinkableCalibratedModel {
  public:
    /*! Parametrizations must be given in the following order
        - IR  (first parametrization defines the domestic currency)
        - FX  (for all pairs domestic-ccy defined by the IR models)
        If the correlation matrix is not given, it is initialized
        as the unit matrix (and can be customized after
        construction of the model).
    */
    CrossAssetModel(
        const std::vector<boost::shared_ptr<Parametrization> >
            &parametrizations,
        const Matrix &correlation = Matrix(),
        SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None);

    /*! IR-FX model based constructor */
    CrossAssetModel(
        const std::vector<boost::shared_ptr<LinearGaussMarkovModel> >
            &currencyModels,
        const std::vector<boost::shared_ptr<FxBsParametrization> >
            &fxParametrizations,
        const Matrix &correlation = Matrix(),
        SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None);

    /*! returns the state process with a given discretization */
    const boost::shared_ptr<StochasticProcess>
    stateProcess(CrossAssetStateProcess::discretization disc =
                     CrossAssetStateProcess::exact) const;

    /*! total dimension of model (sum of number of state variables) */
    Size dimension() const;

    /*! total number of Brownian motions (this is less or equal to dimension) */
    Size brownians() const;

    /*! total number of parameters that can be calibrated */
    Size totalNumberOfParameters() const;

    /*! number of components for an asset class */
    Size components(const AssetType t) const;

    /*! number of brownian motions for a component */
    Size brownians(const AssetType t, const Size i) const;

    /*! number of state variables for a component */
    Size stateVariables(const AssetType t, const Size i) const;

    /*! return index for currency (0 = domestic, 1 = first
      foreign currency and so on) */
    Size ccyIndex(const Currency &ccy) const;

    /*! observer and linked calibrated model interface */
    void update();
    void generateArguments();

    /*! LGM1F components, ccy=0 refers to the domestic currency */
    const boost::shared_ptr<LinearGaussMarkovModel> lgm(const Size ccy) const;

    const boost::shared_ptr<IrLgm1fParametrization>
    irlgm1f(const Size ccy) const;

    Real numeraire(const Size ccy, const Time t, const Real x,
                   Handle<YieldTermStructure> discountCurve =
                       Handle<YieldTermStructure>()) const;

    Real discountBond(const Size ccy, const Time t, const Time T, const Real x,
                      Handle<YieldTermStructure> discountCurve =
                          Handle<YieldTermStructure>()) const;

    Real reducedDiscountBond(const Size ccy, const Time t, const Time T,
                             const Real x,
                             Handle<YieldTermStructure> discountCurve =
                                 Handle<YieldTermStructure>()) const;

    Real discountBondOption(const Size ccy, Option::Type type, const Real K,
                            const Time t, const Time S, const Time T,
                            Handle<YieldTermStructure> discountCurve =
                                Handle<YieldTermStructure>()) const;

    /*! FXBS components, ccy=0 referes to the first foreign currency,
        so it corresponds to ccy+1 if you want to get the corresponding
        irmgl1f component */
    const boost::shared_ptr<FxBsParametrization> fxbs(const Size ccy) const;

    /* ... add more components here ...*/

    /*! correlation linking the different marginal models, note that
        the use of asset class pairs specific inspectors is
        recommended instead of the global matrix directly */
    const Matrix &correlation() const;

    /*! check if correlation matrix is valid */
    void checkCorrelationMatrix() const;

    /*! index of component in the parametrization vector */
    Size idx(const AssetType t, const Size i) const;

    /*! index of component in the correlation matrix, by offset */
    Size cIdx(const AssetType t, const Size i, const Size offset = 0) const;

    /*! index of component in the stochastic process array, by offset */
    Size pIdx(const AssetType t, const Size i, const Size offset = 0) const;

    /*! correlation between two components */
    const Real& correlation(const AssetType s, const Size i, const AssetType t,
                     const Size j, const Size iOffset = 0,
                     const Size jOffset = 0) const;
    /*! set correlation */
    void correlation(const AssetType s, const Size i, const AssetType t,
                     const Size j, const Real value, const Size iOffset = 0,
                     const Size jOffset = 0);

    /*! analytical moments require numerical integration,
      which can be customized here */
    void setIntegrationPolicy(const boost::shared_ptr<Integrator> integrator,
                              const bool usePiecewiseIntegration = true) const;
    const boost::shared_ptr<Integrator> integrator() const;

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

  protected:
    /* ctor to be used in extensions, initialize is not called */
    CrossAssetModel(const std::vector<boost::shared_ptr<Parametrization> >
                        &parametrizations,
                    const Matrix &correlation,
                    SalvagingAlgorithm::Type salvaging, const bool)
        : LinkableCalibratedModel(), p_(parametrizations), rho_(correlation),
          salvaging_(salvaging) {}

    /*! number of arguments for a component */
    Size arguments(const AssetType t, const Size i) const;

    /*! index of component in the arguments vector, by offset */
    Size aIdx(const AssetType t, const Size i, const Size offset = 0) const;

    /* init methods */
    virtual void initialize();
    virtual void initializeParametrizations();
    virtual void initializeCorrelation();
    virtual void initializeArguments();
    virtual void finalizeArguments();
    virtual void checkModelConsistency() const;
    virtual void initDefaultIntegrator();
    virtual void initStateProcess();

    /* members */

    Size nIrLgm1f_, nFxBs_;
    Size totalNumberOfParameters_;
    std::vector<boost::shared_ptr<Parametrization> > p_;
    std::vector<boost::shared_ptr<LinearGaussMarkovModel> > lgm_;
    Matrix rho_;
    SalvagingAlgorithm::Type salvaging_;
    mutable boost::shared_ptr<Integrator> integrator_;
    boost::shared_ptr<CrossAssetStateProcess> stateProcessExact_,
        stateProcessEuler_;

    /* calibration constraints */

    Disposable<std::vector<bool> > MoveFxBsVolatility(const Size ccy,
                                                      const Size i) {
        QL_REQUIRE(i < fxbs(ccy)->parameter(0)->size(),
                   "fxbs volatility index ("
                       << i << ") for ccy " << ccy << " out of bounds 0..."
                       << fxbs(ccy)->parameter(0)->size() - 1);
        std::vector<bool> res(0);
        for (Size j = 0; j < nIrLgm1f_; ++j) {
            std::vector<bool> tmp1(p_[idx(IR,j)]->parameter(0)->size(), true);
            std::vector<bool> tmp2(p_[idx(IR,j)]->parameter(1)->size(), true);
            res.insert(res.end(), tmp1.begin(), tmp1.end());
            res.insert(res.end(), tmp2.begin(), tmp2.end());
        }
        for (Size j = 0; j < nFxBs_; ++j) {
            std::vector<bool> tmp(p_[idx(FX,j)]->parameter(0)->size(),
                                  true);
            if (ccy == j) {
                tmp[i] = false;
            }
            res.insert(res.end(), tmp.begin(), tmp.end());
        }
        return res;
    }
};

// inline

inline const boost::shared_ptr<StochasticProcess> CrossAssetModel::stateProcess(
    CrossAssetStateProcess::discretization disc) const {
    return disc == CrossAssetStateProcess::exact ? stateProcessExact_
                                                 : stateProcessEuler_;
}

inline Size CrossAssetModel::dimension() const {
    return nIrLgm1f_ * 1 + nFxBs_ * 1;
}

inline Size CrossAssetModel::brownians() const {
    return nIrLgm1f_ * 1 + nFxBs_ * 1;
}

inline Size CrossAssetModel::totalNumberOfParameters() const {
    return totalNumberOfParameters_;
}

inline const boost::shared_ptr<LinearGaussMarkovModel>
CrossAssetModel::lgm(const Size ccy) const {
    return lgm_[idx(IR, ccy)];
}

inline const boost::shared_ptr<IrLgm1fParametrization>
CrossAssetModel::irlgm1f(const Size ccy) const {
    return lgm(ccy)->parametrization();
}

inline Real
CrossAssetModel::numeraire(const Size ccy, const Time t, const Real x,
                           Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->numeraire(t, x, discountCurve);
}

inline Real
CrossAssetModel::discountBond(const Size ccy, const Time t, const Time T,
                              const Real x,
                              Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->discountBond(t, T, x, discountCurve);
}

inline Real CrossAssetModel::reducedDiscountBond(
    const Size ccy, const Time t, const Time T, const Real x,
    Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->reducedDiscountBond(t, T, x, discountCurve);
}

inline Real CrossAssetModel::discountBondOption(
    const Size ccy, Option::Type type, const Real K, const Time t, const Time S,
    const Time T, Handle<YieldTermStructure> discountCurve) const {
    return lgm(ccy)->discountBondOption(type, K, t, S, T, discountCurve);
}

inline const boost::shared_ptr<FxBsParametrization>
CrossAssetModel::fxbs(const Size ccy) const {
    return boost::dynamic_pointer_cast<FxBsParametrization>(p_[idx(FX, ccy)]);
}

inline const Matrix &CrossAssetModel::correlation() const { return rho_; }

inline const boost::shared_ptr<Integrator> CrossAssetModel::integrator() const {
    return integrator_;
}

} // namespace QuantExt

#endif
