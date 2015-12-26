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

/*! \file piecewiseconstantparametrization.hpp
    \brief generic parametrization with piecewise constant base parameter
           providing the function and the integral over the squared function
*/

#ifndef quantext_piecewiseconstant_parametrization_hpp
#define quantext_piecewiseconstant_parametrization_hpp

#include <qle/models/parametrization.hpp>
#include <qle/math/piecewisefunction.hpp>

namespace QuantExt {

class PiecewiseConstantParametrization : Parametrization {
  protected:
    PiecewiseConstantParametrization(const Array &x, const Array &y);
    void update() const;
    Real y(const Time t) const;
    Real int_y2(const Time t) const;

  private:
    const Array &x_, &y_;
    mutable std::vector<Real> b_;
}

// inline

inline void
PiecewiseConstantParametrization::update() const {
    Real sum = 0.0;
    b_.resize(x_.size());
    for (Size i = 0; i < x_.size(); ++i) {
        sum += x_[i] * x_[i] * (times_[i] - (i == 0 ? 0.0 : times_[i - 1]));
        b_[i] = sum;
    }
}

inline Real PiecewiseConstantParametrization::y(const Time t) const {
    return QL_PIECEWISE_FUNCTION(x_, y_, t);
}

inline Real PiecewiseConstantParametrization::int_y(const Time t) const {
    if (t < 0.0)
        return 0.0;
    Size i = std::upper_bound(x_.begin(), x_.end(), t) - times_.begin();
    Real res = 0.0;
    if (i >= 1)
        res += b_[std::min(i - 1, b_.size() - 1)];
    Real a = x_[std::min(i, x_.size() - 1)];
    res += a * a * (t - (i == 0 ? 0.0 : times_[i - 1]));
    return res;
}

} // namespace QuantExt

#endif
