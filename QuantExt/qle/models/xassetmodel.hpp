/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file xassetmodel.hpp
    \brief cross asset model
*/

#ifndef quantext_xasset_model_hpp
#define quantext_xasset_model_hpp

#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/lgm.hpp>
#include <qle/models/model.hpp>
#include <qle/processes/xassetstateprocess.hpp>
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
                const Matrix &correlation,
                SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None);

    /*! IR-FX model based constructor */
    XAssetModel(const std::vector<boost::shared_ptr<Lgm> > &currencyModels,
                const std::vector<boost::shared_ptr<FxBsParametrization> >
                    &fxParametrizations,
                const Matrix &correlation,
                SalvagingAlgorithm::Type salvaging = SalvagingAlgorithm::None);

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
    const boost::shared_ptr<Lgm> lgm(const Size ccy) const;

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

    /*! correlation linking the different marginal models, note that
        the use of asset class pairs specific inspectors is
        recommended instead of the global matrix directly */
    const Matrix &correlation() const;

    /*! correlation between two ir components */
    Real ir_ir_correlation(const Size i, const Size j) const;
    /*! correlation between an ir and a fx component */
    Real ir_fx_correlation(const Size i, const Size j) const;
    /*! correlation between two fx compoents */
    Real fx_fx_correlation(const Size i, const Size j) const;

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

  private:
    /* init methods */

    void initialize();
    void initializeParametrizations();
    void initializeCorrelation();
    void initializeArguments();

    /* members */

    Size nIrLgm1f_, nFxBs_;
    Size totalNumberOfParameters_;
    std::vector<boost::shared_ptr<Parametrization> > p_;
    std::vector<boost::shared_ptr<Lgm> > lgm_;
    const Matrix rho_;
    SalvagingAlgorithm::Type salvaging_;
    mutable boost::shared_ptr<Integrator> integrator_;
    boost::shared_ptr<XAssetStateProcess> stateProcessExact_,
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

inline const boost::shared_ptr<Lgm> XAssetModel::lgm(const Size ccy) const {
    QL_REQUIRE(ccy < nIrLgm1f_, "irlgm1f index (" << ccy << ") must be in 0..."
                                                  << (nIrLgm1f_ - 1));
    return lgm_[ccy];
}

inline const boost::shared_ptr<IrLgm1fParametrization>
XAssetModel::irlgm1f(const Size ccy) const {
    return lgm(ccy)->parametrization();
}

inline Real XAssetModel::numeraire(const Size ccy, const Time t,
                                   const Real x) const {
    return lgm(ccy)->numeraire(t, x);
}

inline Real XAssetModel::discountBond(const Size ccy, const Time t,
                                      const Time T, const Real x) const {
    return lgm(ccy)->discountBond(t, T, x);
}

inline Real XAssetModel::reducedDiscountBond(const Size ccy, const Time t,
                                      const Time T, const Real x) const {
    return lgm(ccy)->reducedDiscountBond(t, T, x);
}

inline Real XAssetModel::discountBondOption(const Size ccy, Option::Type type,
                                     const Real K, const Time t, const Time S,
                                     const Time T) const {
    return lgm(ccy)->discountBondOption(type, K, t, S, T);
}

inline const boost::shared_ptr<FxBsParametrization>
XAssetModel::fxbs(const Size ccy) const {
    QL_REQUIRE(ccy < nFxBs_, "fxbs index (" << ccy << ") must be in 0..."
                                            << (nFxBs_ - 1));
    return boost::dynamic_pointer_cast<FxBsParametrization>(
        p_[nIrLgm1f_ + ccy]);
}

inline const Matrix &XAssetModel::correlation() const { return rho_; }

inline Real XAssetModel::ir_ir_correlation(const Size i, const Size j) const {
    QL_REQUIRE(i < nIrLgm1f_, "irlgm1f index (" << i << ") must be in 0..."
                                                << (nIrLgm1f_ - 1));
    QL_REQUIRE(j < nIrLgm1f_, "irlgm1f index (" << i << ") must be in 0..."
                                                << (nIrLgm1f_ - 1));
    return rho_[i][j];
}

inline Real XAssetModel::ir_fx_correlation(const Size i, const Size j) const {
    QL_REQUIRE(i < nIrLgm1f_, "irlgm1f index (" << i << ") must be in 0..."
                                                << (nIrLgm1f_ - 1));
    QL_REQUIRE(j < nFxBs_, "fxbs index (" << j << ") must be in 0..."
                                          << (nFxBs_ - 1));
    return rho_[i][nIrLgm1f_ + j];
}

inline Real XAssetModel::fx_fx_correlation(const Size i, const Size j) const {
    QL_REQUIRE(i < nFxBs_, "fxbs index (" << i << ") must be in 0..."
                                          << (nFxBs_ - 1));
    QL_REQUIRE(j < nFxBs_, "fxbs index (" << j << ") must be in 0..."
                                          << (nFxBs_ - 1));
    return rho_[nIrLgm1f_ + i][nIrLgm1f_ + j];
}

inline const boost::shared_ptr<Integrator> XAssetModel::integrator() const {
    return integrator_;
}

} // namespace QuantExt

#endif
