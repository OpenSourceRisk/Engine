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

#ifndef quantext_piecewiseconstant_irlgm1f_parametrization_hpp
#define quantext_piecewiseconstant_irlgm1f_parametrization_hpp

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
    Real zeta(const Time t) const;
    Real H(const Time t) const;
    Real alpha(const Time t) const;
    Real kappa(Time t) const;
    Real Hprime(const Time t) const;
    Real Hprime2(const Time t) const;
    const Array &parameterTimes(const Size) const;
    const boost::shared_ptr<Parameter> parameter(const Size) const;
    void update() const;

  protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size j, const Real y) const;
};

// inline

inline Real
IrLgm1fPiecewiseConstantParametrization::direct(const Size i,
                                                const Real x) const {
    return i == 0 ? PiecewiseConstantHelper1::direct(x)
                  : PiecewiseConstantHelper2::direct(x);
}

inline Real
IrLgm1fPiecewiseConstantParametrization::inverse(const Size i,
                                                 const Real y) const {
    return i == 0 ? PiecewiseConstantHelper1::inverse(y)
                  : PiecewiseConstantHelper2::inverse(y);
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
    return -PiecewiseConstantHelper2::exp_m_int_y(t) * kappa(t);
}

inline void IrLgm1fPiecewiseConstantParametrization::update() const {
    PiecewiseConstantHelper1::update();
    PiecewiseConstantHelper2::update();
}

inline const Array &
IrLgm1fPiecewiseConstantParametrization::parameterTimes(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return PiecewiseConstantHelper1::t_;
    else
        return PiecewiseConstantHelper2::t_;
}

inline const boost::shared_ptr<Parameter>
IrLgm1fPiecewiseConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i < 2, "parameter " << i << " does not exist, only have 0..1");
    if (i == 0)
        return PiecewiseConstantHelper1::y_;
    else
        return PiecewiseConstantHelper2::y_;
}

} // namespace QuantExt

#endif
