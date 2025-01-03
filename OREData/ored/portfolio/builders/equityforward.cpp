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

#include <ored/portfolio/builders/equityforward.hpp>

#include <qle/models/projectedcrossassetmodel.hpp>
#include <qle/pricingengines/mccamequityforwardengine.hpp>

namespace ore {
namespace data {

using namespace QuantExt;

EquityForwardEngineBuilderBase::EquityForwardEngineBuilderBase(const std::string& model, const std::string& engine)
    : CachingEngineBuilder(model, engine, {"EquityForward"}) {}

string EquityForwardEngineBuilderBase::keyImpl(const string& equityName, const Currency& ccy) {
    return equityName + "/" + ccy.code();
}

EquityForwardEngineBuilder::EquityForwardEngineBuilder()
    : EquityForwardEngineBuilderBase("DiscountedCashflows", "DiscountingEquityForwardEngine") {}

QuantLib::ext::shared_ptr<PricingEngine> EquityForwardEngineBuilder::engineImpl(const string& equityName,
                                                                                const Currency& ccy) {
    return QuantLib::ext::make_shared<QuantExt::DiscountingEquityForwardEngine>(
        market_->equityCurve(equityName, configuration(MarketContext::pricing)),
        market_->discountCurve(ccy.code(), configuration(MarketContext::pricing)));
}

CamAmcEquityForwardEngineBuilder::CamAmcEquityForwardEngineBuilder(
    const QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel>& cam, const std::vector<Date>& simulationDates,
    const std::vector<Date>& stickyCloseOutDates)
    : EquityForwardEngineBuilderBase("CrossAssetModel", "AMC"), cam_(cam), simulationDates_(simulationDates),
      stickyCloseOutDates_(stickyCloseOutDates) {}

QuantLib::ext::shared_ptr<PricingEngine> CamAmcEquityForwardEngineBuilder::engineImpl(const string& equityName,
                                                                                      const Currency& ccy) {

    DLOG("Building AMC Swap engine for equity " << equityName << " (from externally given CAM)");

    /* ccy is the settlement ccy which can be different from the eq currency - we can ignore this for the purpose of
       amc simulation */

    auto eqCurve = market_->equityCurve(equityName, configuration(MarketContext::pricing));
    Currency eqCcy = eqCurve->currency();

    std::set<std::pair<CrossAssetModel::AssetType, Size>> selectedComponents;
    selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::IR, cam_->ccyIndex(eqCcy)));
    selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::EQ, cam_->eqIndex(equityName)));
    std::vector<Size> externalModelIndices;
    Handle<CrossAssetModel> model(getProjectedCrossAssetModel(cam_, selectedComponents, externalModelIndices));

    return QuantLib::ext::make_shared<QuantExt::McCamEquityForwardEngine>(
        eqCurve, model, parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")), simulationDates_,
        stickyCloseOutDates_, externalModelIndices, parseBool(engineParameter("MinObsDate")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())),
        parseBool(engineParameter("RecalibrateOnStickyCloseOutDates", {}, false, "false")),
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")));
}

} // namespace data
} // namespace ore
