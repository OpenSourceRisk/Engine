/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/fxbsconstantparametrization.hpp>
#include <ql/math/comparison.hpp>

namespace QuantExt {

FxBsConstantParametrization::FxBsConstantParametrization(
    const Currency &currency, const Handle<Quote> &fxSpotToday,
    const Real sigma)
    : FxBsParametrization(currency, fxSpotToday),
      sigma_(boost::make_shared<PseudoParameter>(1)) {
    sigma_->setParam(0, inverse(0, sigma));
}

} // namespace QuantExt
