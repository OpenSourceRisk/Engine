/*
 Copyright (C) 2016-2022 Quaternion Risk Management Ltd
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
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/methods/montecarlo/lsmbasissystem.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/pricingengines/mcmultilegoptionengine.hpp>

#include <set>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using namespace ore::data;

namespace {
boost::shared_ptr<PricingEngine> buildMcEngine(const std::function<string(string)>& engineParameters,
                                               const boost::shared_ptr<LGM>& lgm,
                                               const Handle<YieldTermStructure>& discountCurve,
                                               const std::vector<Date>& simulationDates,
                                               const std::vector<Size>& externalModelIndices) {

    return boost::make_shared<QuantExt::McMultiLegOptionEngine>(
        lgm, parseSequenceType(engineParameters("Training.Sequence")),
        parseSequenceType(engineParameters("Pricing.Sequence")), parseInteger(engineParameters("Training.Samples")),
        parseInteger(engineParameters("Pricing.Samples")), parseInteger(engineParameters("Training.Seed")),
        parseInteger(engineParameters("Pricing.Seed")), parseInteger(engineParameters("Training.BasisFunctionOrder")),
        parsePolynomType(engineParameters("Training.BasisFunction")),
        parseSobolBrownianGeneratorOrdering(engineParameters("BrownianBridgeOrdering")),
        parseSobolRsgDirectionIntegers(engineParameters("SobolDirectionIntegers")), discountCurve, simulationDates,
        externalModelIndices, parseBool(engineParameters("MinObsDate")),
        parseBool(engineParameters("RegressionOnExerciseOnly")));
}
} // namespace

namespace ore {
namespace data {

boost::shared_ptr<PricingEngine> EuropeanSwaptionEngineBuilder::engineImpl(const string& key) {
    boost::shared_ptr<IborIndex> index;
    string ccyCode = tryParseIborIndex(key, index) ? index->currency().code() : key;
    Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
    Handle<SwaptionVolatilityStructure> svts = market_->swaptionVol(key, configuration(MarketContext::pricing));
    switch (svts->volatilityType()) {
    case ShiftedLognormal:
        LOG("Build BlackSwaptionEngine for currency " << ccyCode);
        return boost::make_shared<BlackSwaptionEngine>(yts, svts);
    case Normal:
        LOG("Build BachelierSwaptionEngine for currency " << ccyCode);
        return boost::make_shared<BachelierSwaptionEngine>(yts, svts);
    default:
        QL_FAIL("Swaption volatility type " << svts->volatilityType() << "not covered in EngineFactory");
        break;
    }
}

boost::shared_ptr<QuantExt::LGM> LGMBermudanSwaptionEngineBuilder::model(const string& id, const string& key,
                                                                         const std::vector<Date>& expiries,
                                                                         const Date& maturity,
                                                                         const std::vector<Real>& strikes) {
    boost::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;

    DLOG("Get model data");
    auto calibration = parseCalibrationType(modelParameter("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(modelParameter("CalibrationStrategy"));
    std::string referenceCalibrationGrid = modelParameter("ReferenceCalibrationGrid", {}, false, "");
    Real lambda = parseReal(modelParameter("Reversion", {key, ccy}));
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

    auto data = boost::make_shared<IrLgmData>();

    // check for allowed calibration / bermudan strategy settings
    std::vector<std::pair<CalibrationType, CalibrationStrategy>> validCalPairs = {
        {CalibrationType::None, CalibrationStrategy::None},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalATM},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalDealStrike},
        {CalibrationType::BestFit, CalibrationStrategy::CoterminalATM},
        {CalibrationType::BestFit, CalibrationStrategy::CoterminalDealStrike}};

    QL_REQUIRE(std::find(validCalPairs.begin(), validCalPairs.end(),
                         std::make_pair(calibration, calibrationStrategy)) != validCalPairs.end(),
               "Calibration (" << calibration << ") and CalibrationStrategy (" << calibrationStrategy
                               << ") are not allowed in this combination");

    // compute horizon shift
    Real shiftHorizon = parseReal(modelParameter("ShiftHorizon", {}, false, "0.5"));
    Date today = Settings::instance().evaluationDate();
    shiftHorizon = ActualActual(ActualActual::ISDA).yearFraction(today, maturity) * shiftHorizon;

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

    if (calibrationStrategy == CalibrationStrategy::CoterminalATM ||
        calibrationStrategy == CalibrationStrategy::CoterminalDealStrike) {
        DLOG("Build LgmData for co-terminal specification");
        vector<string> expiryDates, termDates;
        for (Size i = 0; i < expiries.size(); ++i) {
            expiryDates.push_back(to_string(expiries[i]));
            termDates.push_back(to_string(maturity));
        }
        data->optionExpiries() = expiryDates;
        data->optionTerms() = termDates;
        data->optionStrikes().resize(expiryDates.size(), "ATM");
        if (calibrationStrategy == CalibrationStrategy::CoterminalDealStrike) {
            for (Size i = 0; i < expiries.size(); ++i) {
                if (strikes[i] != Null<Real>())
                    data->optionStrikes()[i] = std::to_string(strikes[i]);
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

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    // Build and calibrate model
    DLOG("Build LGM model");
    boost::shared_ptr<LgmBuilder> calib =
        boost::make_shared<LgmBuilder>(market_, data, configuration(MarketContext::irCalibration), tolerance,
                                       continueOnCalibrationError, referenceCalibrationGrid, generateAdditionalResults);

    // In some cases, we do not want to calibrate the model
    boost::shared_ptr<QuantExt::LGM> model;
    if (globalParameters_.count("Calibrate") == 0 || parseBool(globalParameters_.at("Calibrate"))) {
        DLOG("Calibrate model (configuration " << configuration(MarketContext::irCalibration) << ")");
        model = calib->model();
    } else {
        DLOG("Skip calibration of model based on global parameters");
        calib->freeze();
        model = calib->model();
        calib->unfreeze();
    }
    modelBuilders_.insert(std::make_pair(id, calib));

    return model;
}

boost::shared_ptr<PricingEngine> LGMGridBermudanSwaptionEngineBuilder::engineImpl(const string& id, const string& key,
                                                                                  const std::vector<Date>& expiries,
                                                                                  const Date& maturity,
                                                                                  const std::vector<Real>& strikes) {
    DLOG("Building Bermudan Swaption engine for trade " << id);

    boost::shared_ptr<QuantExt::LGM> lgm = model(id, key, expiries, maturity, strikes);

    DLOG("Get engine data");
    Real sy = parseReal(engineParameter("sy"));
    Size ny = parseInteger(engineParameter("ny"));
    Real sx = parseReal(engineParameter("sx"));
    Size nx = parseInteger(engineParameter("nx"));

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    boost::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    return boost::make_shared<QuantExt::NumericLgmMultiLegOptionEngine>(
        lgm, sy, ny, sx, nx, market_->discountCurve(ccy, configuration(MarketContext::pricing)));
}

boost::shared_ptr<PricingEngine> LgmMcBermudanSwaptionEngineBuilder::engineImpl(const string& id, const string& key,
                                                                                const std::vector<Date>& expiries,
                                                                                const Date& maturity,
                                                                                const std::vector<Real>& strikes) {
    DLOG("Building MC Bermudan Swaption engine for trade " << id);

    auto lgm = model(id, key, expiries, maturity, strikes);

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    boost::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    auto discountCurve = market_->discountCurve(ccy, configuration(MarketContext::pricing));
    return buildMcEngine([this](const std::string& p) { return this->engineParameter(p); }, lgm, discountCurve,
                         std::vector<Date>(), std::vector<Size>());
} // LgmMc engineImpl()

boost::shared_ptr<PricingEngine> LgmAmcBermudanSwaptionEngineBuilder::engineImpl(const string& id, const string& key,
                                                                                 const std::vector<Date>& expiries,
                                                                                 const Date& maturity,
                                                                                 const std::vector<Real>& strikes) {
    boost::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    Currency curr = parseCurrency(ccy);
    DLOG("Building AMC Bermudan Swaption engine for key " << key << ", ccy " << ccy << " (from externally given CAM)");

    QL_REQUIRE(cam_ != nullptr, "LgmCamBermudanSwaptionEngineBuilder::engineImpl: cam is null");
    Size currIdx = cam_->ccyIndex(curr);
    auto lgm = cam_->lgm(currIdx);
    std::vector<Size> modelIndex(1, cam_->pIdx(CrossAssetModel::AssetType::IR, currIdx));

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    // we assume that the given cam has pricing discount curves attached already
    Handle<YieldTermStructure> discountCurve;
    return buildMcEngine([this](const std::string& p) { return this->engineParameter(p); }, lgm, discountCurve,
                         simulationDates_, modelIndex);
} // LgmCam engineImpl

} // namespace data
} // namespace ore
