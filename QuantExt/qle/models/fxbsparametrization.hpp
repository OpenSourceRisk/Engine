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

/*! \file fxbsparametrization.hpp
    \brief FX Black Scholes parametrization
*/

#ifndef quantext_fxbs_parametrization_hpp
#define quantext_fxbs_parametrization_hpp

#include <qle/models/parametrization.hpp>
#include <ql/handle.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

class FxBsParametrization : public Parametrization {
  public:
    FxBsParametrization(const Currency &foreignCurrency,
                        const Handle<YieldTermStructure> &foreignTermStructure,
                        const Handle<Quote> &fxSpotToday);
    /*! interface */
    virtual Real variance(const Time t) const = 0;
    /*! inspectors */
    virtual Real sigma(const Time t) const;
    virtual Real stdDeviation(const Time t) const;
    const Handle<YieldTermStructure> foreignTermStructure() const;
    const Handle<Quote> fxSpotToday() const;

  private:
    const Handle<YieldTermStructure> foreignTermStructure_;
    const Handle<Quote> fxSpotToday_;
};

// inline

inline Real FxBsParametrization::sigma(const Time t) const {
    return std::sqrt((variance(tr(t)) - variance(tl(t))) / h_);
}

inline Real FxBsParametrization::stdDeviation(const Time t) const {
    return std::sqrt(variance(t));
}

inline const Handle<YieldTermStructure>
FxBsParametrization::foreignTermStructure() const {
    return foreignTermStructure_;
}

inline const Handle<Quote> FxBsParametrization::fxSpotToday() const {
    return fxSpotToday_;
}

} // namespace QuantExt

#endif
