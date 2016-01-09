/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file lgm.hpp
    \brief lgm model class
*/

#ifndef quantext_lgm_model_hpp
#define quantext_lgm_model_hpp

#include <qle/models/xassetmodel.hpp>

namespace QuantExt {

/*! LGM 1f interest rate model
    Basically the same remarks as for XAssetModel hold */

class Lgm : public Observer, public Observable {

  public:
    Lgm(const boost::shared_ptr<IrLgm1fParametrization> &parametrization);
    /*! the model shares the parametrization with the xasset model instance,
        so a calibration changes the component of the xasset model as well,
        however update() on the xAsset model must be called, because the
        caches of its state processes need to be flushed  */
    Lgm(const boost::shared_ptr<XAssetModel> &model, const Size ccy);

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

    /*! calibrate parameters for one ccy globally to a set
        of ir options */
    void calibrateGlobal(
        const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
        OptimizationMethod &method, const EndCriteria &endCriteria,
        const Constraint &constraint = Constraint(),
        const std::vector<Real> &weights = std::vector<Real>());

    /*! observer interface */
    void update();

  private:
    boost::shared_ptr<XAssetModel> x_;
    boost::shared_ptr<StochasticProcess1D> stateProcess_;
};

// inline

inline void Lgm::update() {
    x_->update();
    notifyObservers();
}

inline const boost::shared_ptr<StochasticProcess1D> Lgm::stateProcess() const {
    return stateProcess_;
}

inline const boost::shared_ptr<IrLgm1fParametrization>
Lgm::parametrization() const {
    return x_->irlgm1f(0);
}

inline Real Lgm::numeraire(const Time t, const Real x) const {
    return x_->numeraire(0, t, x);
}

inline Real Lgm::discountBond(const Time t, const Time T, const Real x) const {
    return x_->discountBond(0, t, T, x);
}

inline Real Lgm::reducedDiscountBond(const Time t, const Time T,
                                     const Real x) const {
    return x_->reducedDiscountBond(0, t, T, x);
}

inline Real Lgm::discountBondOption(Option::Type type, const Real K,
                                    const Time t, const Time S,
                                    const Time T) const {
    return x_->discountBondOption(0, type, K, t, S, T);
}

inline void Lgm::calibrateVolatilitiesIterative(
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    x_->calibrateIrLgm1fVolatilitiesIterative(0, helpers, method, endCriteria,
                                              constraint, weights);
}

inline void Lgm::calibrateReversionsIterative(
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    x_->calibrateIrLgm1fReversionsIterative(0, helpers, method, endCriteria,
                                            constraint, weights);
}

inline void Lgm::calibrateGlobal(
    const std::vector<boost::shared_ptr<CalibrationHelper> > &helpers,
    OptimizationMethod &method, const EndCriteria &endCriteria,
    const Constraint &constraint, const std::vector<Real> &weights) {
    x_->calibrateIrLgm1fGlobal(0, helpers, method, endCriteria, constraint,
                              weights);
}

} // namespace QuantExt

#endif
