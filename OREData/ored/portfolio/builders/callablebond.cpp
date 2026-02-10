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

#include <ored/model/lgmbuilder.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/portfolio/builders/callablebond.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/time/date.hpp>
#include <qle/models/projectedcrossassetmodel.hpp>
#include <qle/pricingengines/mccamcallablebondengine.hpp>
#include <qle/pricingengines/numericlgmcallablebondengine.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

boost::shared_ptr<QuantExt::IrModel> CallableBondLgmEngineBuilder::model(const std::string& id, const std::string& ccy,
                                                                         const QuantLib::Date& maturityDate,
                                                                         const bool generateAdditionalResults) {

    auto calibration = parseCalibrationType(modelParameter("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(modelParameter("CalibrationStrategy"));
    std::string referenceCalibrationGrid = modelParameter("ReferenceCalibrationGrid", {}, true, "");

    Real lambda = parseReal(modelParameter("Reversion", {ccy}));
    vector<Real> sigma = parseListOfValues<Real>(modelParameter("Volatility"), &parseReal);
    vector<Real> sigmaTimes = parseListOfValues<Real>(modelParameter("VolatilityTimes", {}, false), &parseReal);
    QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1, "there must be n+1 volatilities (" << sigma.size()
                                                                                         << ") for n volatility times ("
                                                                                         << sigmaTimes.size() << ")");
    Real tolerance = parseReal(modelParameter("Tolerance"));
    auto reversionType = parseReversionType(modelParameter("ReversionType"));
    auto volatilityType = parseVolatilityType(modelParameter("VolatilityType"));
    bool continueOnCalibrationError = globalParameters_.count("ContinueOnCalibrationError") > 0 &&
                                      parseBool(globalParameters_.at("ContinueOnCalibrationError"));

    bool allowModelFallbacks =
        globalParameters_.count("AllowModelFallbacks") > 0 && parseBool(globalParameters_.at("AllowModelFallbacks"));

    auto floatSpreadMapping = parseFloatSpreadMapping(modelParameter("FloatSpreadMapping", {}, false, "proRata"));

    auto data = boost::make_shared<IrLgmData>();

    // check for allowed calibration / bermudan strategy settings
    std::vector<std::pair<CalibrationType, CalibrationStrategy>> validCalPairs = {
        {CalibrationType::None, CalibrationStrategy::None},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalATM},
        {CalibrationType::BestFit, CalibrationStrategy::CoterminalATM}};

    QL_REQUIRE(std::find(validCalPairs.begin(), validCalPairs.end(),
                         std::make_pair(calibration, calibrationStrategy)) != validCalPairs.end(),
               "Calibration (" << calibration << ") and CalibrationStrategy (" << calibrationStrategy
                               << ") are not allowed in this combination");

    // compute horizon shift
    Real shiftHorizon = parseReal(modelParameter("ShiftHorizon", {}, false, "0.5"));
    Date today = Settings::instance().evaluationDate();
    shiftHorizon = ActualActual(ActualActual::ISDA).yearFraction(today, maturityDate) * shiftHorizon;

    // Default: no calibration, constant lambda and sigma from engine configuration
    data->reset();
    data->qualifier() = ccy;
    data->calibrateH() = false;
    data->hParamType() = ParamType::Constant;
    data->hValues() = {lambda};
    data->reversionType() = reversionType;
    data->calibrateA() = false;
    data->aParamType() = ParamType::Piecewise;
    data->aValues() = sigma;
    data->aTimes() = sigmaTimes;
    data->volatilityType() = volatilityType;
    data->calibrationType() = calibration;
    data->shiftHorizon() = shiftHorizon;
    data->floatSpreadMapping() = floatSpreadMapping;

    std::vector<Date> effExpiries;
    std::vector<Real> effStrikes;
    std::vector<Date> effMaturities;

    DateGrid grid(referenceCalibrationGrid);
    std::copy_if(grid.dates().begin(), grid.dates().end(), std::back_inserter(effExpiries),
                 [&maturityDate](const Date& d) { return d < maturityDate; });
    effMaturities.resize(effExpiries.size(), maturityDate);
    effStrikes.resize(effExpiries.size(), Null<Real>());

    if (calibrationStrategy == CalibrationStrategy::CoterminalATM ||
        calibrationStrategy == CalibrationStrategy::CoterminalDealStrike) {
        DLOG("Build LgmData for co-terminal specification");

        vector<string> expiryDates, termDates;
        for (Size i = 0; i < effExpiries.size(); ++i) {
            expiryDates.push_back(to_string(effExpiries[i]));
            termDates.push_back(to_string(effMaturities[i]));
        }
        data->optionExpiries() = expiryDates;
        data->optionTerms() = termDates;
        data->optionStrikes().resize(expiryDates.size(), "ATM");
        if (calibrationStrategy == CalibrationStrategy::CoterminalDealStrike ||
            calibrationStrategy == CalibrationStrategy::DeltaGammaAdjusted) {
            for (Size i = 0; i < effExpiries.size(); ++i) {
                if (effStrikes[i] != Null<Real>())
                    data->optionStrikes()[i] = std::to_string(effStrikes[i]);
            }
        }
        if (calibration == CalibrationType::Bootstrap) {
            DLOG("Calibrate piecewise alpha");
            data->calibrationType() = CalibrationType::Bootstrap;
            data->calibrateH() = false;
            data->hParamType() = ParamType::Constant;
            data->hValues() = {lambda};
            data->calibrateA() = true;
            data->aParamType() = ParamType::Piecewise;
            data->aValues() = {sigma};
        } else if (calibration == CalibrationType::BestFit) {
            DLOG("Calibrate constant sigma");
            data->calibrationType() = CalibrationType::BestFit;
            data->calibrateH() = false;
            data->hParamType() = ParamType::Constant;
            data->hValues() = {lambda};
            data->calibrateA() = true;
            data->aParamType() = ParamType::Constant;
            data->aValues() = {sigma};
        } else
            QL_FAIL("choice of calibration type invalid");
    }

    // Build and calibrate model
    DLOG("Build LGM model");

    auto rt = globalParameters_.find("RunType");
    bool allowChangingFallbacks =
        rt != globalParameters_.end() && rt->second != "SensitivityDelta" && rt->second != "SensitivityDeltaGamma";

    QuantLib::ext::shared_ptr<LgmBuilder> calib = QuantLib::ext::make_shared<LgmBuilder>(
        market_, data, configuration(MarketContext::irCalibration), tolerance, continueOnCalibrationError,
        referenceCalibrationGrid, generateAdditionalResults, id, BlackCalibrationHelper::RelativePriceError,
        allowChangingFallbacks, allowModelFallbacks,
        globalParameters_.count("Calibrate") != 0 && !parseBool(globalParameters_.at("Calibrate")));

    engineFactory()->modelBuilders().insert(std::make_pair(id, calib));

    return calib->model();
}

