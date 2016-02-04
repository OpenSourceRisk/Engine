/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

IrLgm1fPiecewiseLinearParametrization::IrLgm1fPiecewiseLinearParametrization(
    const Currency &currency, const Handle<YieldTermStructure> &termStructure,
    const Array &alphaTimes, const Array &alpha, const Array &hTimes,
    const Array &h)
    : IrLgm1fParametrization(currency, termStructure),
      PiecewiseConstantHelper11(alphaTimes, hTimes) {
    initialize(alpha, h);
}

IrLgm1fPiecewiseLinearParametrization::IrLgm1fPiecewiseLinearParametrization(
    const Currency &currency, const Handle<YieldTermStructure> &termStructure,
    const std::vector<Date> &alphaDates, const Array &alpha,
    const std::vector<Date> &hDates, const Array &h)
    : IrLgm1fParametrization(currency, termStructure),
      PiecewiseConstantHelper11(alphaDates, hDates, termStructure) {
    initialize(alpha, h);
}

void IrLgm1fPiecewiseLinearParametrization::initialize(const Array &alpha,
                                                       const Array &h) {
    QL_REQUIRE(helper1().t().size() + 1 == alpha.size(),
               "alpha size (" << alpha.size()
                              << ") inconsistent to times size ("
                              << helper1().t().size() << ")");
    QL_REQUIRE(helper2().t().size() + 1 == h.size(),
               "h size (" << h.size() << ") inconsistent to times size ("
                          << helper1().t().size() << ")");
    // store raw parameter values
    for (Size i = 0; i < helper1().p()->size(); ++i) {
        helper1().p()->setParam(i, inverse(0, alpha[i]));
    }
    for (Size i = 0; i < helper2().p()->size(); ++i) {
        helper2().p()->setParam(i, inverse(1, h[i]));
    }
    update();
}

} // namespace QuantExt
