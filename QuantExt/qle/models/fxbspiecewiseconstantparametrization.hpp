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
        const Handle<YieldTermStructure> &foreignTermStructure,
        const Handle<Quote> &fxSpotToday, const Array &times,
        const Array &sigma);
    /*! inspectors */
    Real variance(const Time t) const;
    Real sigma(const Time t) const;
    const Array &parameterTimes(const Size) const;
    const boost::shared_ptr<Parameter> parameter(const Size) const;
    /*! additional methods */
    void update() const;

  protected:
    Real direct(const Size i, const Real x) const;
    Real inverse(const Size i, const Real y) const;
};

// inline

inline Real FxBsPiecewiseConstantParametrization::direct(const Size,
                                                         const Real x) const {
    return PiecewiseConstantHelper1::direct(x);
}

inline Real FxBsPiecewiseConstantParametrization::inverse(const Size,
                                                          const Real y) const {
    return PiecewiseConstantHelper1::inverse(y);
}

inline Real FxBsPiecewiseConstantParametrization::variance(const Time t) const {
    return PiecewiseConstantHelper1::int_y_sqr(t);
}

inline Real FxBsPiecewiseConstantParametrization::sigma(const Time t) const {
    return PiecewiseConstantHelper1::y(t);
}

inline const Array &
FxBsPiecewiseConstantParametrization::parameterTimes(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return PiecewiseConstantHelper1::t_;
}

inline const boost::shared_ptr<Parameter>
FxBsPiecewiseConstantParametrization::parameter(const Size i) const {
    QL_REQUIRE(i == 0, "parameter " << i << " does not exist, only have 0");
    return PiecewiseConstantHelper1::y_;
}

inline void FxBsPiecewiseConstantParametrization::update() const {
    PiecewiseConstantHelper1::update();
}

} // namespace QuantExt

#endif
