/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>

namespace QuantExt {

IrLgm1fPiecewiseConstantParametrization::
    IrLgm1fPiecewiseConstantParametrization(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure,
        const Array &alphaTimes, const Array &alpha, const Array &kappaTimes,
        const Array &kappa)
    : IrLgm1fParametrization(currency, termStructure),
      PiecewiseConstantHelper1(alphaTimes),
      PiecewiseConstantHelper2(kappaTimes) {
    QL_REQUIRE(alphaTimes.size() + 1 == alpha.size(),
               "alpha size (" << alpha.size()
                              << ") inconsistent to times size ("
                              << alphaTimes.size() << ")");
    QL_REQUIRE(kappaTimes.size() + 1 == kappa.size(),
               "kappa size (" << kappa.size()
                              << ") inconsistent to times size ("
                              << kappaTimes.size() << ")");
    // store raw parameter values
    for (Size i = 0; i < PiecewiseConstantHelper1::y_->size(); ++i) {
        PiecewiseConstantHelper1::y_->setParam(i, inverse(0, alpha[i]));
    }
    for (Size i = 0; i < PiecewiseConstantHelper2::y_->size(); ++i) {
        PiecewiseConstantHelper2::y_->setParam(i, inverse(1, kappa[i]));
    }
    update();
}

} // namespace QuantExt
