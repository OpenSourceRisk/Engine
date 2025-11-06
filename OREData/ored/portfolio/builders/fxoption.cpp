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
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/scripting/engines/amccgfxoptionengine.hpp>

#include <qle/models/projectedcrossassetmodel.hpp>
#include <qle/pricingengines/mccamfxoptionengine.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

template <typename E>
QuantLib::ext::shared_ptr<PricingEngine>
CamAmcFxOptionEngineBuilderBase::engineImplBase(const string& assetName, const Currency& domCcy,
                                                const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                const bool useFxSpot) {

    QL_REQUIRE(assetClassUnderlying == AssetClass::FX, "FX Option required");
    Currency forCcy = parseCurrency(assetName);

    std::string ccysStr = forCcy.code() + "_" + domCcy.code();

    DLOG("Building AMC FX option engine for ccys " << ccysStr << " (from externally given CAM)");

    QL_REQUIRE(domCcy != forCcy, "CamAmcFxOptionEngineBuilder: domCcy = forCcy = " << domCcy.code());

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

    return QuantLib::ext::make_shared<E>(
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
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")),
        parseInteger(engineParameter("CashflowGeneration.OnCpnMaxSimTimes", {}, false, "1")),
        parsePeriod(engineParameter("CashflowGeneration.OnCpnAddSimTimesCutoff", {}, false, "0D")),
        parseInteger(engineParameter("Regression.MaxSimTimesIR", {}, false, "0")),
        parseInteger(engineParameter("Regression.MaxSimTimesFX", {}, false, "0")),
        parseInteger(engineParameter("Regression.MaxSimTimesEQ", {}, false, "0")),
        parseVarGroupMode(engineParameter("Regression.VarGroupMode", {}, false, "Global")));
}

QuantLib::ext::shared_ptr<PricingEngine>
CamAmcFxEuropeanOptionEngineBuilder::engineImpl(const string& assetName, const Currency& domCcy, const string& discountCurveName,
                                                const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                const bool useFxSpot, const std::optional<Currency>&) {
    return engineImplBase<McCamFxOptionEngine>(assetName, domCcy, assetClassUnderlying, expiryDate, useFxSpot);
}

QuantLib::ext::shared_ptr<PricingEngine>
CamAmcFxEuropeanForwardOptionEngineBuilder::engineImpl(const string& assetName, const Currency& domCcy, const string& discountCurveName,
                                                       const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                       const bool useFxSpot, const std::optional<Currency>&) {
    return engineImplBase<McCamFxEuropeanForwardOptionEngine>(assetName, domCcy, assetClassUnderlying, expiryDate,
                                                              useFxSpot);
}

QuantLib::ext::shared_ptr<PricingEngine>
CamAmcFxEuropeanCSOptionEngineBuilder::engineImpl(const string& assetName, const Currency& domCcy, const string& discountCurveName,
                                                  const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                  const bool useFxSpot, const std::optional<Currency>&) {
    return engineImplBase<McCamFxEuropeanCSOptionEngine>(assetName, domCcy, assetClassUnderlying, expiryDate,
                                                         useFxSpot);
}

template <typename E>
QuantLib::ext::shared_ptr<PricingEngine>
AmcCgFxOptionEngineBuilderBase::engineImplBase(const string& assetName, const Currency& domCcy, const string& discountCurveName,
                                               const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                               const bool useFxSpot, const std::optional<Currency>&) {

    QL_REQUIRE(assetClassUnderlying == AssetClass::FX, "FX Option required");
    Currency forCcy = parseCurrency(assetName);

    std::string ccysStr = forCcy.code() + "_" + domCcy.code();

    DLOG("Building AMC FX option engine for ccys " << ccysStr << " (from externally given CAM)");

    QL_REQUIRE(domCcy != forCcy, "AmcCgFxOptionEngineBuilder: domCcy = forCcy = " << domCcy.code());

    return QuantLib::ext::make_shared<E>(
        domCcy.code(), forCcy.code(), modelCg_, simulationDates_,
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")));
}

QuantLib::ext::shared_ptr<PricingEngine>
AmcCgFxEuropeanOptionEngineBuilder::engineImpl(const string& assetName, const Currency& domCcy, const string& discountCurveName,
                                               const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                               const bool useFxSpot, const std::optional<Currency>&) {
    return engineImplBase<AmcCgFxOptionEngine>(assetName, domCcy, discountCurveName, assetClassUnderlying, expiryDate, useFxSpot, std::nullopt);
}

QuantLib::ext::shared_ptr<PricingEngine>
AmcCgFxEuropeanForwardOptionEngineBuilder::engineImpl(const string& assetName, const Currency& domCcy, const string& discountCurveName,
                                                      const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                      const bool useFxSpot, const std::optional<Currency>&) {
    return engineImplBase<AmcCgFxEuropeanForwardOptionEngine>(assetName, domCcy, discountCurveName, assetClassUnderlying, expiryDate,
                                                              useFxSpot, std::nullopt);
}

QuantLib::ext::shared_ptr<PricingEngine>
AmcCgFxEuropeanCSOptionEngineBuilder::engineImpl(const string& assetName, const Currency& domCcy, const string& discountCurveName,
                                                 const AssetClass& assetClassUnderlying, const Date& expiryDate,
                                                 const bool useFxSpot, const std::optional<Currency>&) {
    return engineImplBase<AmcCgFxEuropeanCSOptionEngine>(assetName, domCcy, discountCurveName, assetClassUnderlying, expiryDate,
                                                         useFxSpot, std::nullopt);
}

} // namespace data
} // namespace ore
