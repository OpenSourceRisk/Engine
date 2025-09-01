/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file fdlgmcallablebondengine.hpp */

#include <qle/pricingengines/fdlgmcallablebondengine.hpp>

namespace QuantExt {

FdLgmCallableBondEngine::FdLgmCallableBondEngine(const Handle<LGM>& model,
                                                 const Handle<QuantLib::YieldTermStructure>& discountingCurve,
                                                 const Handle<QuantLib::Quote>& discountingSpread,
                                                 const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
                                                 const Handle<QuantLib::Quote>& recoveryRate, const bool mesherIsStatic,
                                                 const Size timeStepsPerYear, const Size stateGridPoints,
                                                 const Real mesherEpsilon, const Real mesherScaling,
                                                 const bool generateAdditionalResults)
    : model_(model), discountingCurve_(discountingCurve), discountingSpread_(discountingSpread),
      creditCurve_(creditCurve), recoveryRate_(recoveryRate), mesherIsStatic_(mesherIsStatic),
      timeStepsPerYear_(timeStepsPerYear), stateGridPoints_(stateGridPoints), mesherEpsilon_(mesherEpsilon),
      mesherScaling_(mesherScaling), generateAdditionalResults_(generateAdditionalResults) {}

void FdLgmCallableBondEngine::calculate() const {}

} // namespace QuantExt
