/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <qle/termstructures/simplemclocalvolstochasticratescorrection.hpp>

namespace QuantExt {

SimpleMcLocalVolStochasticRatesCorrection::SimpleMcLocalVolStochasticRatesCorrection(
    Handle<LocalVolTermStructure> source, QuantLib::ext::shared_ptr<IrModel> r, QuantLib::ext::shared_ptr<IrModel> q,
    QuantLib::ext::shared_ptr<FxModel> S, std::function<QuantLib::Array(QuantLib::Real, QuantLib::Real)> dwGenerator)
    : source_(std::move(source)), r_(std::move(r)), q_(std::move(q)), S_(std::move(S)),
      dwGenerator_(std::move(dwGenerator)) {}

void SimpleMcLocalVolStochasticRatesCorrection::update() {
    TermStructure::update();
    LazyObject::update();
}

void SimpleMcLocalVolStochasticRatesCorrection::performCalculations() const {

}

Volatility SimpleMcLocalVolStochasticRatesCorrection::localVolImpl(Time t, Real strike) const {
    return source_->localVol(t, strike);
}

} // namespace QuantExt
