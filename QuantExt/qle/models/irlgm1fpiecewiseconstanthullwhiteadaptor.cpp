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

#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>

namespace QuantExt {

IrLgm1fPiecewiseConstantHullWhiteAdaptor::
    IrLgm1fPiecewiseConstantHullWhiteAdaptor(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure, const Array &times,
        const Array &sigma, const Array &kappa)
    : IrLgm1fParametrization(currency, termStructure),
      PiecewiseConstantHelper3(times, sigma, kappa),
      PiecewiseConstantHelper2(times, kappa) {
    // store raw parameter values
    for (Size i = 0; i < PiecewiseConstantHelper3::y1_.size(); ++i) {
        PiecewiseConstantHelper3::y1_[i] =
            inverse(0, PiecewiseConstantHelper3::y1_[i]);
    }
    update();
}

} // namespace QuantExt
