/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

IrLgm1fPiecewiseLinearParametrization::IrLgm1fPiecewiseLinearParametrization(
    const Currency &currency, const Handle<YieldTermStructure> &termStructure,
    const Array &alphaTimes, const Array &alpha, const Array &lambdaTimes,
    const Array &lambda, const Real shift, const Real scaling)
    : IrLgm1fParametrization(currency, termStructure),
      PiecewiseConstantHelper11(alphaTimes, lambdaTimes), shift_(shift),
      scaling_(scaling) {
    QL_REQUIRE(!close_enough(scaling, 0.0),
               "scaling (" << scaling << ") must be non-zero");
    QL_REQUIRE(alphaTimes.size() + 1 == alpha.size(),
               "alpha size (" << alpha.size()
                              << ") inconsistent to times size ("
                              << alphaTimes.size() << ")");
    QL_REQUIRE(lambdaTimes.size() + 1 == lambda.size(),
               "kappa size (" << lambda.size()
                              << ") inconsistent to times size ("
                              << lambdaTimes.size() << ")");
    // store raw parameter values
    for (Size i = 0; i < helper1().p()->size(); ++i) {
        helper1().p()->setParam(i, inverse(0, alpha[i]));
    }
    for (Size i = 0; i < helper2().p()->size(); ++i) {
        helper2().p()->setParam(i, inverse(1, lambda[i]));
    }
    update();
}

} // namespace QuantExt
