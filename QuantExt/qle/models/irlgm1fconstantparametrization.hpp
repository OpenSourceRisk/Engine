/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file irlgm1fconstantparametrization.hpp
    \brief constant model parametrization
*/

#ifndef quantext_constant_irlgm1f_parametrizations_hpp
#define quantext_constant_irlgm1f_parametrizations_hpp

#include <qle/models/irlgm1fparametrization.hpp>

namespace QuantExt {

class IrLgm1fConstantParametrization : public IrLgm1fParametrization {
  public:
    /*! note that if a non unit scaling is provided, then
        the parameterValues method returns the unscaled alpha,
        while all other method return scaled (and shifted) values */
    IrLgm1fConstantParametrization(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure, const Real alpha,
        const Real kappa, const Real shift = 0.0, const Real scaling = 1.0);
    Real zeta(const Time t) const;
    Real H(const Time t) const;
    Real alpha(const Time t) const;
    Real kappa(Time t) const;
    Real Hprime(const Time t) const;
    Real Hprime2(const Time t) const;
    const boost::shared_ptr<Parameter> parameter(const Size) const;

  protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size j, const Real y) const;

  private:
    const boost::shared_ptr<PseudoParameter> alpha_, kappa_;
    const Real shift_, scaling_;
    const Real zeroKappaCutoff_;
};

// inline

inline Real IrLgm1fConstantParametrization::direct(const Size i,
                                                   const Real x) const {
    return i == 0 ? x * x : x;
}

inline Real IrLgm1fConstantParametrization::inverse(const Size i,
                                                    const Real y) const {
    return i == 0 ? std::sqrt(y) : y;
}

inline Real IrLgm1fConstantParametrization::zeta(const Time t) const {
    return direct(0, alpha_->params()[0]) * direct(0, alpha_->params()[0]) * t /
           (scaling_ * scaling_);
}

inline Real IrLgm1fConstantParametrization::H(const Time t) const {
    if (std::fabs(kappa_->params()[0]) < zeroKappaCutoff_) {
        return scaling_ * t + shift_;
    } else {
        return scaling_ * (1.0 - std::exp(-kappa_->params()[0] * t)) /
                   kappa_->params()[0] +
               shift_;
    }
}

inline Real IrLgm1fConstantParametrization::alpha(const Time) const {
    return direct(0, alpha_->params()[0]) / scaling_;
}

inline Real IrLgm1fConstantParametrization::kappa(const Time) const {
    return kappa_->params()[0];
}

inline Real IrLgm1fConstantParametrization::Hprime(const Time t) const {
    return scaling_ * std::exp(-kappa_->params()[0] * t);
}

inline Real IrLgm1fConstantParametrization::Hprime2(const Time t) const {
    return -scaling_ * kappa_->params()[0] * std::exp(-kappa_->params()[0] * t);
}

inline const boost::shared_ptr<Parameter>
IrLgm1fConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return alpha_;
    else
        return kappa_;
}

} // namespace QuantExt

#endif
