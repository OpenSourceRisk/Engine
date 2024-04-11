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

/*! \file lgm.hpp
    \brief lgm model class
    \ingroup models
*/

#ifndef quantext_lgm_model_hpp
#define quantext_lgm_model_hpp

#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/irmodel.hpp>
#include <qle/models/lgmcalibrationinfo.hpp>
#include <qle/models/linkablecalibratedmodel.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/integrals/integral.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/stochasticprocess.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Linear Gauss Morkov Model
/*! LGM 1f interest rate model
    Basically the same remarks as for CrossAssetModel hold
    \ingroup models
*/
class LinearGaussMarkovModel : public IrModel {
public:
    enum class Discretization { Euler, Exact };

    LinearGaussMarkovModel(const QuantLib::ext::shared_ptr<IrLgm1fParametrization>& parametrization,
                           const Measure measure = Measure::LGM, const Discretization = Discretization::Euler,
                           const bool evaluateBankAccount = true,
                           const QuantLib::ext::shared_ptr<Integrator>& integrator = QuantLib::ext::make_shared<SimpsonIntegral>(1.0E-8,
                                                                                                                 100));

    //! IrModel interface

    Measure measure() const override { return measure_; }
    const QuantLib::ext::shared_ptr<Parametrization> parametrizationBase() const override { return parametrization_; }
    Handle<YieldTermStructure> termStructure() const override { return parametrization_->termStructure(); }
    Size n() const override;
    Size m() const override;
    Size n_aux() const override;
    Size m_aux() const override;
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess() const override;

    QuantLib::Real discountBond(const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& x,
                                const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve =
                                    Handle<YieldTermStructure>()) const override;

    QuantLib::Real
    numeraire(const QuantLib::Time t, const QuantLib::Array& x,
              const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve = Handle<YieldTermStructure>(),
              const QuantLib::Array& aux = Array()) const override;

    QuantLib::Real shortRate(const QuantLib::Time t, const QuantLib::Array& x,
                             const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve =
                                 Handle<YieldTermStructure>()) const override;

    //! LGM specific methods

    const QuantLib::ext::shared_ptr<IrLgm1fParametrization> parametrization() const;

