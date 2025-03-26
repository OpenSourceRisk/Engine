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

#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/scripting/engines/amccgfxforwardengine.hpp>

#include <qle/pricingengines/mccamfxforwardengine.hpp>
#include <qle/models/projectedcrossassetmodel.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

QuantLib::ext::shared_ptr<PricingEngine> CamAmcFxForwardEngineBuilder::engineImpl(const Currency& forCcy,
                                                                          const Currency& domCcy) {

    QL_REQUIRE(domCcy != forCcy, "CamAmcFxForwardEngineBuilder: domCcy = forCcy = " << domCcy.code());

    std::set<std::pair<CrossAssetModel::AssetType, Size>> selectedComponents;
    for (Size i = 0; i < cam_->components(CrossAssetModel::AssetType::IR); ++i) {
        if (i == 0 || cam_->irlgm1f(i)->currency() == domCcy || cam_->irlgm1f(i)->currency() == forCcy) {
            selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::IR, i));
            if (i > 0) {
                selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::FX, i - 1));
            }
        }
    }

    std::vector<Size> externalModelIndices;
    Handle<CrossAssetModel> model(getProjectedCrossAssetModel(cam_, selectedComponents, externalModelIndices));

    // build the pricing engine
    // we assume that the model has the pricing discount curves attached already, so
    // we leave the discountCurves vector empty here

    // NPV should be in domCcy, consistent with the npv currency of an ORE FX Forward Trade
    auto engine = QuantLib::ext::make_shared<McCamFxForwardEngine>(
        model, domCcy, forCcy, domCcy, parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")),
        std::vector<Handle<YieldTermStructure>>{}, simulationDates_, stickyCloseOutDates_, externalModelIndices,
        parseBool(engineParameter("MinObsDate")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())),
        parseBool(engineParameter("RecalibrateOnStickyCloseOutDates", {}, false, "false")),
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")));

    return engine;
}

QuantLib::ext::shared_ptr<PricingEngine> AmcCgFxForwardEngineBuilder::engineImpl(const Currency& forCcy,
                                                                                 const Currency& domCcy) {

    QL_REQUIRE(domCcy != forCcy, "AmcCgFxForwardEngineBuilder: domCcy = forCcy = " << domCcy.code());

    auto engine =
        QuantLib::ext::make_shared<AmcCgFxForwardEngine>(domCcy.code(), forCcy.code(), modelCg_, simulationDates_);

    return engine;
}

} // namespace data
} // namespace ore
