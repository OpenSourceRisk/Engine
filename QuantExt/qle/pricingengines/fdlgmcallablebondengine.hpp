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

#pragma once

#include <qle/instruments/callablebond.hpp>
#include <qle/models/lgm.hpp>

#include <ql/methods/finitedifferences/meshers/fdm1dmesher.hpp>

#include <ql/termstructures/defaulttermstructure.hpp>

namespace QuantExt {

class FdLgmCallableBondEngine : public CallableBond::engine {
public:
    FdLgmCallableBondEngine(
        const Handle<LGM>& model,
        const Handle<QuantLib::YieldTermStructure>& discountingCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& discountingSpread = Handle<QuantLib::Quote>(),
        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve =
            Handle<QuantLib::DefaultProbabilityTermStructure>(),
        const Handle<QuantLib::Quote>& recoveryRate = Handle<QuantLib::Quote>(), const bool mesherIsStatic = false,
        const Size timeStepsPerYear = 24, const Size stateGridPoints = 100, const Real mesherEpsilon = 1E-4,
        const Real mesherScaling = 1.5, const bool generateAdditionalResults = true);

private:
    void calculate() const override;

    Handle<LGM> model_;
    Handle<QuantLib::YieldTermStructure> discountingCurve_;
    Handle<QuantLib::Quote> discountingSpread_;
    Handle<QuantLib::DefaultProbabilityTermStructure> creditCurve_;
    Handle<QuantLib::Quote> recoveryRate_;
    bool mesherIsStatic_;
    Size timeStepsPerYear_;
    Size stateGridPoints_;
    Real mesherEpsilon_;
    Real mesherScaling_;
    bool generateAdditionalResults_;

    mutable boost::shared_ptr<Fdm1dMesher> mesher_;
};

} // namespace QuantExt
