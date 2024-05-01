/*
 Copyright (C) 2021 Quaternion Risk Management Ltd.
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

/*! \file qle/pricingengines/fddefaultableequityjumpdiffusionconvertiblebondengine.hpp */

#pragma once

#include <qle/instruments/convertiblebond2.hpp>
#include <qle/models/defaultableequityjumpdiffusionmodel.hpp>

#include <qle/indexes/fxindex.hpp>

#include <ql/methods/finitedifferences/meshers/fdm1dmesher.hpp>

namespace QuantExt {

class FdDefaultableEquityJumpDiffusionConvertibleBondEngine : public ConvertibleBond2::engine {
public:
    /* - The discounting curve / discounting spread replaces the model rate r for discounting purposes.
       - The credit curve - if given - adds an additional discounting and recovery term, related to the
         bond credit component, while in this case the model credit component is linked to the equity only. */
    explicit FdDefaultableEquityJumpDiffusionConvertibleBondEngine(
        const Handle<DefaultableEquityJumpDiffusionModel>& model,
        const Handle<QuantLib::YieldTermStructure>& discountingCurve = Handle<QuantLib::YieldTermStructure>(),
        const Handle<QuantLib::Quote>& discountingSpread = Handle<QuantLib::Quote>(),
        const Handle<QuantLib::DefaultProbabilityTermStructure>& creditCurve =
            Handle<QuantLib::DefaultProbabilityTermStructure>(),
        const Handle<QuantLib::Quote>& recoveryRate = Handle<QuantLib::Quote>(),
        const Handle<QuantExt::FxIndex>& fxConversion = Handle<QuantExt::FxIndex>(), const bool staticMesher = false,
        const Size timeStepsPerYear = 24, const Size stateGridPoints = 100, const Real mesherEpsilon = 1E-4,
        const Real mesherScaling = 1.5,
        const std::vector<Real> conversionRatioDiscretisationGrid = {0.1, 0.5, 0.7, 0.9, 1.0, 1.1, 1.3, 1.5, 2.0, 5.0,
                                                                     10.0},
        const bool generateAdditionalResults = true);

private:
    void calculate() const override;

    Handle<DefaultableEquityJumpDiffusionModel> model_;
    Handle<QuantLib::YieldTermStructure> discountingCurve_;
    Handle<QuantLib::Quote> discountingSpread_;
    Handle<QuantLib::DefaultProbabilityTermStructure> creditCurve_;
    Handle<QuantLib::Quote> recoveryRate_;
    Handle<QuantExt::FxIndex> fxConversion_;
    bool staticMesher_;
    Size timeStepsPerYear_;
    Size stateGridPoints_;
    Real mesherEpsilon_;
    Real mesherScaling_;
    std::vector<Real> conversionRatioDiscretisationGrid_;
    bool generateAdditionalResults_;

    mutable QuantLib::ext::shared_ptr<Fdm1dMesher> mesher_;
};

} // namespace QuantExt
