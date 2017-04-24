/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <qle/models/eqbspiecewiseconstantparametrization.hpp>

namespace QuantExt {

EqBsPiecewiseConstantParametrization::EqBsPiecewiseConstantParametrization(
    const Currency& currency, const std::string& eqName, const Handle<Quote>& eqSpotToday,
    const Handle<Quote>& fxSpotToday, const Array& times, const Array& sigma,
    const Handle<YieldTermStructure>& eqIrCurveToday, const Handle<YieldTermStructure>& eqDivYieldCurveToday)
    : EqBsParametrization(currency, eqName, eqSpotToday, fxSpotToday, eqIrCurveToday, eqDivYieldCurveToday),
      PiecewiseConstantHelper1(times) {
    initialize(sigma);
}

EqBsPiecewiseConstantParametrization::EqBsPiecewiseConstantParametrization(
    const Currency& currency, const std::string& eqName, const Handle<Quote>& eqSpotToday,
    const Handle<Quote>& fxSpotToday, const std::vector<Date>& dates, const Array& sigma,
    const Handle<YieldTermStructure>& domesticTermStructure, const Handle<YieldTermStructure>& eqIrCurveToday,
    const Handle<YieldTermStructure>& eqDivYieldCurveToday)
    : EqBsParametrization(currency, eqName, eqSpotToday, fxSpotToday, eqIrCurveToday, eqDivYieldCurveToday),
      PiecewiseConstantHelper1(dates, domesticTermStructure) {
    initialize(sigma);
}

void EqBsPiecewiseConstantParametrization::initialize(const Array& sigma) {
    QL_REQUIRE(PiecewiseConstantHelper1::t().size() + 1 == sigma.size(),
               "alpha size (" << sigma.size() << ") inconsistent to times size ("
                              << PiecewiseConstantHelper1::t().size() << ")");

    // store raw parameter values
    for (Size i = 0; i < PiecewiseConstantHelper1::y_->size(); ++i) {
        PiecewiseConstantHelper1::y_->setParam(i, inverse(0, sigma[i]));
    }
    update();
}

} // namespace QuantExt
