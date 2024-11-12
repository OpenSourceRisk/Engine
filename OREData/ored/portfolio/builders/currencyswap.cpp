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

#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/model/fxbsdata.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/portfolio/builders/currencyswap.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/scripting/engines/amccgcurrencyswapengine.hpp>

#include <qle/pricingengines/mccamcurrencyswapengine.hpp>
#include <qle/models/projectedcrossassetmodel.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

namespace {
struct CcyComp {
    bool operator()(const Currency& c1, const Currency& c2) const { return c1.code() < c2.code(); }
};
} // namespace

QuantLib::ext::shared_ptr<PricingEngine>
CamAmcCurrencySwapEngineBuilder::engineImpl(const std::vector<Currency>& ccys, const Currency& base,
                                            bool useXccyYieldCurves, const std::set<std::string>& eqNames) {

    std::set<Currency, CcyComp> allCurrencies(ccys.begin(), ccys.end());
    allCurrencies.insert(base);

    std::string ccysStr = base.code();
    for (auto const& c : ccys) {
        ccysStr += "_" + c.code();
    }

    // add currencies from equities

    for (auto const& eq : eqNames) {
        allCurrencies.insert(market_->equityCurve(eq, configuration(MarketContext::pricing))->currency());
    }

    DLOG("Building multi leg option engine for ccys " << ccysStr << " (from externally given CAM)");

    QL_REQUIRE(!ccys.empty(), "CamMcMultiLegOptionEngineBuilder: no currencies given");

    bool needBaseCcy = allCurrencies.size() > 1;

    std::set<std::pair<CrossAssetModel::AssetType, Size>> selectedComponents;
    if(needBaseCcy)
        selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::IR, 0));
    for(auto const& c: allCurrencies) {
        Size ccyIdx = cam_->ccyIndex(c);
        selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::IR, ccyIdx));
        if(ccyIdx > 0 && needBaseCcy)
            selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::FX,ccyIdx-1));
    }
    for (auto const& eq : eqNames) {
        selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::EQ, cam_->eqIndex(eq)));
    }
    std::vector<Size> externalModelIndices;
    Handle<CrossAssetModel> model(getProjectedCrossAssetModel(cam_, selectedComponents, externalModelIndices));

    // we assume that the model has the pricing discount curves attached already, so
    // we leave the discountCurves vector empty here

    // build the pricing engine

    auto engine = QuantLib::ext::make_shared<McCamCurrencySwapEngine>(
        model, ccys, base, parseSequenceType(engineParameter("Training.Sequence")),
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

QuantLib::ext::shared_ptr<PricingEngine>
AmcCgCurrencySwapEngineBuilder::engineImpl(const std::vector<Currency>& ccys, const Currency& base,
                                           bool useXccyYieldCurves, const std::set<std::string>& eqNames) {
    QL_REQUIRE(modelCg_ != nullptr, "AmcCgSwapEngineBuilder::engineImpl: modelcg is null");
    std::vector<std::string> ccysStr;
    std::transform(ccys.begin(), ccys.end(), std::back_inserter(ccysStr), [](const Currency& c) { return c.code(); });
    return QuantLib::ext::make_shared<AmcCgCurrencySwapEngine>(
        ccysStr, base.code(), modelCg_, simulationDates_, stickyCloseOutDates_,
        parseBool(engineParameter("RecalibrateOnStickyCloseOutDates", {}, false, "false")),
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")));
}

} // namespace data
} // namespace ore
