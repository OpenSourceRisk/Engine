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

#include <qle/pricingengines/numericlgmcallablebondengine.hpp>

namespace QuantExt {

NumericLgmCallableBondEngineBase::NumericLgmCallableBondEngineBase(
    const QuantLib::ext::shared_ptr<LgmBackwardSolver>& solver, const Size americanExerciseTimeStepsPerYear,
    const Handle<QuantLib::YieldTermStructure>& referenceCurve, const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate)
    : solver_(solver), americanExerciseTimeStepsPerYear_(americanExerciseTimeStepsPerYear),
      referenceCurve_(referenceCurve), discountingSpread_(discountingSpread), creditCurve_(creditCurve),
      incomeCurve_(incomeCurve), recoveryRate_(recoveryRate) {}

NumericLgmCallableBondEngine::NumericLgmCallableBondEngine(
    const Handle<LGM>& model, const Real sy, const Size ny, const Real sx, const Size nx,
    const Size americanExerciseTimeStepsPerYear, const Handle<QuantLib::YieldTermStructure>& referenceCurve,
    const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate)
    : NumericLgmCallableBondEngineBase(QuantLib::ext::make_shared<LgmConvolutionSolver2>(*model, sy, ny, sx, nx),
                                       americanExerciseTimeStepsPerYear, referenceCurve, discountingSpread, creditCurve,
                                       incomeCurve, recoveryRate) {
    registerWith(solver_->model());
    registerWith(referenceCurve_);
    registerWith(discountingSpread_);
    registerWith(creditCurve_);
    registerWith(incomeCurve_);
    registerWith(recoveryRate_);
}

NumericLgmCallableBondEngine::NumericLgmCallableBondEngine(
    const Handle<LGM>& model, const Real maxTime, const QuantLib::FdmSchemeDesc scheme, const Size stateGridPoints,
    const Size timeStepsPerYear, const Real mesherEpsilon, const Size americanExerciseTimeStepsPerYear,
    const Handle<QuantLib::YieldTermStructure>& referenceCurve, const Handle<QuantLib::Quote>& discountingSpread,
    const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve,
    const Handle<QuantLib::YieldTermStructure>& incomeCurve, const Handle<QuantLib::Quote>& recoveryRate)
    : NumericLgmCallableBondEngineBase(QuantLib::ext::make_shared<LgmFdSolver>(*model, maxTime, scheme, stateGridPoints,
                                                                               timeStepsPerYear, mesherEpsilon),
                                       americanExerciseTimeStepsPerYear, referenceCurve, discountingSpread, creditCurve,
                                       incomeCurve, recoveryRate) {
    registerWith(solver_->model());
    registerWith(referenceCurve_);
    registerWith(discountingSpread_);
    registerWith(creditCurve_);
    registerWith(incomeCurve_);
    registerWith(recoveryRate_);
}

void NumericLgmCallableBondEngine::calculate() const {}

} // namespace QuantExt
