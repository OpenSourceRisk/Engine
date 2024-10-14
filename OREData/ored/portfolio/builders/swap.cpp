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
#include <ored/scripting/engines/amccgswapengine.hpp>

#include <ql/methods/montecarlo/lsmbasissystem.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/projectedcrossassetmodel.hpp>
#include <qle/pricingengines/mclgmswapengine.hpp>

#include <set>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

QuantLib::ext::shared_ptr<PricingEngine>
CamAmcSwapEngineBuilder::buildMcEngine(const QuantLib::ext::shared_ptr<LGM>& lgm,
                                       const Handle<YieldTermStructure>& discountCurve,
                                       const std::vector<Size>& externalModelIndices) {

    return QuantLib::ext::make_shared<QuantExt::McLgmSwapEngine>(
        lgm, parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")), discountCurve, simulationDates_,
        stickyCloseOutDates_, externalModelIndices, parseBool(engineParameter("MinObsDate")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())),
        parseBool(engineParameter("RecalibrateOnStickyCloseOutDates", {}, false, "false")),
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")));
}

QuantLib::ext::shared_ptr<PricingEngine> CamAmcSwapEngineBuilder::engineImpl(const ore::data::Swap* swap,
                                                                             const Currency& ccy,
                                                                             const std::string& discountCurveName,
                                                                             const std::string& securitySpread) {
    DLOG("Building AMC Swap engine for ccy " << ccy << " (from externally given CAM)");

    QL_REQUIRE(cam_ != nullptr, "LgmAmcSwapEngineBuilder::engineImpl: cam is null");

    // collect currencies and equities from indexing and from equity legs

    std::set<std::string> allCurrencies{ccy.code()};
    std::set<std::string> allEquities;

    for (auto const& ld : swap->legData()) {
        for (auto const& ind : ld.indexing()) {
            if (ind.hasData()) {
                if (boost::starts_with(ind.index(), "FX-")) {
                    auto index = parseFxIndex(ind.index());
                    allCurrencies.insert(index->targetCurrency().code());
                    allCurrencies.insert(index->sourceCurrency().code());
                } else if (boost::starts_with(ind.index(), "EQ-")) {
                    auto index = parseEquityIndex(ind.index());
                    allCurrencies.insert(
                        market_->equityCurve(index->name(), configuration(MarketContext::pricing))->currency().code());
                    allEquities.insert(index->name());
                }
            }
        }
        if (ld.legType() == "Equity") {
            allEquities.insert(
                parseEquityIndex(boost::dynamic_pointer_cast<EquityLegData>(ld.concreteLegData())->eqName())->name());
        }
    }

    // get projected model

    bool needBaseCcy = allCurrencies.size() > 1;

    std::set<std::pair<CrossAssetModel::AssetType, Size>> selectedComponents;
    for (Size i = 0; i < cam_->components(CrossAssetModel::AssetType::IR); ++i) {
        if ((i == 0 && needBaseCcy) || std::find(allCurrencies.begin(), allCurrencies.end(),
                                                 cam_->irlgm1f(i)->currency().code()) != allCurrencies.end()) {
            selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::IR, i));
            if (i > 0)
                selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::FX, i - 1));
        }
    }
    for (auto const& eqName : allEquities) {
        selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::EQ, cam_->eqIndex(eqName)));
    }
    std::vector<Size> externalModelIndices;
    Handle<CrossAssetModel> model(getProjectedCrossAssetModel(cam_, selectedComponents, externalModelIndices));

    // build mc engine

    Handle<YieldTermStructure> discountCurve =
        discountCurveName.empty()
            ? market_->discountCurve(ccy.code(), configuration(MarketContext::pricing))
            : indexOrYieldCurve(market_, discountCurveName, configuration(MarketContext::pricing));

    return buildMcEngine(model->lgm(0), discountCurve, externalModelIndices);
}

QuantLib::ext::shared_ptr<PricingEngine> AmcCgSwapEngineBuilder::engineImpl(const ore::data::Swap* swap,
                                                                            const Currency& ccy,
                                                                            const std::string& discountCurveName,
                                                                            const std::string& securitySpread) {
    DLOG("Building AMCCG Swap engine for ccy " << ccy << " (from externally given modelcg)");
    QL_REQUIRE(modelCg_ != nullptr, "AmcCgSwapEngineBuilder::engineImpl: modelcg is null");
    return QuantLib::ext::make_shared<AmcCgSwapEngine>(
        ccy.code(), modelCg_, simulationDates_, stickyCloseOutDates_,
        parseBool(engineParameter("RecalibrateOnStickyCloseOutDates", {}, false, "false")),
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")));
}

} // namespace data
} // namespace ore
