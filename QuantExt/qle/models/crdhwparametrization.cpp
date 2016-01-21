/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/crdhwparametrization.hpp>

namespace QuantExt {

CrdHwParametrization::CrdHwParametrization(
    const Currency &currency,
    const Handle<DefaultProbabilityTermStructure> &defaultTermStructure)
    : Parametrization(currency), defaultTermStructure_(defaultTermStructure) {}

} // namespace QuantExt
