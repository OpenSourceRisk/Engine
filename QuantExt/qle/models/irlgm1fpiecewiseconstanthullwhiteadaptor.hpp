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

/*! \file irlgm1fconstanthullwhiteadaptor.hpp
    \brief adaptor to emulate piecewise constant Hull White parameters
*/

#ifndef quantext_piecewiseconstant_irlgm1f_hwadaptor_hpp
#define quantext_piecewiseconstant_irlgm1f_hwadaptor_hpp

#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {

class IrLgm1fPiecewiseConstantHullWhiteAdaptor
    : public IrLgm1fParametrization,
      private PiecewiseConstantHelper3,
      private PiecewiseConstantHelper2 {
  public:
    IrLgm1fPiecewiseConstantHullWhiteAdaptor(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure, const Array &times,
        const Array &sigma, const Array &kappa);
    Real zeta(const Time t) const;
    Real H(const Time t) const;
    /*! inspectors */
    Real alpha(const Time t) const;
    Real kappa(Time t) const;
    Real Hprime(const Time t) const;
    Real Hprime2(const Time t) const;
    Real hullWhiteSigma(Time t) const;
    const Array &parameterTimes(const Size) const;
    const Array &parameterValues(const Size) const;
    /*! additional methods */
    void update() const;
};

// inline

inline Real IrLgm1fPiecewiseConstantHullWhiteAdaptor::zeta(const Time t) const {
    return PiecewiseConstantHelper3::int_y1_sqr_exp_2_int_y2(t);
}

inline Real
IrLgm1fPiecewiseConstantHullWhiteAdaptor::alpha(const Time t) const {
    return hullWhiteSigma(t) / Hprime(t);
}

inline Real IrLgm1fPiecewiseConstantHullWhiteAdaptor::H(const Time t) const {
    return PiecewiseConstantHelper2::int_exp_m_int_y(t);
}

inline Real
IrLgm1fPiecewiseConstantHullWhiteAdaptor::kappa(const Time t) const {
    return PiecewiseConstantHelper2::y(t);
}

inline Real
IrLgm1fPiecewiseConstantHullWhiteAdaptor::Hprime(const Time t) const {
    return PiecewiseConstantHelper2::exp_m_int_y(t);
}

inline Real
IrLgm1fPiecewiseConstantHullWhiteAdaptor::Hprime2(const Time t) const {
    return -PiecewiseConstantHelper2::exp_m_int_y(t) *
           PiecewiseConstantHelper2::y(t);
}

inline Real
IrLgm1fPiecewiseConstantHullWhiteAdaptor::hullWhiteSigma(const Time t) const {
    return PiecewiseConstantHelper3::y1(t);
}

inline void IrLgm1fPiecewiseConstantHullWhiteAdaptor::update() const {
    PiecewiseConstantHelper3::update();
    PiecewiseConstantHelper2::update();
}

inline const Array &
IrLgm1fPiecewiseConstantHullWhiteAdaptor::parameterTimes(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return PiecewiseConstantHelper3::t_;
    else
        return PiecewiseConstantHelper2::t_;
}

const Array &
IrLgm1fPiecewiseConstantHullWhiteAdaptor::parameterValues(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return PiecewiseConstantHelper3::y1_;
    else
        return PiecewiseConstantHelper2::y_;
}

} // namespace QuantExt

#endif
