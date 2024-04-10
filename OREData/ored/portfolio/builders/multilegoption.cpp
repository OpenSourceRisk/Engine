/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/multilegoption.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/crossassetmodeldata.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/model/fxbsdata.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/pricingengines/mcmultilegoptionengine.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

namespace {
struct CcyComp {
    bool operator()(const Currency& c1, const Currency& c2) const { return c1.code() < c2.code(); }
};
} // namespace

std::string CamMcMultiLegOptionEngineBuilder::getCcyValue(const string& s, const string& c, const bool mandatory) {
    std::string sc = s + "_" + c;
    auto it = modelParameters_.find(sc);
    if (it != modelParameters_.end()) {
        return it->second;
    }
    it = modelParameters_.find(s);
    if (it != modelParameters_.end()) {
        return it->second;
    }
    if (mandatory)
        QL_FAIL("did not find model parameter " << s << " (when looking for ccy " << c << ")");
    return "";
}

QuantLib::ext::shared_ptr<PricingEngine> CamMcMultiLegOptionEngineBuilder::engineImpl(
    const string& id, const std::vector<Date>& exDates, const Date& maturityDate,
    const std::vector<Currency>& currencies, const std::vector<Date>& fixingDates,
    const std::vector<QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexes) {

    DLOG("Building multi leg option engine for trade " << id << "...");

    QL_REQUIRE(!currencies.empty(), "CamMcMultiLegOptionEngineBuilder: no currencies given");
    QL_REQUIRE(fixingDates.size() == indexes.size(), "CamMcMultiLegOptionEngineBuilder: fixing dates size ("
                                                         << fixingDates.size() << ") must match indexes size ("
                                                         << indexes.size() << ")");

    std::vector<Handle<YieldTermStructure>> discountCurves;
    auto irCalibration = parseCalibrationType(modelParameters_.at("IrCalibration"));
    auto irCalibrationStrategy = parseCalibrationStrategy(modelParameters_.at("IrCalibrationStrategy"));
    std::vector<std::pair<CalibrationType, CalibrationStrategy>> validCalPairs = {
        {CalibrationType::None, CalibrationStrategy::None},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalATM},
        {CalibrationType::Bootstrap, CalibrationStrategy::UnderlyingATM}};

    auto fxCalibration = parseCalibrationType(modelParameters_.at("FxCalibration"));
    QL_REQUIRE(fxCalibration == CalibrationType::None || fxCalibration == CalibrationType::Bootstrap,
               "CamMcMultiLegOptionEngineBuilder: invalid FxCalibration (" << fxCalibration << ")");

    QL_REQUIRE(std::find(validCalPairs.begin(), validCalPairs.end(),
                         std::make_pair(irCalibration, irCalibrationStrategy)) != validCalPairs.end(),
               "Calibration (" << irCalibration << ") and CalibrationStrategy (" << irCalibrationStrategy
                               << ") are not allowed in this combination");

    Real tolerance = parseReal(modelParameters_.at("Tolerance"));

    // shift horizon

    Real shiftHorizon = 0.5; // default value
    if (modelParameters_.find("ShiftHorizon") != modelParameters_.end()) {
        shiftHorizon = parseReal(modelParameters_.at("ShiftHorizon"));
    }
    Date today = Settings::instance().evaluationDate();
    shiftHorizon = ActualActual(ActualActual::ISDA).yearFraction(today, maturityDate) * shiftHorizon;

    // build calibration basket data

    vector<string> swaptionExStr, swaptionTermStr, swaptionStrikesStr, fxStrikesStr;
    map<Currency, string, CcyComp> swaptionTermCcyStr; // will be duplicated to a vector below
    if (irCalibrationStrategy == CalibrationStrategy::CoterminalATM) {
        DLOG("Building calibration basket from exercise dates");
        for (Size i = 0; i < exDates.size(); ++i) {
            swaptionExStr.push_back(ore::data::to_string(QuantLib::io::iso_date(exDates[i])));
            swaptionTermStr.push_back(ore::data::to_string(QuantLib::io::iso_date(maturityDate)));
            swaptionStrikesStr.push_back("ATM");
            fxStrikesStr.push_back("ATMF");
            DLOG("added exercsie " << swaptionExStr.back() << ", term " << swaptionTermStr.back() << ", strike "
                                   << swaptionStrikesStr.back());
        }
    } else if (irCalibrationStrategy == CalibrationStrategy::UnderlyingATM) {
        DLOG("Building calibration basket from underlying fixing dates");
        // find unique set of fixing dates
        std::vector<Date> allFixingDates(fixingDates.begin(), fixingDates.end());
        std::sort(allFixingDates.begin(), allFixingDates.end());
        auto it = std::unique(allFixingDates.begin(), allFixingDates.end());
        allFixingDates.resize(it - allFixingDates.begin());
        for (auto const& d : allFixingDates) {
            swaptionExStr.push_back(ore::data::to_string(QuantLib::io::iso_date(d)));
            swaptionTermStr.push_back("1Y"); // default value, if we don't have anything for a ccy
            swaptionStrikesStr.push_back("ATM");
            fxStrikesStr.push_back("ATMF");
            DLOG("added exercise " << swaptionExStr.back() << ", term tbd, strike " << swaptionStrikesStr.back());
        }
        // Find a tenor for each currency (there may be several of course, just pick one...)
        for (auto const& i : indexes) {
            if (swaptionTermCcyStr.find(i->currency()) == swaptionTermCcyStr.end())
                swaptionTermCcyStr[i->currency()] = ore::data::to_string(i->tenor());
        }
        // Logging
        for (auto const& k : swaptionTermCcyStr) {
            DLOG("added term " << k.second << " for ccy " << k.first.code());
        }
    } else if (irCalibrationStrategy == CalibrationStrategy::None) {
        // nothing to do
    } else {
        QL_FAIL("IR Calibration Strategy " << irCalibrationStrategy
                                           << " not supported by CamMcMultiLegOptionEngineBuilder");
    }

    // ir components

    std::vector<QuantLib::ext::shared_ptr<IrModelData>> irData;
    for (Size i = 0; i < currencies.size(); ++i) {
        DLOG("IR component #" << i << " (" << currencies[i].code() << ")");
        discountCurves.push_back(market_->discountCurve(currencies[i].code(), configuration(MarketContext::pricing)));
        Real rev = parseReal(getCcyValue("IrReversion", currencies[i].code(), true));
        vector<Real> vols =
            parseListOfValues<Real>(getCcyValue("IrVolatility", currencies[i].code(), true), &parseReal);
        vector<Real> volTimes =
            parseListOfValues<Real>(getCcyValue("IrVolatilityTimes", currencies[i].code(), false), &parseReal);
        QL_REQUIRE(vols.size() == volTimes.size() + 1, "there must be n+1 volatilities ("
                                                           << vols.size() << ") for n volatility times ("
                                                           << volTimes.size() << "), for ccy " << currencies[i]);
        auto reversionType = parseReversionType(getCcyValue("IrReversionType", currencies[i].code(), true));
        auto volatilityType = parseVolatilityType(getCcyValue("IrVolatilityType", currencies[i].code(), true));
        auto lgmData = QuantLib::ext::make_shared<IrLgmData>();
        lgmData->reset();
        lgmData->ccy() = currencies[i].code();
        lgmData->calibrateH() = false;
        lgmData->hParamType() = ParamType::Constant;
        lgmData->hValues() = {rev};
        lgmData->reversionType() = reversionType;
        lgmData->calibrateA() = false;
        lgmData->aParamType() = ParamType::Piecewise;
        lgmData->aValues() = vols;
        lgmData->aTimes() = volTimes;
        lgmData->volatilityType() = volatilityType;
        lgmData->calibrationType() = irCalibration;
        lgmData->shiftHorizon() = i == 0 ? shiftHorizon : 0.0; // only for domestic component
        lgmData->optionExpiries() = swaptionExStr;
        // currency specific term or default term
        if (swaptionTermCcyStr.find(currencies[i]) != swaptionTermCcyStr.end())
            lgmData->optionTerms() =
                std::vector<std::string>(swaptionTermStr.size(), swaptionTermCcyStr.at(currencies[i]));
        else
            lgmData->optionTerms() = swaptionTermStr;
        lgmData->optionStrikes() = swaptionStrikesStr;
        lgmData->calibrateA() = irCalibration == CalibrationType::Bootstrap;
        irData.push_back(lgmData);
    }

    // fx components

    std::vector<QuantLib::ext::shared_ptr<FxBsData>> fxData;
    for (Size i = 1; i < currencies.size(); ++i) {
        string ccyPair = currencies[i].code() + currencies.front().code();
        DLOG("FX component # " << (i - 1) << " (" << ccyPair << ")");
        auto bsData = QuantLib::ext::make_shared<FxBsData>();
        vector<Real> vols = parseListOfValues<Real>(getCcyValue("FxVolatility", ccyPair, true), &parseReal);
        vector<Real> volTimes =
            parseListOfValues<Real>(getCcyValue("FxVolatilityTimes", currencies[i].code(), false), &parseReal);
        QL_REQUIRE(vols.size() == volTimes.size() + 1, "there must be n+1 FX volatilities ("
                                                           << vols.size() << ") for n volatility times ("
                                                           << volTimes.size() << "), for pair " << ccyPair);
        // cut off parts where we have to extrapolate ?
        std::vector<string> fxExStr(swaptionExStr);
        if (!parseBool(getCcyValue("ExtrapolateFxVolatility", ccyPair, true))) {
            Size n = 0;
            auto fxvol = market_->fxVol(ccyPair, configuration(MarketContext::pricing));
            while (n < fxExStr.size() && parseDate(fxExStr[n]) <= fxvol->maxDate())
                ++n;
            fxExStr.resize(n);
            fxStrikesStr.resize(n);
        }
        bsData->domesticCcy() = currencies.front().code();
        bsData->foreignCcy() = currencies[i].code();
        bsData->calibrationType() = fxCalibration;
        bsData->calibrateSigma() = fxCalibration == CalibrationType::Bootstrap;
        bsData->sigmaParamType() = ParamType::Piecewise;
        bsData->sigmaTimes() = volTimes;
        bsData->sigmaValues() = vols;
        bsData->optionExpiries() = fxExStr;
        bsData->optionStrikes() = fxStrikesStr;
        fxData.push_back(bsData);
    }

    // correlations

    DLOG("Setting correlations (IR-IR, IR-FX, FX-FX)");
    map<CorrelationKey, Handle<Quote>> corr;
    for (auto const& p : modelParameters_) {
        if (p.first.substr(0, 5) == "Corr_") {
            std::vector<std::string> tokens;
            boost::split(tokens, p.first, boost::is_any_of("_"));
            QL_REQUIRE(tokens.size() == 3, "CamMcMultiLegOptionEngineBuilder: invalid correlation key "
                                               << p.first << " expected 'Corr_Key1_Key2'");
            CorrelationFactor f_1 = parseCorrelationFactor(tokens[1]);
            CorrelationFactor f_2 = parseCorrelationFactor(tokens[2]);
            corr[std::make_pair(f_1, f_2)] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(parseReal(p.second)));
            DLOG("added correlation " << tokens[1] << " " << tokens[2] << " " << p.second);
        }
    }

    // build the model

    bool calibrate = globalParameters_.count("Calibrate") == 0 || parseBool(globalParameters_.at("Calibrate"));
    bool continueOnCalibrationError = globalParameters_.count("ContinueOnCalibrationError") > 0 &&
                                      parseBool(globalParameters_.at("ContinueOnCalibrationError"));

    std::string configurationInCcy = configuration(MarketContext::irCalibration);
    std::string configurationXois = configuration(MarketContext::pricing);

    auto builder = QuantLib::ext::make_shared<CrossAssetModelBuilder>(
        market_, QuantLib::ext::make_shared<CrossAssetModelData>(irData, fxData, corr, tolerance), configurationInCcy,
        configurationXois, configurationXois, configurationInCcy, configurationInCcy, configurationXois, !calibrate,
        continueOnCalibrationError, "", SalvagingAlgorithm::Spectral, id);

    modelBuilders_.insert(std::make_pair(id, builder));

    // build the pricing engine

    auto engine = QuantLib::ext::make_shared<McMultiLegOptionEngine>(
        builder->model(), parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")), discountCurves, std::vector<Date>(),
        std::vector<Size>());

    return engine;
}

