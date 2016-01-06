/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

#include <qle/models/fxbsparametrization.hpp>

namespace QuantExt {

FxBsParametrization::FxBsParametrization(const Currency &foreignCurrency,
                                         const Handle<Quote> &fxSpotToday)
    : Parametrization(foreignCurrency), fxSpotToday_(fxSpotToday) {}

} // namespace QuantExt
