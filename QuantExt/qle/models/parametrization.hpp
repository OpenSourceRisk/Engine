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

  protected:
    // this method should be called when input parameters
    // linked via references or pointers change in order
    // to ensure consistent results
    virtual void update() const;
    // step size for numerical differentiation
    const Real h_, h2_;
    // adjusted central difference scheme
    Time tr(const Time t) const;
    Time tl(const Time t) const;
    Time tr2(const Time t) const;
    Time tl2(const Time t) const;

  private:
    Currency currency_;
    Array emptyTimes_;
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
    return t > h2_ ? t + h2_ : h2_;
}

inline Time Parametrization::tl2(const Time t) const {
    return std::max(t - h2_, 0.0);
}

} // namespace QuantExt

#endif
