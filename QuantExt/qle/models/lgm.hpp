/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file lgm.hpp
    \brief lgm model class
*/

#ifndef quantext_lgm_model_hpp
#define quantext_lgm_model_hpp

#include <qle/math/cumulativenormaldistribution.hpp>
#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>

#include <ql/stochasticprocess.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! LGM 1f interest rate model
    Basically the same remarks as for CrossAssetModel hold */

class LinearGaussMarkovModel : public LinkableCalibratedModel {

  public:
    LinearGaussMarkovModel(const boost::shared_ptr<IrLgm1fParametrization> &parametrization);

    const boost::shared_ptr<StochasticProcess1D> stateProcess() const;
    const boost::shared_ptr<IrLgm1fParametrization> parametrization() const;

    Real numeraire(const Time t, const Real x) const;
    Real discountBond(const Time t, const Time T, const Real x) const;
    Real reducedDiscountBond(const Time t, const Time T, const Real x) const;
    Real discountBondOption(Option::Type type, const Real K, const Time t,
                            const Time S, const Time T) const;

    /*! calibrate volatilities to a sequence of ir options with
        expiry times equal to step times in the parametrization */
    void calibrateVolatilitiesIterative(
        const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
        OptimizationMethod &method, const EndCriteria &endCriteria,
        const Constraint &constraint = Constraint(),
        const std::vector<Real> &weights = std::vector<Real>());

    /*! calibrate reversion to a sequence of ir options with
        maturities equal to step times in the parametrization */
    void calibrateReversionsIterative(
        const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
        OptimizationMethod &method, const EndCriteria &endCriteria,
        const Constraint &constraint = Constraint(),
        const std::vector<Real> &weights = std::vector<Real>());

    /*! observer and linked calibrated model interface */
    void update();
    void generateArguments();

    /*! calibration constraints, these can be used directly, or
        through the customized calibrate methods above */
    Disposable<std::vector<bool> > MoveVolatility(const Size i) {
        QL_REQUIRE(i < parametrization_->parameter(0)->size(),
                   "volatility index ("
                       << i << ") out of range 0..."
                       << parametrization_->parameter(0)->size() - 1);
        std::vector<bool> res(parametrization_->parameter(0)->size() +
                                  parametrization_->parameter(1)->size(),
                              true);
        res[i] = false;
        return res;
    }

    Disposable<std::vector<bool> > MoveReversion(const Size i) {
        QL_REQUIRE(i < parametrization_->parameter(1)->size(),
                   "reversion index ("
                       << i << ") out of range 0..."
                       << parametrization_->parameter(1)->size() - 1);
        std::vector<bool> res(parametrization_->parameter(0)->size() +
                                  parametrization_->parameter(1)->size(),
                              true);
        res[parametrization_->parameter(0)->size() + i] = false;
        return res;
    }

  private:
    boost::shared_ptr<IrLgm1fParametrization> parametrization_;
    boost::shared_ptr<StochasticProcess1D> stateProcess_;
};

// inline

inline void LinearGaussMarkovModel::update() {
    parametrization_->update();
    notifyObservers();
}

inline void LinearGaussMarkovModel::generateArguments() { update(); }

inline const boost::shared_ptr<StochasticProcess1D> LinearGaussMarkovModel::stateProcess() const {
    return stateProcess_;
}

inline const boost::shared_ptr<IrLgm1fParametrization>
LinearGaussMarkovModel::parametrization() const {
    return parametrization_;
}

inline Real LinearGaussMarkovModel::numeraire(const Time t, const Real x) const {
    QL_REQUIRE(t >= 0.0, "t (" << t << ") >= 0 required in LGM::numeraire");
    Real Ht = parametrization_->H(t);
    return std::exp(Ht * x + 0.5 * Ht * Ht * parametrization_->zeta(t)) /
           parametrization_->termStructure()->discount(t);
}

inline Real LinearGaussMarkovModel::discountBond(const Time t, const Time T, const Real x) const {
    QL_REQUIRE(T >= t && t >= 0.0,
               "T(" << T << ") >= t(" << t
                    << ") >= 0 required in LGM::discountBond");
    Real Ht = parametrization_->H(t);
    Real HT = parametrization_->H(T);
    return parametrization_->termStructure()->discount(T) /
           parametrization_->termStructure()->discount(t) *
           std::exp(-(HT - Ht) * x -
                    0.5 * (HT * HT - Ht * Ht) * parametrization_->zeta(t));
}

inline Real LinearGaussMarkovModel::reducedDiscountBond(const Time t, const Time T,
                                     const Real x) const {
    QL_REQUIRE(T >= t && t >= 0.0,
               "T(" << T << ") >= t(" << t
                    << ") >= 0 required in LGM::discountBond");
    Real HT = parametrization_->H(T);
    return parametrization_->termStructure()->discount(T) *
           std::exp(-HT * x - 0.5 * HT * HT * parametrization_->zeta(t));
}

inline Real LinearGaussMarkovModel::discountBondOption(Option::Type type, const Real K,
                                    const Time t, const Time S,
                                    const Time T) const {
    QL_REQUIRE(T > S && S >= t && t >= 0.0,
               "T(" << T << ") > S(" << S << ") >= t(" << t
                    << ") >= 0 required in LGM::discountBondOption");
    Real w = (type == Option::Call ? 1.0 : -1.0);
    Real pS = parametrization_->termStructure()->discount(S);
    Real pT = parametrization_->termStructure()->discount(T);
    // slight generalization of Lichters, Stamm, Gallagher 11.2.1
    // with t < S only resulting in a different time at which zeta
    // has to be taken
    Real sigma = sqrt(parametrization_->zeta(t)) *
                 (parametrization_->H(T) - parametrization_->H(S));
    Real dp = (std::log(pT / (K * pS)) / sigma + 0.5 * sigma);
    Real dm = dp - sigma;
    QuantExt::CumulativeNormalDistribution N;
    return w * (pT * N(w * dp) - pS * K * N(w * dm));
}

inline void LinearGaussMarkovModel::calibrateVolatilitiesIterative(
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights,
                  MoveVolatility(i));
    }
}

inline void LinearGaussMarkovModel::calibrateReversionsIterative(
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<boost::shared_ptr<CalibrationHelper> > h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights,
                  MoveReversion(i));
    }
}

} // namespace QuantExt

#endif
