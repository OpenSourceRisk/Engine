/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/models/combspiecewiseconstantparametrization.hpp>

namespace QuantExt {

ComBsPiecewiseConstantParametrization::ComBsPiecewiseConstantParametrization(
    const Currency& currency, const std::string& name, const Handle<CommodityIndex>& index,
    const Handle<Quote>& fxSpotToday, const Array& times, const Array& sigma)
    : ComBsParametrization(currency, name, index, fxSpotToday),
      PiecewiseConstantHelper1(times) {
    initialize(sigma);
}

void ComBsPiecewiseConstantParametrization::initialize(const Array& sigma) {
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