Handle<QuantExt::CrossAssetModel> CallableBondCamEngineBuilder::model(const std::string& id, const std::string& ccy,
                                                                        const std::string& creditCurveId,
                                                                        const QuantLib::Date& maturityDate,
                                                                        const bool generateAdditionalResults) {
    auto calibration = parseCalibrationType(modelParameter("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(modelParameter("CalibrationStrategy"));
    std::string referenceCalibrationGrid = modelParameter("ReferenceCalibrationGrid", {}, true, "");

    Real lambda = parseReal(modelParameter("Reversion", {ccy}));
    vector<Real> sigma = parseListOfValues<Real>(modelParameter("Volatility"), &parseReal);
    vector<Real> sigmaTimes = parseListOfValues<Real>(modelParameter("VolatilityTimes", {}, false), &parseReal);
    QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1, "there must be n+1 volatilities (" << sigma.size()
                                                                                         << ") for n volatility times ("
                                                                                         << sigmaTimes.size() << ")");
    //Real tolerance = parseReal(modelParameter("Tolerance"));
    auto reversionType = parseReversionType(modelParameter("ReversionType"));
    auto volatilityType = parseVolatilityType(modelParameter("VolatilityType"));
    bool continueOnCalibrationError = globalParameters_.count("ContinueOnCalibrationError") > 0 &&
                                      parseBool(globalParameters_.at("ContinueOnCalibrationError"));

    bool allowModelFallbacks =
        globalParameters_.count("AllowModelFallbacks") > 0 && parseBool(globalParameters_.at("AllowModelFallbacks"));

    auto floatSpreadMapping = parseFloatSpreadMapping(modelParameter("FloatSpreadMapping", {}, false, "proRata"));

    auto data = boost::make_shared<IrLgmData>();

    // check for allowed calibration / bermudan strategy settings
    std::vector<std::pair<CalibrationType, CalibrationStrategy>> validCalPairs = {
        {CalibrationType::None, CalibrationStrategy::None},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalATM},
        {CalibrationType::BestFit, CalibrationStrategy::CoterminalATM}};

    QL_REQUIRE(std::find(validCalPairs.begin(), validCalPairs.end(),
                         std::make_pair(calibration, calibrationStrategy)) != validCalPairs.end(),
               "Calibration (" << calibration << ") and CalibrationStrategy (" << calibrationStrategy
                               << ") are not allowed in this combination");

    // compute horizon shift
    Real shiftHorizon = parseReal(modelParameter("ShiftHorizon", {}, false, "0.5"));
    Date today = Settings::instance().evaluationDate();
    shiftHorizon = ActualActual(ActualActual::ISDA).yearFraction(today, maturityDate) * shiftHorizon;

    // Default: no calibration, constant lambda and sigma from engine configuration
    data->reset();
    data->qualifier() = ccy;
    data->calibrateH() = false;
    data->hParamType() = ParamType::Constant;
    data->hValues() = {lambda};
    data->reversionType() = reversionType;
    data->calibrateA() = false;
    data->aParamType() = ParamType::Piecewise;
    data->aValues() = sigma;
    data->aTimes() = sigmaTimes;
    data->volatilityType() = volatilityType;
    data->calibrationType() = calibration;
    data->shiftHorizon() = shiftHorizon;
    data->floatSpreadMapping() = floatSpreadMapping;

    std::vector<Date> effExpiries;
    std::vector<Real> effStrikes;
    std::vector<Date> effMaturities;

    DateGrid grid(referenceCalibrationGrid);
    std::copy_if(grid.dates().begin(), grid.dates().end(), std::back_inserter(effExpiries),
                 [&maturityDate](const Date& d) { return d < maturityDate; });
    effMaturities.resize(effExpiries.size(), maturityDate);
    effStrikes.resize(effExpiries.size(), Null<Real>());

    if (calibrationStrategy == CalibrationStrategy::CoterminalATM ||
        calibrationStrategy == CalibrationStrategy::CoterminalDealStrike) {
        DLOG("Build LgmData for co-terminal specification");

        vector<string> expiryDates, termDates;
        for (Size i = 0; i < effExpiries.size(); ++i) {
            expiryDates.push_back(to_string(effExpiries[i]));
            termDates.push_back(to_string(effMaturities[i]));
        }
        data->optionExpiries() = expiryDates;
        data->optionTerms() = termDates;
        data->optionStrikes().resize(expiryDates.size(), "ATM");
        if (calibrationStrategy == CalibrationStrategy::CoterminalDealStrike ||
            calibrationStrategy == CalibrationStrategy::DeltaGammaAdjusted) {
            for (Size i = 0; i < effExpiries.size(); ++i) {
                if (effStrikes[i] != Null<Real>())
                    data->optionStrikes()[i] = std::to_string(effStrikes[i]);
            }
        }
        if (calibration == CalibrationType::Bootstrap) {
            DLOG("Calibrate piecewise alpha");
            data->calibrationType() = CalibrationType::Bootstrap;
            data->calibrateH() = false;
            data->hParamType() = ParamType::Constant;
            data->hValues() = {lambda};
            data->calibrateA() = true;
            data->aParamType() = ParamType::Piecewise;
            data->aValues() = {sigma};
        } else if (calibration == CalibrationType::BestFit) {
            DLOG("Calibrate constant sigma");
            data->calibrationType() = CalibrationType::BestFit;
            data->calibrateH() = false;
            data->hParamType() = ParamType::Constant;
            data->hValues() = {lambda};
            data->calibrateA() = true;
            data->aParamType() = ParamType::Constant;
            data->aValues() = {sigma};
        } else
            QL_FAIL("choice of calibration type invalid");
    }
    std::vector<QuantLib::ext::shared_ptr<IrModelData>> irConfigs = {data};

    std::vector<QuantLib::ext::shared_ptr<CrLgmData>> crLgmConfigs = {};
    if (dynamicCreditModelEnabled()) {

        Real creditLambda = parseReal(modelParameter("Credit_Reversion", {ccy}));
        vector<Real> creditSigma = parseListOfValues<Real>(modelParameter("Credit_Volatility"), &parseReal);
        vector<Real> creditSigmaTimes = parseListOfValues<Real>(modelParameter("Credit_VolatilityTimes", {}, false), &parseReal);
        QL_REQUIRE(creditSigma.size() == creditSigmaTimes.size() + 1, "there must be n+1 volatilities (" << creditSigma.size()
                                                                                         << ") for n volatility times ("
                                                                                         << creditSigmaTimes.size() << ")");
        auto creditLgmData = ext::make_shared<CrLgmData>();
        creditLgmData->reset();
        creditLgmData->name() = creditCurveId;
        creditLgmData->ccy() = ccy;
        creditLgmData->calibrationType() = CalibrationType::None;
        creditLgmData->hParamType() = ParamType::Constant;
        creditLgmData->hValues() = {creditLambda};
        creditLgmData->calibrateH() = false;
        creditLgmData->aParamType() = ParamType::Constant;
        creditLgmData->calibrateA() = false;
        creditLgmData->aValues() = creditSigma;
        creditLgmData->aTimes() = creditSigmaTimes;
        crLgmConfigs.push_back(creditLgmData);
    }
    
    
    //! Vector of FX model specifications
    std::vector<QuantLib::ext::shared_ptr<FxBsData>> fxConfigs;
    //! Vector of EQ model specifications
    std::vector<QuantLib::ext::shared_ptr<EqBsData>> eqConfigs;
    //! Vector of INF model specifications
    std::vector<QuantLib::ext::shared_ptr<InflationModelData>> infConfigs;
    //! Vector of CR LGM model specifications
    
    //! Vector of CR CIR model specifications
    std::vector<QuantLib::ext::shared_ptr<CrCirData>> crCirConfigs = {};
    //! Vector of COM Schwartz model specifications
    std::vector<QuantLib::ext::shared_ptr<CommoditySchwartzData>> comConfigs = {};
    std::map<CorrelationKey, QuantLib::Handle<QuantLib::Quote>> corr;
    CorrelationFactor ir = parseCorrelationFactor("IR:" + ccy);
    CorrelationFactor cr = parseCorrelationFactor("CR:" + creditCurveId);
    //corr[std::make_pair(ir, cr)] = Handle<Quote>(ext::make_shared<SimpleQuote>(0.0));

    auto camModelData = ext::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, eqConfigs, infConfigs, crLgmConfigs,
                                                              crCirConfigs, comConfigs, 0, corr);

    auto rt = globalParameters_.find("RunType");
    bool allowChangingFallbacks =
        rt != globalParameters_.end() && rt->second != "SensitivityDelta" && rt->second != "SensitivityDeltaGamma";

    QuantLib::ext::shared_ptr<CrossAssetModelBuilder> calib = QuantLib::ext::make_shared<CrossAssetModelBuilder>(
        market_, camModelData, Market::defaultConfiguration, Market::defaultConfiguration, Market::defaultConfiguration, Market::defaultConfiguration
    ,Market::defaultConfiguration,Market::defaultConfiguration, false, continueOnCalibrationError, "", id, allowChangingFallbacks, allowModelFallbacks);

    engineFactory()->modelBuilders().insert(std::make_pair(id, calib));

    return calib->model();
}

