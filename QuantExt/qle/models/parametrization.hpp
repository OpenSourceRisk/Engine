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

/*! \file parametrization.hpp
    \brief base class for model parametrizations
*/

#ifndef quantext_model_parametrization_hpp
#define quantext_model_parametrization_hpp

#include <ql/currency.hpp>
#include <ql/math/array.hpp>

using namespace QuantLib;

namespace QuantExt {

class Parametrization {
  public:
    Parametrization(const Currency &currency);

    /*! interface */
    virtual const Currency currency() const;
    virtual Size parameterSize(const Size) const;
    virtual const Array &parameterTimes(const Size) const;
    virtual Disposable<Array> parameterValues(const Size) const;

    /*! these are the parameter values on which the actual
      optimiazation is done - these might be identical to
      the parameter values themselves, or transformed in
      order to implement a constraint, the spirit being
      to avoid hard constraints */
    virtual Array &rawValues(const Size) const;

    // this method should be called when input parameters
    // linked via references or pointers change in order
    // to ensure consistent results
    virtual void update() const;

  protected:
    // step size for numerical differentiation
    const Real h_, h2_;
    // adjusted central difference scheme
    Time tr(const Time t) const;
    Time tl(const Time t) const;
    Time tr2(const Time t) const;
    Time tm2(const Time t) const;
    Time tl2(const Time t) const;
    // transformations between raw and real parameters
    virtual Real direct(const Size, const Real x) const;
    virtual Real inverse(const Size, const Real y) const;

  private:
    Currency currency_;
    const Array emptyTimes_;
    mutable Array emptyValues_;
};

// inline

inline void Parametrization::update() const {}

inline Time Parametrization::tr(const Time t) const {
    return t > 0.5 * h_ ? t + 0.5 * h_ : h_;
}

inline Time Parametrization::tl(const Time t) const {
    return std::max(t - 0.5 * h_, 0.0);
}

inline Time Parametrization::tr2(const Time t) const {
    return t > h2_ ? t + h2_ : 2.0 * h2_;
}

inline Time Parametrization::tm2(const Time t) const {
    return t > h2_ ? t : h2_;
}

inline Time Parametrization::tl2(const Time t) const {
    return std::max(t - h2_, 0.0);
}

inline Real Parametrization::direct(const Size, const Real x) const {
    return x;
}

inline Real Parametrization::inverse(const Size, const Real y) const {
    return y;
}

inline const Currency Parametrization::currency() const { return currency_; }

inline Size Parametrization::parameterSize(const Size) const { return 0; }

inline const Array &Parametrization::parameterTimes(const Size) const {
    return emptyTimes_;
}

inline Disposable<Array> Parametrization::parameterValues(const Size i) const {
    Array &tmp = rawValues(i);
    Array res(tmp.size());
    for (Size i = 0; i < res.size(); ++i) {
        res[i] = direct(i, tmp[i]);
    }
    return res;
}

inline Array &Parametrization::rawValues(const Size i) const { return emptyValues_; }

} // namespace QuantExt

#endif
