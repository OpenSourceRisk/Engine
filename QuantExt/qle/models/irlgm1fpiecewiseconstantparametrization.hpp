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
    \brief piecewise constant model parametrization
*/

#ifndef quantext_piecewiseconstant_irlgm1f_parametrizations_hpp
#define quantext_piecewiseconstant_irlgm1f_parametrizations_hpp

#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {

class IrLgm1fPiecewiseConstantParametrization
    : public IrLgm1fParametrization,
      private PiecewiseConstantHelper1,
      private PiecewiseConstantHelper2 {
  public:
    IrLgm1fPiecewiseConstantParametrization(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure,
        const Array &alphaTimes, const Array &alpha, const Array &kappaTimes,
        const Array &kappa);
    Handle<YieldTermStructure> termStructure() const;
    Real zeta(const Time t) const;
    Real H(const Time t) const;
    /*! inspectors */
    Real alpha(const Time t) const;
    Real kappa(Time t) const;
    Real Hprime(const Time t) const;
    Real Hprime2(const Time t) const;
    /*! additional methods */
    void update() const;

  private:
    const Handle<YieldTermStructure> termStructure_;
};

// inline

inline Handle<YieldTermStructure>
IrLgm1fPiecewiseConstantParametrization::termStructure() const {
    return termStructure_;
}

inline Real IrLgm1fPiecewiseConstantParametrization::zeta(const Time t) const {
    return PiecewiseConstantHelper1::int_y_sqr(t);
}

inline Real IrLgm1fPiecewiseConstantParametrization::H(const Time t) const {
    return PiecewiseConstantHelper2::int_exp_m_int_y(t);
}

inline Real IrLgm1fPiecewiseConstantParametrization::alpha(const Time t) const {
    return PiecewiseConstantHelper1::y(t);
}

inline Real IrLgm1fPiecewiseConstantParametrization::kappa(const Time t) const {
    return PiecewiseConstantHelper2::y(t);
}

inline Real
IrLgm1fPiecewiseConstantParametrization::Hprime(const Time t) const {
    return PiecewiseConstantHelper2::exp_m_int_y(t);
}

inline Real
IrLgm1fPiecewiseConstantParametrization::Hprime2(const Time t) const {
    return -PiecewiseConstantHelper2::exp_m_int_y(t) *
           PiecewiseConstantHelper2::y(t);
}

inline void IrLgm1fPiecewiseConstantParametrization::update() const {
    PiecewiseConstantHelper1::update();
    PiecewiseConstantHelper2::update();
}

} // namespace QuantExt

#endif
