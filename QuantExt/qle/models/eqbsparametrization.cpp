/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/eqbsparametrization.hpp>

namespace QuantExt {

EqBsParametrization::EqBsParametrization(
    const Currency &currency, const Handle<Quote> spotToday,
    const Handle<YieldTermStructure> &dividendTermStructure)
    : : Parametrization(currency),
        spotToday_(spotToday),
        dividendTermStructure_(dividendTermStructure) {}

} // namespace QuantExt