QuantLib::ext::shared_ptr<PricingEngine> CamAmcMultiLegOptionEngineBuilder::engineImpl(
    const string& id, const std::vector<Date>& exDates, const Date& maturityDate,
    const std::vector<Currency>& currencies, const std::vector<Date>& fixingDates,
    const std::vector<QuantLib::ext::shared_ptr<QuantLib::InterestRateIndex>>& indexes) {

    std::string ccysStr;
    for (auto const& c : currencies) {
        ccysStr += c.code() + "_";
    }

    DLOG("Building multi leg option engine for ccys " << ccysStr << " (from externally given CAM)");

    QL_REQUIRE(!currencies.empty(), "CamMcMultiLegOptionEngineBuilder: no currencies given");
    QL_REQUIRE(fixingDates.size() == indexes.size(), "CamMcMultiLegOptionEngineBuilder: fixing dates size ("
                                                         << fixingDates.size() << ") must match indexes size ("
                                                         << indexes.size() << ")");

    std::vector<Size> externalModelIndices;
    std::vector<Handle<YieldTermStructure>> discountCurves;
    std::vector<Size> cIdx;
    std::vector<QuantLib::ext::shared_ptr<IrModel>> lgm;
    std::vector<QuantLib::ext::shared_ptr<FxBsParametrization>> fx;

    // base ccy is the base ccy of the external cam by definition
    // but in case we only have one currency, we don't need this
    bool needBaseCcy = currencies.size() > 1;

    // add the IR and FX components in the order they appear in the CAM; this way
    // we can sort the external model indices and be sure that they match up with
    // the indices 0,1,2,3,... of the projected model we build here
    for (Size i = 0; i < cam_->components(CrossAssetModel::AssetType::IR); ++i) {
        if ((i == 0 && needBaseCcy) ||
            std::find(currencies.begin(), currencies.end(), cam_->irlgm1f(i)->currency()) != currencies.end()) {
            lgm.push_back(cam_->lgm(i));
            externalModelIndices.push_back(cam_->pIdx(CrossAssetModel::AssetType::IR, i));
            cIdx.push_back(cam_->cIdx(CrossAssetModel::AssetType::IR, i));
            if (i > 0) {
                fx.push_back(cam_->fxbs(i - 1));
                externalModelIndices.push_back(cam_->pIdx(CrossAssetModel::AssetType::FX, i - 1));
                cIdx.push_back(cam_->cIdx(CrossAssetModel::AssetType::FX, i - 1));
            }
        }
    }

    std::sort(externalModelIndices.begin(), externalModelIndices.end());
    std::sort(cIdx.begin(), cIdx.end());

    // build correlation matrix
    Matrix corr(cIdx.size(), cIdx.size(), 1.0);
    for (Size i = 0; i < cIdx.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            corr(i, j) = corr(j, i) = cam_->correlation()(cIdx[i], cIdx[j]);
        }
    }

    Handle<CrossAssetModel> model(QuantLib::ext::make_shared<CrossAssetModel>(lgm, fx, corr));
    // we assume that the model has the pricing discount curves attached already, so
    // we leave the discountCurves vector empty here

    // build the pricing engine

    auto engine = QuantLib::ext::make_shared<McMultiLegOptionEngine>(
        model, parseSequenceType(engineParameter("Training.Sequence")),
        parseSequenceType(engineParameter("Pricing.Sequence")), parseInteger(engineParameter("Training.Samples")),
        parseInteger(engineParameter("Pricing.Samples")), parseInteger(engineParameter("Training.Seed")),
        parseInteger(engineParameter("Pricing.Seed")), parseInteger(engineParameter("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameter("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers")), discountCurves, simulationDates_,
        externalModelIndices, parseBool(engineParameter("MinObsDate")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())));

    return engine;
}

} // namespace data
} // namespace ore