template <class... Args>
QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
CallableBondLgmEngineBuilder::makeEngine(const std::string& id, const std::string& ccy,
                                         const std::string& creditCurveId, const std::string& securityId,
                                         const std::string& referenceCurveId, const std::string& incomeCurveId,
                                         const QuantLib::Date& maturityDate, Args... args) {

    std::string marketConfig = configuration(ore::data::MarketContext::pricing);

    // get reference curve, default curve, recovery and spread

    Handle<YieldTermStructure> referenceCurve = market_->yieldCurve(referenceCurveId, marketConfig);

    Handle<DefaultProbabilityTermStructure> defaultCurve;
    if (!creditCurveId.empty())
        defaultCurve = securitySpecificCreditCurve(market_, securityId, creditCurveId, marketConfig)->curve();

    Handle<YieldTermStructure> incomeCurve = referenceCurve;
    if (!incomeCurveId.empty())
        incomeCurve = market_->yieldCurve(incomeCurveId, marketConfig);

    Handle<Quote> recovery;
    try {
        recovery = market_->recoveryRate(securityId, marketConfig);
    } catch (...) {
        WLOG("security specific recovery rate not found for security ID "
             << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
        if (!creditCurveId.empty())
            recovery = market_->recoveryRate(creditCurveId, marketConfig);
    }

    Handle<Quote> spread;
    try {
        spread = market_->securitySpread(securityId, marketConfig);
    } catch (...) {
    }

    // determine whether add results should be generated

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    // get LGM model

    auto lgm = model(id, ccy, maturityDate, generateAdditionalResults);

    // return engine

    Size americanExerciseTimeStepsPerYear = parseInteger(modelParameter("ExerciseTimeStepsPerYear", {}, false, "0"));

    return QuantLib::ext::make_shared<QuantExt::NumericLgmCallableBondEngine>(
        Handle<QuantExt::LGM>(QuantLib::ext::dynamic_pointer_cast<QuantExt::LGM>(lgm)), args...,
        americanExerciseTimeStepsPerYear, referenceCurve, spread, defaultCurve, incomeCurve, recovery,
        parseBool(engineParameter("SpreadOnIncomeCurve", {}, false, "true")), generateAdditionalResults);
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine> CallableBondLgmFdEngineBuilder::engineImpl(
    const std::string& id, const std::string& ccy, const std::string& creditCurveId, const std::string& securityId,
    const std::string& referenceCurveId, const std::string& incomeCurveId, const QuantLib::Date& maturityDate) {

    QuantLib::FdmSchemeDesc scheme = parseFdmSchemeDesc(engineParameter("Scheme"));
    Size stateGridPoints = parseInteger(engineParameter("StateGridPoints"));
    Size timeStepsPerYear = parseInteger(engineParameter("TimeStepsPerYear"));
    Real mesherEpsilon = parseReal(engineParameter("MesherEpsilon"));

    std::string marketConfig = configuration(ore::data::MarketContext::pricing);
    Real maxTime = market_->yieldCurve(referenceCurveId, marketConfig)->timeFromReference(maturityDate);

    return makeEngine(id, ccy, creditCurveId, securityId, referenceCurveId, incomeCurveId, maturityDate, maxTime,
                      scheme, stateGridPoints, timeStepsPerYear, mesherEpsilon);
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine> CallableBondLgmGridEngineBuilder::engineImpl(
    const std::string& id, const std::string& ccy, const std::string& creditCurveId, const std::string& securityId,
    const std::string& referenceCurveId, const std::string& incomeCurveId, const QuantLib::Date& maturityDate) {

    Real sy = parseReal(engineParameter("sy"));
    Size ny = parseInteger(engineParameter("ny"));
    Real sx = parseReal(engineParameter("sx"));
    Size nx = parseInteger(engineParameter("nx"));

    return makeEngine(id, ccy, creditCurveId, securityId, referenceCurveId, incomeCurveId, maturityDate, sy, ny, sx,
                      nx);
}

QuantLib::ext::shared_ptr<QuantExt::PricingEngine> CallableBondCamMcEngineBuilder::engineImpl(
    const std::string& id, const std::string& ccy, const std::string& creditCurveId, const std::string& securityId,
    const std::string& referenceCurveId, const std::string& incomeCurveId, const QuantLib::Date& maturityDate) {

    std::string marketConfig = configuration(ore::data::MarketContext::pricing);

    // get reference curve, default curve, recovery and spread

    Handle<YieldTermStructure> referenceCurve = market_->yieldCurve(referenceCurveId, marketConfig);

    Handle<DefaultProbabilityTermStructure> defaultCurve;
    if (!creditCurveId.empty())
        defaultCurve = securitySpecificCreditCurve(market_, securityId, creditCurveId, marketConfig)->curve();

    Handle<YieldTermStructure> incomeCurve = referenceCurve;
    if (!incomeCurveId.empty())
        incomeCurve = market_->yieldCurve(incomeCurveId, marketConfig);

    Handle<Quote> recovery;
    try {
        recovery = market_->recoveryRate(securityId, marketConfig);
    } catch (...) {
        WLOG("security specific recovery rate not found for security ID "
             << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
        if (!creditCurveId.empty())
            recovery = market_->recoveryRate(creditCurveId, marketConfig);
    }

    Handle<Quote> discountingSpread;
    try {
        discountingSpread = market_->securitySpread(securityId, marketConfig);
    } catch (...) {
    }

    // determine whether add results should be generated

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    // get LGM model

    auto cam = model(id, ccy, creditCurveId, maturityDate, generateAdditionalResults);
    bool spreadOnIncome = parseBool(engineParameter("SpreadOnIncomeCurve", {}, false, "true"));
    
    // return engine
    std::vector<Date> simulationDates;      // You need to define or obtain simulationDates appropriately
    std::vector<Date> stickyCloseOutDates;  // You need to define or obtain stickyCloseOutDates appropriately
    std::vector<Size> externalModelIndices; // You need to define or obtain externalModelIndices appropriately

    Size americanExerciseTimeStepsPerYear = parseInteger(modelParameter("ExerciseTimeStepsPerYear", {}, false, "0"));

    return ext::make_shared<QuantExt::McCamCallableBondEngine>(
        cam, parseSequenceType(engineParameter("Training.Sequence", {}, false, "SobolBrownianBridge")),
        parseSequenceType(engineParameter("Pricing.Sequence", {}, false, "SobolBrownianBridge")),
        parseInteger(engineParameter("Training.Samples", {}, true, std::string())),
        parseInteger(engineParameter("Pricing.Samples", {}, false, "0")),
        parseInteger(engineParameter("Training.Seed", {}, true, std::string())),
        parseInteger(engineParameter("Pricing.Seed", {}, false, "42")),
        parseInteger(engineParameter("Training.BasisFunctionOrder", {}, true, std::string())),
        parsePolynomType(engineParameter("Training.BasisFunction", {}, true, std::string())),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering", {}, false, "Steps")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers", {}, false, "JoeKuoD7")),
        referenceCurve, discountingSpread, defaultCurve, incomeCurve, recovery, spreadOnIncome,
        americanExerciseTimeStepsPerYear,
        generateAdditionalResults, simulationDates, stickyCloseOutDates, externalModelIndices,
        parseBool(engineParameter("MinObsDate", {}, false, "true")),
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

QuantLib::ext::shared_ptr<QuantExt::PricingEngine> CallableBondCamAmcEngineBuilder::engineImpl(
    const std::string& id, const std::string& ccy, const std::string& creditCurveId, const std::string& securityId,
    const std::string& referenceCurveId, const std::string& incomeCurveId, const QuantLib::Date& maturityDate) {
    std::string marketConfig = configuration(ore::data::MarketContext::pricing);

    // get reference curve, default curve, recovery and spread

    Handle<YieldTermStructure> referenceCurve = market_->yieldCurve(referenceCurveId, marketConfig);

    Handle<DefaultProbabilityTermStructure> defaultCurve;
    if (!creditCurveId.empty())
        defaultCurve = securitySpecificCreditCurve(market_, securityId, creditCurveId, marketConfig)->curve();

    Handle<YieldTermStructure> incomeCurve = referenceCurve;
    if (!incomeCurveId.empty())
        incomeCurve = market_->yieldCurve(incomeCurveId, marketConfig);

    Handle<Quote> recovery;
    try {
        recovery = market_->recoveryRate(securityId, marketConfig);
    } catch (...) {
        WLOG("security specific recovery rate not found for security ID "
             << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
        if (!creditCurveId.empty())
            recovery = market_->recoveryRate(creditCurveId, marketConfig);
    }

    Handle<Quote> discountingSpread;
    try {
        discountingSpread = market_->securitySpread(securityId, marketConfig);
    } catch (...) {
    }

    // determine whether add results should be generated

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }
    bool spreadOnIncome = parseBool(engineParameter("SpreadOnIncomeCurve", {}, false, "true"));
    Size americanExerciseTimeStepsPerYear = parseInteger(modelParameter("ExerciseTimeStepsPerYear", {}, false, "0"));
    // get LGM model
    std::vector<Size> externalModelIndices;
    Currency cur = parseCurrency(ccy);

    std::set<std::pair<QuantExt::CrossAssetModel::AssetType, Size>> selectedModelComponents = {
        std::make_pair(QuantExt::CrossAssetModel::AssetType::IR, cam_->ccyIndex(cur))};

    if (dynamicCreditModelEnabled()) {
        selectedModelComponents.insert(
            std::make_pair(QuantExt::CrossAssetModel::AssetType::CR, cam_->crName(creditCurveId)));
    }

    QuantLib::Handle<QuantExt::CrossAssetModel> model(
        getProjectedCrossAssetModel(cam_, selectedModelComponents, externalModelIndices));
    return ext::make_shared<QuantExt::McCamCallableBondEngine>(
        model, parseSequenceType(engineParameter("Training.Sequence", {}, false, "SobolBrownianBridge")),
        parseSequenceType(engineParameter("Pricing.Sequence", {}, false, "SobolBrownianBridge")),
        parseInteger(engineParameter("Training.Samples", {}, true, std::string())),
        parseInteger(engineParameter("Pricing.Samples", {}, false, "0")),
        parseInteger(engineParameter("Training.Seed", {}, true, std::string())),
        parseInteger(engineParameter("Pricing.Seed", {}, false, "42")),
        parseInteger(engineParameter("Training.BasisFunctionOrder", {}, true, std::string())),
        parsePolynomType(engineParameter("Training.BasisFunction", {}, true, std::string())),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering", {}, false, "Steps")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers", {}, false, "JoeKuoD7")),
        referenceCurve, discountingSpread, defaultCurve, incomeCurve, recovery, spreadOnIncome,
        americanExerciseTimeStepsPerYear,
        generateAdditionalResults, simulationDates_, stickyCloseOutDates_, externalModelIndices,
        parseBool(engineParameter("MinObsDate", {}, false, "true")),
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

} // namespace data
} // namespace ore
