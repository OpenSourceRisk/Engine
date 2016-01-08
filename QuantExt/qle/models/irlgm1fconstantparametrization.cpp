/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

IrLgm1fConstantParametrization::IrLgm1fConstantParametrization(
    const Currency &currency, const Handle<YieldTermStructure> &termStructure,
    const Real alpha, const Real kappa, const Real shift, const Real scaling)
    : IrLgm1fParametrization(currency, termStructure),
      alpha_(boost::make_shared<PseudoParameter>(1)),
      kappa_(boost::make_shared<PseudoParameter>(1)), shift_(shift),
      scaling_(scaling), zeroKappaCutoff_(1.0E-6) {
    QL_REQUIRE(!close_enough(scaling, 0.0),
               "scaling (" << scaling << ") must be non-zero");
    alpha_->setParam(0, inverse(0, alpha));
    kappa_->setParam(0, inverse(1, kappa));
}

} // namespace QuantExt
