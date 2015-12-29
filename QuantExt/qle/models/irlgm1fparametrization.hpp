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

/*! \file irlgm1fparametrization.hpp
    \brief Interest Rate Linear Gaussian Markov 1 factor parametrization
*/

#ifndef quantext_irlgm1f_parametrization_hpp
#define quantext_irlgm1f_parametrization_hpp

#include <qle/models/parametrization.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

class IrLgm1fParametrization : public Parametrization {
  public:
    IrLgm1fParametrization(const Currency &currency,
                           const Handle<YieldTermStructure> &termStructure);
    /*! interface */
    virtual Real zeta(const Time t) const = 0;
    virtual Real H(const Time t) const = 0;
    /*! inspectors */
    virtual Real alpha(const Time t) const;
    virtual Real kappa(const Time t) const;
    virtual Real Hprime(const Time t) const;
    virtual Real Hprime2(const Time t) const;
    virtual Real hullWhiteSigma(const Time t) const;
    const Handle<YieldTermStructure> termStructure() const;

  private:
    const Handle<YieldTermStructure> termStructure_;
};

// inline

inline Real IrLgm1fParametrization::alpha(const Time t) const {
    return std::sqrt((zeta(tr(t)) - zeta(tl(t))) / h_);
}

inline Real IrLgm1fParametrization::Hprime(const Time t) const {
    return (H(tr(t)) - H(tl(t))) / h_;
}

inline Real IrLgm1fParametrization::Hprime2(const Time t) const {
    return (H(tr2(t)) - 2.0 * H(tm2(t)) + H(tl2(t))) / (h2_ * h2_);
}

inline Real IrLgm1fParametrization::hullWhiteSigma(const Time t) const {
    return Hprime(t) * alpha(t);
}

inline Real IrLgm1fParametrization::kappa(const Time t) const {
    return -Hprime2(t) / Hprime(t);
}

inline const Handle<YieldTermStructure>
IrLgm1fParametrization::termStructure() const {
    return termStructure_;
}

} // namespace QuantExt

#endif