    Real numeraire(const Time t, const Real x,
                   const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    /*! Bank account measure numeraire B(t) as a function of LGM state variable x (with drift) and auxiliary state
     * variable y */
    Real bankAccountNumeraire(const Time t, const Real x, const Real y,
                              const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    Real discountBond(const Time t, const Time T, const Real x,
                      Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    Real reducedDiscountBond(const Time t, const Time T, const Real x,
                             const Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    Real discountBondOption(Option::Type type, const Real K, const Time t, const Time S, const Time T,
                            Handle<YieldTermStructure> discountCurve = Handle<YieldTermStructure>()) const;

    /*! calibrate volatilities to a sequence of ir options with
        expiry times equal to step times in the parametrization */
    void calibrateVolatilitiesIterative(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
                                        OptimizationMethod& method, const EndCriteria& endCriteria,
                                        const Constraint& constraint = Constraint(),
                                        const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate reversion to a sequence of ir options with
        maturities equal to step times in the parametrization */
    void calibrateReversionsIterative(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
                                      OptimizationMethod& method, const EndCriteria& endCriteria,
                                      const Constraint& constraint = Constraint(),
                                      const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate volatilities globally */
    void calibrateVolatilities(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
                               OptimizationMethod& method, const EndCriteria& endCriteria,
                               const Constraint& constraint = Constraint(),
                               const std::vector<Real>& weights = std::vector<Real>());

    /*! calibrate volatilities globally */
    void calibrateReversions(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
                             OptimizationMethod& method, const EndCriteria& endCriteria,
                             const Constraint& constraint = Constraint(),
                             const std::vector<Real>& weights = std::vector<Real>());

    /*! observer and linked calibrated model interface */
    void update() override;
    void generateArguments() override;

    /*! calibration constraints, these can be used directly, or
        through the customized calibrate methods above */
    std::vector<bool> MoveVolatility(const Size i) {
        QL_REQUIRE(i < parametrization_->parameter(0)->size(),
                   "volatility index (" << i << ") out of range 0..." << parametrization_->parameter(0)->size() - 1);
        std::vector<bool> res(parametrization_->parameter(0)->size() + parametrization_->parameter(1)->size(), true);
        res[i] = false;
        return res;
    }

    std::vector<bool> MoveReversion(const Size i) {
        QL_REQUIRE(i < parametrization_->parameter(1)->size(),
                   "reversion index (" << i << ") out of range 0..." << parametrization_->parameter(1)->size() - 1);
        std::vector<bool> res(parametrization_->parameter(0)->size() + parametrization_->parameter(1)->size(), true);
        res[parametrization_->parameter(0)->size() + i] = false;
        return res;
    }

    /*! set info on how the model was calibrated */
    void setCalibrationInfo(const LgmCalibrationInfo& calibrationInfo) { calibrationInfo_ = calibrationInfo; }
    /*! get info on how the model was calibrated */
    const LgmCalibrationInfo& getCalibrationInfo() const { return calibrationInfo_; }

private:
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> parametrization_;
    QuantLib::ext::shared_ptr<Integrator> integrator_;
    Measure measure_;
    Discretization discretization_;
    bool evaluateBankAccount_;
    QuantLib::ext::shared_ptr<StochasticProcess1D> stateProcess_;
    LgmCalibrationInfo calibrationInfo_;
};

typedef LinearGaussMarkovModel LGM;

// inline

inline void LinearGaussMarkovModel::update() {
    parametrization_->update();
    notifyObservers();
}

inline void LinearGaussMarkovModel::generateArguments() { update(); }

inline QuantLib::ext::shared_ptr<StochasticProcess> LinearGaussMarkovModel::stateProcess() const {
    QL_REQUIRE(measure_ == Measure::LGM, "LinearGaussMarkovModel::stateProcess() only supports measure = LGM");
    return stateProcess_;
}

inline QuantLib::Real
LinearGaussMarkovModel::discountBond(const QuantLib::Time t, const QuantLib::Time T, const QuantLib::Array& x,
                                     const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve) const {
    QL_REQUIRE(x.size() == n(), "LinearGaussMarkovModel::discountBond() requires input state of dimension " << n());
    return discountBond(t, T, x[0], discountCurve);
}

inline QuantLib::Real
LinearGaussMarkovModel::numeraire(const QuantLib::Time t, const QuantLib::Array& x,
                                  const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve,
                                  const QuantLib::Array& aux) const {
    QL_REQUIRE(x.size() == n(), "LinearGaussMarkovModel::numeraire() requires input state of dimension " << n());
    QL_REQUIRE(aux.size() == n_aux(),
               "LinearGaussMarkovModel::numeraire() requires aux input state of dimension " << n_aux());
    if (measure() == Measure::LGM) {
        return numeraire(t, x[0], discountCurve);
    } else {
        return bankAccountNumeraire(t, x[0], aux[0], discountCurve);
    }
}

inline const QuantLib::ext::shared_ptr<IrLgm1fParametrization> LinearGaussMarkovModel::parametrization() const {
    return parametrization_;
}

inline Real LinearGaussMarkovModel::numeraire(const Time t, const Real x,
                                              const Handle<YieldTermStructure> discountCurve) const {
    QL_REQUIRE(t >= 0.0, "t (" << t << ") >= 0 required in LGM::numeraire");
    Real Ht = parametrization_->H(t);
    return std::exp(Ht * x + 0.5 * Ht * Ht * parametrization_->zeta(t)) /
           (discountCurve.empty() ? parametrization_->termStructure()->discount(t) : discountCurve->discount(t));
}

inline Real LinearGaussMarkovModel::discountBond(const Time t, const Time T, const Real x,
                                                 const Handle<YieldTermStructure> discountCurve) const {
    if (QuantLib::close_enough(t, T))
        return 1.0;
    QL_REQUIRE(T >= t && t >= 0.0, "T(" << T << ") >= t(" << t << ") >= 0 required in LGM::discountBond");
    Real Ht = parametrization_->H(t);
    Real HT = parametrization_->H(T);
    return (discountCurve.empty()
                ? parametrization_->termStructure()->discount(T) / parametrization_->termStructure()->discount(t)
                : discountCurve->discount(T) / discountCurve->discount(t)) *
           std::exp(-(HT - Ht) * x - 0.5 * (HT * HT - Ht * Ht) * parametrization_->zeta(t));
}

inline QuantLib::Real
LinearGaussMarkovModel::shortRate(const QuantLib::Time t, const QuantLib::Array& x,
                                  const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve) const {
    QL_FAIL("LGM does not provide short rate.");
}

inline Real LinearGaussMarkovModel::reducedDiscountBond(const Time t, const Time T, const Real x,
                                                        const Handle<YieldTermStructure> discountCurve) const {
    if (QuantLib::close_enough(t, T))
        return 1.0 / numeraire(t, x, discountCurve);
    QL_REQUIRE(T >= t && t >= 0.0, "T(" << T << ") >= t(" << t << ") >= 0 required in LGM::reducedDiscountBond");
    Real HT = parametrization_->H(T);
    return (discountCurve.empty() ? parametrization_->termStructure()->discount(T) : discountCurve->discount(T)) *
           std::exp(-HT * x - 0.5 * HT * HT * parametrization_->zeta(t));
}

inline Real LinearGaussMarkovModel::discountBondOption(Option::Type type, const Real K, const Time t, const Time S,
                                                       const Time T,
                                                       const Handle<YieldTermStructure> discountCurve) const {
    QL_REQUIRE(T > S && S >= t && t >= 0.0,
               "T(" << T << ") > S(" << S << ") >= t(" << t << ") >= 0 required in LGM::discountBondOption");
    Real w = (type == Option::Call ? 1.0 : -1.0);
    Real pS = discountCurve.empty() ? parametrization_->termStructure()->discount(S) : discountCurve->discount(S);
    Real pT = discountCurve.empty() ? parametrization_->termStructure()->discount(T) : discountCurve->discount(T);
    // slight generalization of Lichters, Stamm, Gallagher 11.2.1
    // with t < S, SSRN: https://ssrn.com/abstract=2246054
    Real sigma = std::sqrt(parametrization_->zeta(t)) * (parametrization_->H(T) - parametrization_->H(S));
    Real dp = std::log(pT / (K * pS)) / sigma + 0.5 * sigma * sigma;
    Real dm = dp - sigma;
    CumulativeNormalDistribution N;
    return w * (pT * N(w * dp) - pS * K * N(w * dm));
}

inline void LinearGaussMarkovModel::calibrateVolatilitiesIterative(
    const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveVolatility(i));
    }
    update();
}

inline void LinearGaussMarkovModel::calibrateReversionsIterative(
    const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers, OptimizationMethod& method,
    const EndCriteria& endCriteria, const Constraint& constraint, const std::vector<Real>& weights) {
    for (Size i = 0; i < helpers.size(); ++i) {
        std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> h(1, helpers[i]);
        calibrate(h, method, endCriteria, constraint, weights, MoveReversion(i));
    }
    update();
}

inline void
LinearGaussMarkovModel::calibrateVolatilities(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
                                              OptimizationMethod& method, const EndCriteria& endCriteria,
                                              const Constraint& constraint, const std::vector<Real>& weights) {
    std::vector<bool> moveVols(parametrization_->parameter(0)->size() + parametrization_->parameter(1)->size(), true);
    for (Size i = 0; i < parametrization_->parameter(0)->size(); ++i)
        moveVols[i] = false;
    calibrate(helpers, method, endCriteria, constraint, weights, moveVols);
    update();
}

inline void
LinearGaussMarkovModel::calibrateReversions(const std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>>& helpers,
                                            OptimizationMethod& method, const EndCriteria& endCriteria,
                                            const Constraint& constraint, const std::vector<Real>& weights) {
    std::vector<bool> moveRevs(parametrization_->parameter(0)->size() + parametrization_->parameter(1)->size(), true);
    for (Size i = 0; i < parametrization_->parameter(1)->size(); ++i)
        moveRevs[parametrization_->parameter(0)->size() + i] = false;
    calibrate(helpers, method, endCriteria, constraint, weights, moveRevs);
    update();
}

} // namespace QuantExt

#endif
