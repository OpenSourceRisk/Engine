/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/fxbspiecewiseconstantparametrization.hpp>

namespace QuantExt {

FxBsPiecewiseConstantParametrization::FxBsPiecewiseConstantParametrization(
    const Currency &currency,
    const Handle<YieldTermStructure> &foreignTermStructure,
    const Handle<Quote> &fxSpotToday, const Array &times, const Array &sigma)
    : FxBsParametrization(currency, foreignTermStructure, fxSpotToday),
      PiecewiseConstantHelper1(times) {
    QL_REQUIRE(times.size() + 1 == sigma.size(),
               "alpha size (" << sigma.size()
                              << ") inconsistent to times size ("
                              << times.size() << ")");

    // store raw parameter values
    for (Size i = 0; i < PiecewiseConstantHelper1::y_->size(); ++i) {
        PiecewiseConstantHelper1::y_->setParam(i, inverse(0, sigma[i]));
    }
    update();
}

} // namespace QuantExt
