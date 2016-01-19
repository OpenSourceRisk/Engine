/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

IrLgm1fPiecewiseConstantHullWhiteAdaptor::
    IrLgm1fPiecewiseConstantHullWhiteAdaptor(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure, const Array &times,
        const Array &sigma, const Array &kappa)
    : IrLgm1fParametrization(currency, termStructure),
      PiecewiseConstantHelper3(times), PiecewiseConstantHelper2(times) {
    QL_REQUIRE(times.size() + 1 == sigma.size(),
               "sigma size (" << sigma.size()
                              << ") inconsistent to times size ("
                              << times.size() << ")");
    QL_REQUIRE(times.size() + 1 == kappa.size(),
               "kappa size (" << kappa.size()
                              << ") inconsistent to times size ("
                              << times.size() << ")");

    // store raw parameter values
    for (Size i = 0; i < PiecewiseConstantHelper3::y1_->size(); ++i) {
        PiecewiseConstantHelper3::y1_->setParam(i, inverse(0, sigma[i]));
    }
    for (Size i = 0; i < PiecewiseConstantHelper3::y2_->size(); ++i) {
        PiecewiseConstantHelper3::y2_->setParam(i, inverse(1, kappa[i]));
    }
    for (Size i = 0; i < PiecewiseConstantHelper2::y_->size(); ++i) {
        PiecewiseConstantHelper2::y_->setParam(i, inverse(1, kappa[i]));
    }
    update();
}

} // namespace QuantExt
