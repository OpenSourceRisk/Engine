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

/*! \file irlgm1fconstantparametrization.hpp
    \brief constant model parametrization
*/

#ifndef quantext_constant_irlgm1f_parametrizations_hpp
#define quantext_constant_irlgm1f_parametrizations_hpp

#include <qle/models/irlgm1fparametrization.hpp>

namespace QuantExt {

class IrLgm1fConstantParametrization : public IrLgm1fParametrization {
  public:
    IrLgm1fConstantParametrization(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure, const Real alpha,
        const Real kappa);
    Real zeta(const Time t) const;
    Real H(const Time t) const;
    /*! inspectors */
    Real alpha(const Time t) const;
    Real kappa(Time t) const;
    Real Hprime(const Time t) const;
    Real Hprime2(const Time t) const;
    Array &rawValues(const Size) const;

  protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size j, const Real y) const;

  private:
    mutable Array rawAlpha_, kappa_;
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
    return direct(0, rawAlpha_[0]) * direct(0, rawAlpha_[0]) * t;
}

inline Real IrLgm1fConstantParametrization::H(const Time t) const {
    if (std::fabs(kappa_[0]) < zeroKappaCutoff_) {
        return t;
    } else {
        return (1.0 - std::exp(-kappa_[0] * t)) / kappa_[0];
    }
}

inline Real IrLgm1fConstantParametrization::alpha(const Time t) const {
    return direct(0, rawAlpha_[0]);
}

inline Real IrLgm1fConstantParametrization::kappa(const Time t) const {
    return kappa_[0];
}

inline Real IrLgm1fConstantParametrization::Hprime(const Time t) const {
    return std::exp(-kappa_[0] * t);
}

inline Real IrLgm1fConstantParametrization::Hprime2(const Time t) const {
    return -kappa_[0] * std::exp(-kappa_[0] * t);
}

inline Array &IrLgm1fConstantParametrization::rawValues(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return rawAlpha_;
    else
        return kappa_;
}

} // namespace QuantExt

#endif
