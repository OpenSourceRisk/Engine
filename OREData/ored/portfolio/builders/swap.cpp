/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/swap.hpp>

#include <ql/methods/montecarlo/lsmbasissystem.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/pricingengines/mclgmswapengine.hpp>

#include <set>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

QuantLib::ext::shared_ptr<PricingEngine> CamAmcSwapEngineBuilder::buildMcEngine(const QuantLib::ext::shared_ptr<LGM>& lgm,
                                                                        const Handle<YieldTermStructure>& discountCurve,
                                                                        const std::vector<Date>& simulationDates,
                                                                        const std::vector<Size>& externalModelIndices) {

    return QuantLib::ext::make_shared<QuantExt::McLgmSwapEngine>(
        lgm, parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")), discountCurve, simulationDates,
        externalModelIndices, parseBool(engineParameter("MinObsDate")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())));
}

QuantLib::ext::shared_ptr<PricingEngine> CamAmcSwapEngineBuilder::engineImpl(const Currency& ccy,
                                                                     const std::string& discountCurveName,
                                                                     const std::string& securitySpread) {
    DLOG("Building AMC Swap engine for ccy " << ccy << " (from externally given CAM)");

    QL_REQUIRE(cam_ != nullptr, "LgmAmcSwapEngineBuilder::engineImpl: cam is null");
    Size currIdx = cam_->ccyIndex(ccy);
    auto lgm = cam_->lgm(currIdx);
    std::vector<Size> modelIndex(1, cam_->pIdx(CrossAssetModel::AssetType::IR, currIdx));

    // we assume that the given cam has pricing discount curves attached already
    Handle<YieldTermStructure> discountCurve;
    return buildMcEngine(lgm, discountCurve, simulationDates_, modelIndex);
}

} // namespace data
} // namespace ore
