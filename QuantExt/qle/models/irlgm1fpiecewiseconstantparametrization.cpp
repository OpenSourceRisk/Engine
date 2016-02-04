/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <ql/math/comparison.hpp>

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
    initialize(alpha, kappa);
}

IrLgm1fPiecewiseConstantParametrization::
    IrLgm1fPiecewiseConstantParametrization(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure,
        const std::vector<Date> &alphaDates, const Array &alpha,
        const std::vector<Date> &kappaDates, const Array &kappa)
    : IrLgm1fParametrization(currency, termStructure),
      PiecewiseConstantHelper1(alphaDates, termStructure),
      PiecewiseConstantHelper2(kappaDates, termStructure) {
    initialize(alpha, kappa);
}

void IrLgm1fPiecewiseConstantParametrization::initialize(const Array &alpha,
                                                         const Array &kappa) {
    QL_REQUIRE(PiecewiseConstantHelper1::t().size() + 1 == alpha.size(),
               "alpha size (" << alpha.size()
                              << ") inconsistent to times size ("
                              << PiecewiseConstantHelper1::t().size() << ")");
    QL_REQUIRE(PiecewiseConstantHelper2::t().size() + 1 == kappa.size(),
               "kappa size (" << kappa.size()
                              << ") inconsistent to times size ("
                              << PiecewiseConstantHelper2::t().size() << ")");
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
