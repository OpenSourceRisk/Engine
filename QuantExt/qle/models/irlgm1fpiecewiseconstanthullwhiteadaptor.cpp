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
    initialize(sigma, kappa);
}

IrLgm1fPiecewiseConstantHullWhiteAdaptor::
    IrLgm1fPiecewiseConstantHullWhiteAdaptor(
        const Currency &currency,
        const Handle<YieldTermStructure> &termStructure,
        const std::vector<Date> &dates, const Array &sigma, const Array &kappa)
    : IrLgm1fParametrization(currency, termStructure),
      PiecewiseConstantHelper3(dates, termStructure),
      PiecewiseConstantHelper2(dates, termStructure) {
    initialize(sigma, kappa);
}

void IrLgm1fPiecewiseConstantHullWhiteAdaptor::initialize(const Array &sigma,
                                                          const Array &kappa) {
    QL_REQUIRE(PiecewiseConstantHelper3::t().size() + 1 == sigma.size(),
               "sigma size (" << sigma.size()
                              << ") inconsistent to times size ("
                              << PiecewiseConstantHelper3::t().size() << ")");
    QL_REQUIRE(PiecewiseConstantHelper2::t().size() + 1 == kappa.size(),
               "kappa size (" << kappa.size()
                              << ") inconsistent to times size ("
                              << PiecewiseConstantHelper2::t().size() << ")");

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
