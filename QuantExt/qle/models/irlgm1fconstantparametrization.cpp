/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

IrLgm1fConstantParametrization::IrLgm1fConstantParametrization(
    const Currency &currency, const Handle<YieldTermStructure> &termStructure,
    const Real alpha, const Real kappa)
    : IrLgm1fParametrization(currency, termStructure),
      alpha_(boost::make_shared<PseudoParameter>(1)),
      kappa_(boost::make_shared<PseudoParameter>(1)), zeroKappaCutoff_(1.0E-6) {
    alpha_->setParam(0, inverse(0, alpha));
    kappa_->setParam(0, inverse(1, kappa));
}

} // namespace QuantExt
