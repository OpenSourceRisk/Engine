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

/*! \file irlgm1fconstantparametrization.hpp
    \brief constant model parametrization
    \ingroup models
*/

#ifndef quantext_constant_irlgm1f_parametrizations_hpp
#define quantext_constant_irlgm1f_parametrizations_hpp

#include <qle/models/irlgm1fparametrization.hpp>

namespace QuantExt {

//! LGM 1F Constant Parametrization
/*! \ingroup models
 */
template <class TS> class Lgm1fConstantParametrization : public Lgm1fParametrization<TS> {
public:
    Lgm1fConstantParametrization(const Currency& currency, const Handle<TS>& termStructure, const Real alpha,
                                 const Real kappa, const std::string& name = std::string());
    Real zeta(const Time t) const;
    Real H(const Time t) const;
    Real alpha(const Time t) const;
    Real kappa(const Time t) const;
    Real Hprime(const Time t) const;
    Real Hprime2(const Time t) const;
    const boost::shared_ptr<Parameter> parameter(const Size) const;

protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size j, const Real y) const;

private:
    const boost::shared_ptr<PseudoParameter> alpha_, kappa_;
    const Real zeroKappaCutoff_;
};

// implementation

template <class TS>
Lgm1fConstantParametrization<TS>::Lgm1fConstantParametrization(const Currency& currency,
                                                               const Handle<TS>& termStructure, const Real alpha,
                                                               const Real kappa, const std::string& name)
    : Lgm1fParametrization<TS>(currency, termStructure, name), alpha_(boost::make_shared<PseudoParameter>(1)),
      kappa_(boost::make_shared<PseudoParameter>(1)), zeroKappaCutoff_(1.0E-6) {
    alpha_->setParam(0, inverse(0, alpha));
    kappa_->setParam(0, inverse(1, kappa));
}

// inline

template <class TS> inline Real Lgm1fConstantParametrization<TS>::direct(const Size i, const Real x) const {
    return i == 0 ? x * x : x;
}

template <class TS> inline Real Lgm1fConstantParametrization<TS>::inverse(const Size i, const Real y) const {
    return i == 0 ? std::sqrt(y) : y;
}

template <class TS> inline Real Lgm1fConstantParametrization<TS>::zeta(const Time t) const {
    return direct(0, alpha_->params()[0]) * direct(0, alpha_->params()[0]) * t / (this->scaling_ * this->scaling_);
}

template <class TS> inline Real Lgm1fConstantParametrization<TS>::H(const Time t) const {
    if (std::fabs(kappa_->params()[0]) < zeroKappaCutoff_) {
        return this->scaling_ * t + this->shift_;
    } else {
        return this->scaling_ * (1.0 - std::exp(-kappa_->params()[0] * t)) / kappa_->params()[0] + this->shift_;
    }
}

template <class TS> inline Real Lgm1fConstantParametrization<TS>::alpha(const Time) const {
    return direct(0, alpha_->params()[0]) / this->scaling_;
}

template <class TS> inline Real Lgm1fConstantParametrization<TS>::kappa(const Time) const {
    return kappa_->params()[0];
}

template <class TS> inline Real Lgm1fConstantParametrization<TS>::Hprime(const Time t) const {
    return this->scaling_ * std::exp(-kappa_->params()[0] * t);
}

template <class TS> inline Real Lgm1fConstantParametrization<TS>::Hprime2(const Time t) const {
    return -this->scaling_ * kappa_->params()[0] * std::exp(-kappa_->params()[0] * t);
}

template <class TS>
inline const boost::shared_ptr<Parameter> Lgm1fConstantParametrization<TS>::parameter(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return alpha_;
    else
        return kappa_;
}

// typedef

typedef Lgm1fConstantParametrization<YieldTermStructure> IrLgm1fConstantParametrization;

} // namespace QuantExt

#endif
