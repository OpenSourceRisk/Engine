/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/irlgm1fparametrization.hpp>

namespace QuantExt {

IrLgm1fParametrization::IrLgm1fParametrization(
    const Currency &currency, const Handle<YieldTermStructure> &termStructure)
    : Parametrization(currency), shift_(0.0), scaling_(1.0),
      termStructure_(termStructure) {}

} // namespace QuantExt
