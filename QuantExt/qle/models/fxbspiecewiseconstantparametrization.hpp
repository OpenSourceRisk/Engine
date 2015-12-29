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

/*! \file fxbspiecewiseconstantparametrization.hpp
    \brief piecewise constant model parametrization
*/

#ifndef quantext_piecewiseconstant_fxbs_parametrization_hpp
#define quantext_piecewiseconstant_fxbs_parametrization_hpp

#include <qle/models/fxbsparametrization.hpp>
#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {

class FxBsPiecewiseConstantParametrization : public FxBsParametrization,
                                             private PiecewiseConstantHelper1 {
  public:
    FxBsPiecewiseConstantParametrization(
        const Currency &currency,
        const Handle<YieldTermStructure> &domesticTermStructure,
        const Handle<YieldTermStructure> &foreignTermStructure,
        const Array &times, const Array &sigma);
    Real variance(const Time t) const;
    Real sigma(const Time t) const;
};

// inline

inline Real FxBsPiecewiseConstantParametrization::variance(const Time t) const {
    return PiecewiseConstantHelper1::int_y_sqr(t);
}

inline Real FxBsPiecewiseConstantParametrization::sigma(const Time t) const {
    return PiecewiseConstantHelper1::y(t);
}

} // namespace QuantExt

#endif
