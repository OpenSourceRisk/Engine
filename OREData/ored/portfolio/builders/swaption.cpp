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

#include <ored/model/camcorrelationsfromindexcorrelations.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/model/lgmbuilder.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/scripting/engines/amccgmultilegoptionengine.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/dependencies.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/projectedcrossassetmodel.hpp>
#include <qle/pricingengines/blackmultilegoptionengine.hpp>
#include <qle/pricingengines/mcmultilegoptionengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>

#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <set>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace {

QuantLib::ext::shared_ptr<PricingEngine>
buildMcEngine(const Handle<CrossAssetModel>& model, const std::vector<Handle<YieldTermStructure>>& discountCurves,
              const std::vector<Size>& externalModelIndices, const std::vector<QuantLib::Date>& simulationDates,
              const std::vector<QuantLib::Date>& stickyCloseOutDates, const EngineBuilder* builder) {
    return QuantLib::ext::make_shared<QuantExt::McMultiLegOptionEngine>(
        model, parseSequenceType(builder->engineParameter("Training.Sequence", {}, false, "SobolBrownianBridge")),
        parseSequenceType(builder->engineParameter("Pricing.Sequence", {}, false, "SobolBrownianBridge")),
        parseInteger(builder->engineParameter("Training.Samples", {}, true, std::string())),
        parseInteger(builder->engineParameter("Pricing.Samples", {}, false, "0")),
        parseInteger(builder->engineParameter("Training.Seed", {}, true, std::string())),
        parseInteger(builder->engineParameter("Pricing.Seed", {}, false, "42")),
        parseInteger(builder->engineParameter("Training.BasisFunctionOrder", {}, true, std::string())),
        parsePolynomType(builder->engineParameter("Training.BasisFunction", {}, true, std::string())),
        parseSobolBrownianGeneratorOrdering(builder->engineParameter("BrownianBridgeOrdering", {}, false, "Steps")),
        parseSobolRsgDirectionIntegers(builder->engineParameter("SobolDirectionIntegers", {}, false, "JoeKuoD7")),
        discountCurves, simulationDates, stickyCloseOutDates, externalModelIndices,
        parseBool(builder->engineParameter("MinObsDate", {}, false, "true")),
        parseRegressorModel(builder->engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(builder->engineParameter("RegressionVarianceCutoff", {}, false, std::string())),
        parseBool(builder->engineParameter("RecalibrateOnStickyCloseOutDates", {}, false, "false")),
        parseBool(builder->engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")),
        parseInteger(builder->engineParameter("CashflowGeneration.OnCpnMaxSimTimes", {}, false, "1")),
        parsePeriod(builder->engineParameter("CashflowGeneration.OnCpnAddSimTimesCutoff", {}, false, "0D")),
        parseInteger(builder->engineParameter("Regression.MaxSimTimesIR", {}, false, "0")),
        parseInteger(builder->engineParameter("Regression.MaxSimTimesFX", {}, false, "0")),
        parseInteger(builder->engineParameter("Regression.MaxSimTimesEQ", {}, false, "0")),
        parseVarGroupMode(builder->engineParameter("Regression.VarGroupMode", {}, false, "Global")));
}

std::variant<Handle<CrossAssetModel>, ext::shared_ptr<QuantExt::LGM>>
model(const string& id, const std::vector<string>& keys, const std::vector<Date>& expiries,
      const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
      const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican, const EngineBuilder* builder) {

    auto calibration = parseCalibrationType(builder->modelParameter("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(builder->modelParameter("CalibrationStrategy"));
    bool continueOnCalibrationError = builder->globalParameters().count("ContinueOnCalibrationError") > 0 &&
                                      parseBool(builder->globalParameters().at("ContinueOnCalibrationError"));

    auto fxCalibration = parseCalibrationType(builder->modelParameter("FxCalibration", {}, false, "Bootstrap"));
    QL_REQUIRE(fxCalibration == CalibrationType::None || fxCalibration == CalibrationType::Bootstrap,
               "Unsupported FxCalibration (" << fxCalibration << ")");

    Real tolerance = parseReal(builder->modelParameter("Tolerance"));
    auto reversionType = parseReversionType(builder->modelParameter("ReversionType"));
    auto volatilityType = parseVolatilityType(builder->modelParameter("VolatilityType"));
    auto floatSpreadMapping =
        parseFloatSpreadMapping(builder->modelParameter("FloatSpreadMapping", {}, false, "proRata"));
    bool allowModelFallbacks = builder->globalParameters().count("AllowModelFallbacks") > 0 &&
                               parseBool(builder->globalParameters().at("AllowModelFallbacks"));

    // required for american options to set up calibration basket
    std::string referenceCalibrationGrid = builder->modelParameter("ReferenceCalibrationGrid", {}, isAmerican, "");

    // check for allowed calibration / bermudan strategy settings
    std::vector<std::pair<CalibrationType, CalibrationStrategy>> validCalPairs = {
        {CalibrationType::None, CalibrationStrategy::None},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalATM},
        {CalibrationType::Bootstrap, CalibrationStrategy::DeltaGammaAdjusted},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalDealStrike},
        {CalibrationType::BestFit, CalibrationStrategy::CoterminalATM},
        {CalibrationType::BestFit, CalibrationStrategy::CoterminalDealStrike}};

    QL_REQUIRE(std::find(validCalPairs.begin(), validCalPairs.end(),
                         std::make_pair(calibration, calibrationStrategy)) != validCalPairs.end(),
               "Calibration (" << calibration << ") and CalibrationStrategy (" << calibrationStrategy
                               << ") are not allowed in this combination");

    // compute horizon shift
    Real shiftHorizon = parseReal(builder->modelParameter("ShiftHorizon", {}, false, "0.5"));
    Date today = Settings::instance().evaluationDate();
    shiftHorizon = ActualActual(ActualActual::ISDA).yearFraction(today, maturities.back()) * shiftHorizon;

    // option expires, maturities (ir, fx)

    std::vector<Date> effExpiries;
    std::vector<Date> effMaturities;
    if (!isAmerican) {
        effExpiries = expiries;
        effMaturities = maturities;
    } else {
        QL_REQUIRE(maturities.size() == 2,
                   "LGMBermudanAmericanSwaptionEngineBuilder::model(): expected 2 expiries for exercise "
                   "style 'American', got "
                       << expiries.size() << " expiries");
        // keep one calibration instrument per reference grid interval
        DateGrid grid(referenceCalibrationGrid);
        std::copy_if(grid.dates().begin(), grid.dates().end(), std::back_inserter(effExpiries),
                     [&expiries](const Date& d) { return d >= expiries[0] && d < expiries[1]; });

        effMaturities.resize(effExpiries.size(), maturities.back());
    }

    vector<string> expiryDates, termDates;
    for (Size i = 0; i < effExpiries.size(); ++i) {
        expiryDates.push_back(to_string(effExpiries[i]));
        termDates.push_back(to_string(effMaturities[i]));
    }

    // index names (ir, fx) that are later used to build correlation matrix

    std::set<std::string> correlationIndices;

    // loop over currencies and set up ir model data

    std::vector<ext::shared_ptr<IrModelData>> irData;
    std::vector<std::string> currencies;

    for (Size i = 0; i < keys.size(); ++i) {

        std::string key = keys[i];
        QuantLib::ext::shared_ptr<IborIndex> index;
        std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
        currencies.push_back(ccy);

        if (tryParseIborIndex(key, index))
            correlationIndices.insert(key);
        else
            correlationIndices.insert(swapIndexDiscountCurve(key, std::string(), std::string()));

        Real lambda = parseReal(builder->modelParameter("Reversion", {key, ccy}));
        vector<Real> sigma = parseListOfValues<Real>(builder->modelParameter("Volatility", {key, ccy}), &parseReal);
        vector<Real> sigmaTimes =
            parseListOfValues<Real>(builder->modelParameter("VolatilityTimes", {key, ccy}, false), &parseReal);
        QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1, "there must be n+1 volatilities ("
                                                              << sigma.size() << ") for n volatility times ("
                                                              << sigmaTimes.size() << ")");

        auto data = QuantLib::ext::make_shared<IrLgmData>();
        // Default: no calibration, constant lambda and sigma from engine configuration
        data->reset();
        data->qualifier() = key;
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

        std::vector<Real> effStrikes;

        if (!isAmerican) {
            effStrikes = strikes[i];
        } else {
            effStrikes.resize(effExpiries.size(), Null<Real>());
            if (strikes[i][0] != Null<Real>() && strikes[i][1] != Null<Real>()) {
                Real t0 = Actual365Fixed().yearFraction(today, expiries[0]);
                Real t1 = Actual365Fixed().yearFraction(today, expiries[1]);
                for (Size i = 0; i < effExpiries.size(); ++i) {
                    Real t = Actual365Fixed().yearFraction(today, effExpiries[i]);
                    effStrikes[i] = strikes[i][0] + (strikes[i][1] - strikes[i][0]) / (t1 - t0) * (t - t0);
                }
            }
        }

        if (calibrationStrategy == CalibrationStrategy::CoterminalATM ||
            calibrationStrategy == CalibrationStrategy::CoterminalDealStrike ||
            calibrationStrategy == CalibrationStrategy::DeltaGammaAdjusted) {

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

        irData.push_back(data);

    } // loop over currencies

    // set up fx model data

    std::vector<QuantLib::ext::shared_ptr<FxData>> fxData;

    for (Size i = 1; i < currencies.size(); ++i) {
        string ccyPair = currencies[i] + currencies.front();
        auto bsData = QuantLib::ext::make_shared<FxBsData>();
        vector<Real> vols =
            parseListOfValues<Real>(builder->modelParameter("FxVolatility", {ccyPair}, true), &parseReal);
        vector<Real> volTimes =
            parseListOfValues<Real>(builder->modelParameter("FxVolatilityTimes", {ccyPair}, false), &parseReal);
        QL_REQUIRE(vols.size() == volTimes.size() + 1, "there must be n+1 FX volatilities ("
                                                           << vols.size() << ") for n volatility times ("
                                                           << volTimes.size() << "), for pair " << ccyPair);

        std::vector<Real> effStrikes;
        if (!isAmerican) {
            effStrikes = fxStrikes[i - 1];
        } else {
            effStrikes.resize(effExpiries.size(), Null<Real>());
            if (fxStrikes[i][0] != Null<Real>() && fxStrikes[i][1] != Null<Real>()) {
                Real t0 = Actual365Fixed().yearFraction(today, expiries[0]);
                Real t1 = Actual365Fixed().yearFraction(today, expiries[1]);
                for (Size i = 0; i < effExpiries.size(); ++i) {
                    Real t = Actual365Fixed().yearFraction(today, effExpiries[i]);
                    effStrikes[i] = fxStrikes[i][0] + (fxStrikes[i][1] - fxStrikes[i][0]) / (t1 - t0) * (t - t0);
                }
            }
        }

        bsData->setDomesticCcy(currencies.front());
        bsData->setForeignCcy(currencies[i]);
        bsData->setCalibrationType(fxCalibration);
        bsData->setCalibrateSigma(fxCalibration == CalibrationType::Bootstrap);
        bsData->setSigmaParamType(ParamType::Piecewise);
        bsData->setSigmaTimes(volTimes);
        bsData->setSigmaValues(vols);
        bsData->setOptionExpiries(expiryDates);
        std::vector<std::string> strikes(expiryDates.size(), "ATMF");
        for (Size i = 0; i < effExpiries.size(); ++i) {
            if (effStrikes[i] != Null<Real>())
                strikes[i] = std::to_string(effStrikes[i]);
        }
        bsData->setOptionStrikes(strikes);

        fxData.push_back(bsData);

        correlationIndices.insert("FX-GENERIC-" + currencies[i] + "-" + currencies[0]);

    } // loop over currency pairs

    // correlations

    std::map<std::pair<std::string, std::string>, Handle<QuantExt::CorrelationTermStructure>> indexCorrelations;
    for (Size i = 0; i < correlationIndices.size(); ++i) {
        for (Size j = 0; j < i; ++j) {
            std::string p1 = *std::next(correlationIndices.begin(), i);
            std::string p2 = *std::next(correlationIndices.begin(), j);
            indexCorrelations[std::make_pair(p1, p2)] =
                builder->market()->correlationCurve(p1, p2, builder->configuration(MarketContext::pricing));
        }
    }

    auto camCorr = getCamCorrelationsFromIndexCorrelations(indexCorrelations);

    // set some flags

    bool generateAdditionalResults = false;
    if (auto p = builder->globalParameters().find("GenerateAdditionalResults");
        p != builder->globalParameters().end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    auto rt = builder->globalParameters().find("RunType");
    bool allowChangingFallbacks = rt != builder->globalParameters().end() && rt->second != "SensitivityDelta" &&
                                  rt->second != "SensitivityDeltaGamma";

    auto dc = builder->globalParameters().find("Calibrate");
    bool dontCalibrate = dc != builder->globalParameters().end() && !parseBool(dc->second);

    // build model

    if (irData.size() == 1) {

        auto calib = ext::make_shared<LgmBuilder>(
            builder->market(), ext::dynamic_pointer_cast<IrLgmData>(irData.front()),
            builder->configuration(MarketContext::irCalibration), tolerance, continueOnCalibrationError,
            referenceCalibrationGrid, generateAdditionalResults, id, BlackCalibrationHelper::RelativePriceError,
            allowChangingFallbacks, allowModelFallbacks, dontCalibrate);

        builder->engineFactory()->modelBuilders().insert(std::make_pair(id, calib));
        return ext::dynamic_pointer_cast<LGM>(calib->model());

    } else {

        std::string configurationInCcy = builder->configuration(MarketContext::irCalibration);
        std::string configurationXois = builder->configuration(MarketContext::pricing);

        auto calib = ext::make_shared<CrossAssetModelBuilder>(
            builder->market(),
            ext::make_shared<CrossAssetModelData>(irData, fxData, camCorr, tolerance, "LGM",
                                                  CrossAssetModel::Discretization::Exact,
                                                  QuantLib::SalvagingAlgorithm::Spectral),
            configurationInCcy, configurationXois, configurationXois, configurationInCcy, configurationInCcy,
            configurationXois, dontCalibrate, continueOnCalibrationError, "", id, allowChangingFallbacks,
            allowModelFallbacks);

        builder->engineFactory()->modelBuilders().insert(std::make_pair(id, calib));
        return calib->model();
    }

} // model()

} // namespace

namespace ore {
namespace data {

string SwaptionEngineBuilder::keyImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
                                      const std::vector<Date>& maturities,
                                      const std::vector<std::vector<Real>>& strikes,
                                      const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican,
                                      const std::string& discountCurve, const std::string& securitySpread) {
    if (idBasedKey_)
        return id;
    return boost::algorithm::join(keys, "_") + "_" + (isAmerican ? "1" : "0") + "_" + discountCurve + "_" +
           securitySpread;
}

QuantLib::ext::shared_ptr<PricingEngine>
EuropeanSwaptionEngineBuilder::engineImpl(const string& id, const std::vector<string>& keys,
                                          const std::vector<Date>& dates, const std::vector<Date>& maturities,
                                          const std::vector<std::vector<Real>>& strikes,
                                          const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican,
                                          const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building Black Swaption engine for trade " << id);
    QL_REQUIRE(keys.size() == 1, "EuropeanSwaptionEngineBuilder::engingImpl(): multiple ccys are not supported.");
    QuantLib::ext::shared_ptr<IborIndex> index;
    string ccyCode = tryParseIborIndex(keys.front(), index) ? index->currency().code() : keys.front();
    Handle<YieldTermStructure> yts =
        discountCurve.empty() ? market_->discountCurve(ccyCode, configuration(MarketContext::pricing))
                              : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
    if (!securitySpread.empty())
        yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
            yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
    Handle<SwaptionVolatilityStructure> svts =
        market_->swaptionVol(keys.front(), configuration(MarketContext::pricing));
    return QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(yts, svts);
}

QuantLib::ext::shared_ptr<PricingEngine>
LGMGridSwaptionEngineBuilder::engineImpl(const string& id, const std::vector<string>& keys,
                                         const std::vector<Date>& dates, const std::vector<Date>& maturities,
                                         const std::vector<std::vector<Real>>& strikes,
                                         const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican,
                                         const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building LGM Grid Swaption engine for trade " << id);
    QL_REQUIRE(keys.size() == 1, "LGMGridSwaptionEngineBuilder::engingImpl(): multiple ccys are not supported.");

    auto lgm = std::get<ext::shared_ptr<LGM>>(::model(id, keys, dates, maturities, strikes, {}, isAmerican, this));

    DLOG("Get engine data");
    Real sy = parseReal(engineParameter("sy"));
    Size ny = parseInteger(engineParameter("ny"));
    Real sx = parseReal(engineParameter("sx"));
    Size nx = parseInteger(engineParameter("nx"));

    QuantLib::ext::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(keys.front(), index) ? index->currency().code() : keys.front();
    Handle<YieldTermStructure> yts =
        discountCurve.empty() ? market_->discountCurve(ccy, configuration(MarketContext::pricing))
                              : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
    if (!securitySpread.empty())
        yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
            yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));

    return QuantLib::ext::make_shared<QuantExt::NumericLgmMultiLegOptionEngine>(
        lgm, sy, ny, sx, nx, yts, isAmerican ? parseInteger(modelParameter("ExerciseTimeStepsPerYear")) : 0);
}

QuantLib::ext::shared_ptr<PricingEngine>
LGMFDSwaptionEngineBuilder::engineImpl(const string& id, const std::vector<string>& keys,
                                       const std::vector<Date>& dates, const std::vector<Date>& maturities,
                                       const std::vector<std::vector<Real>>& strikes,
                                       const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican,
                                       const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building LGM FD Swaption engine for trade " << id);
    QL_REQUIRE(keys.size() == 1, "LGMFDSwaptionEngineBuilder::engingImpl(): multiple ccys are not supported.");

    auto lgm = std::get<ext::shared_ptr<LGM>>(::model(id, keys, dates, maturities, strikes, {}, isAmerican, this));

    QuantLib::FdmSchemeDesc scheme = parseFdmSchemeDesc(engineParameter("Scheme"));
    Size stateGridPoints = parseInteger(engineParameter("StateGridPoints"));
    Size timeStepsPerYear = parseInteger(engineParameter("TimeStepsPerYear"));
    Real mesherEpsilon = parseReal(engineParameter("MesherEpsilon"));

    Real maxTime = lgm->termStructure()->timeFromReference(maturities.back());

    QuantLib::ext::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(keys.front(), index) ? index->currency().code() : keys.front();
    Handle<YieldTermStructure> yts =
        discountCurve.empty() ? market_->discountCurve(ccy, configuration(MarketContext::pricing))
                              : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
    if (!securitySpread.empty())
        yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
            yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
    return QuantLib::ext::make_shared<QuantExt::NumericLgmMultiLegOptionEngine>(
        lgm, maxTime, scheme, stateGridPoints, timeStepsPerYear, mesherEpsilon, yts,
        isAmerican ? parseInteger(modelParameter("ExerciseTimeStepsPerYear")) : 0);
}

QuantLib::ext::shared_ptr<PricingEngine>
CamMCSwaptionEngineBuilder::engineImpl(const string& id, const std::vector<string>& keys,
                                       const std::vector<Date>& dates, const std::vector<Date>& maturities,
                                       const std::vector<std::vector<Real>>& strikes,
                                       const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican,
                                       const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building CAM MC Swaption engine for trade " << id);

    auto cam =
        std::get<Handle<CrossAssetModel>>(::model(id, keys, dates, maturities, strikes, fxStrikes, isAmerican, this));

    std::vector<Handle<YieldTermStructure>> discountCurves;
    for (Size i = 0; i < keys.size(); ++i) {
        QuantLib::ext::shared_ptr<IborIndex> index;
        std::string ccy = tryParseIborIndex(keys[i], index) ? index->currency().code() : keys[i];
        Handle<YieldTermStructure> yts =
            discountCurve.empty() || i > 0
                ? market_->discountCurve(ccy, configuration(MarketContext::pricing))
                : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
        if (!securitySpread.empty())
            yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
                yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
        discountCurves.push_back(yts);
    }

    return buildMcEngine(cam, discountCurves, std::vector<Size>(), {}, {}, this);
} // LgmMc engineImpl()

QuantLib::ext::shared_ptr<PricingEngine>
AmcSwaptionEngineBuilder::engineImpl(const string& id, const std::vector<string>& keys, const std::vector<Date>& dates,
                                     const std::vector<Date>& maturities, const std::vector<std::vector<Real>>& strikes,
                                     const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican,
                                     const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building AMC Swaption engine for trade " << id << " from external CAM");

    std::vector<std::string> currencies;
    for (Size i = 0; i < keys.size(); ++i) {
        QuantLib::ext::shared_ptr<IborIndex> index;
        std::string ccy = tryParseIborIndex(keys[i], index) ? index->currency().code() : keys[i];
        currencies.push_back(ccy);
    }

    bool needBaseCcy = currencies.size() > 1;

    std::set<std::pair<CrossAssetModel::AssetType, Size>> selectedComponents;
    if (needBaseCcy) {
        selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::IR, 0));
    }
    for (auto const& c : currencies) {
        Size ccyIdx = cam_->ccyIndex(parseCurrency(c));
        if (ccyIdx != 0 || !needBaseCcy)
            selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::IR, ccyIdx));
        if (needBaseCcy && ccyIdx > 0)
            selectedComponents.insert(std::make_pair(CrossAssetModel::AssetType::FX, ccyIdx - 1));
    }
    std::vector<Size> externalModelIndices;
    Handle<CrossAssetModel> model(getProjectedCrossAssetModel(cam_, selectedComponents, externalModelIndices));

    return buildMcEngine(model, {}, externalModelIndices, simulationDates_, stickyCloseOutDates_, this);
} // Amc engineImpl

QuantLib::ext::shared_ptr<PricingEngine>
AmcCgSwaptionEngineBuilder::engineImpl(const string& id, const std::vector<string>& keys,
                                       const std::vector<Date>& dates, const std::vector<Date>& maturities,
                                       const std::vector<std::vector<Real>>& strikes,
                                       const std::vector<std::vector<Real>>& fxStrikes, const bool isAmerican,
                                       const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building AMC-CG Swaption engine for trade " << id << " from external CAM");
    QL_REQUIRE(keys.size() == 1, "AmcCgSwapptionEngineBuilder::engingImpl(): multiple ccys are not supported. TODO.");
    QL_REQUIRE(modelCg_ != nullptr, "AmcCgSwapEngineBuilder::engineImpl: modelcg is null");
    QuantLib::ext::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(keys.front(), index) ? index->currency().code() : keys.front();
    return QuantLib::ext::make_shared<AmcCgMultiLegOptionEngine>(
        std::vector<std::string>{ccy}, modelCg_, simulationDates_,
        parseBool(engineParameter("ReevaluateExerciseInStickyRun", {}, false, "false")));
} // AmcCg engineImpl

} // namespace data
} // namespace ore
