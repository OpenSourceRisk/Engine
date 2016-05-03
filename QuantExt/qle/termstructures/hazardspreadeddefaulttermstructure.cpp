/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <qle/termstructures/hazardspreadeddefaulttermstructure.hpp>

namespace QuantExt {

HazardSpreadedDefaultTermStructure::HazardSpreadedDefaultTermStructure(
    const Handle<DefaultProbabilityTermStructure> &source,
    const Handle<Quote> &spread)
    : source_(source), spread_(spread) {
    if (!source_.empty())
        enableExtrapolation(source_->allowsExtrapolation());
    registerWith(source_);
    registerWith(spread_);
}

} // namespace QuantExt
