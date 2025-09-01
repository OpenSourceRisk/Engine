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
#include <ored/portfolio/builders/callablebond.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/pricingengines/fdlgmcallablebondengine.hpp>

#include <ql/time/date.hpp>

#include <boost/make_shared.hpp>

namespace ore {
namespace data {

boost::shared_ptr<QuantExt::LGM> CallableBondLgmFdEngineBuilder::model(const std::string& id, const std::string& ccy,
                                                                       const std::string& securityId,
                                                                       const QuantLib::Date& maturity,
                                                                       const bool generateAdditionalResults) {

    auto useSwaptionVolatility = parseBool(modelParameter("UseSwaptionVolatility"));
    QL_REQUIRE(
        useSwaptionVolatility,
        "CallableBondLgmFdEngineBuilder::model(): UseSwaptionVolatility = false is not supported at the moment.");
    auto calibration = parseCalibrationType(modelParameter("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(modelParameter("CalibrationStrategy"));
    std::vector<std::string> calibrationGrid = parseListOfValues(modelParameter("CalibrationGrid", {}, false, ""));

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
    shiftHorizon = ActualActual(ActualActual::ISDA).yearFraction(today, maturity) * shiftHorizon;

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

    if (calibrationStrategy == CalibrationStrategy::CoterminalATM ||
        calibrationStrategy == CalibrationStrategy::CoterminalDealStrike) {
        DLOG("Build LgmData for co-terminal specification");
        data->optionExpiries() = calibrationGrid;
        data->optionTerms() = std::vector<std::string>(calibrationGrid.size(), ore::data::to_string(maturity));
        data->optionStrikes().resize(calibrationGrid.size(), "ATM");
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
    boost::shared_ptr<LgmBuilder> calib =
        boost::make_shared<LgmBuilder>(market_, data, configuration(MarketContext::irCalibration), tolerance,
                                       continueOnCalibrationError, std::string(), generateAdditionalResults);

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

boost::shared_ptr<QuantExt::PricingEngine> CallableBondLgmFdEngineBuilder::engineImpl(
    const std::string& id, const std::string& ccy, const std::string& creditCurveId, const bool hasCreditRisk,
    const std::string& securityId, const std::string& referenceCurveId, const QuantLib::Date& maturityDate) {

    std::string config = this->configuration(ore::data::MarketContext::pricing);

    // get reference curve, default curve, recovery and spread

    Handle<YieldTermStructure> referenceCurve = market_->yieldCurve(referenceCurveId, config);
    Handle<DefaultProbabilityTermStructure> defaultCurve;
    if (!creditCurveId.empty())
        defaultCurve = securitySpecificCreditCurve(market_, securityId, creditCurveId, config)->curve();
    Handle<Quote> recovery;
    try {
        recovery = market_->recoveryRate(securityId, config);
    } catch (...) {
        WLOG("security specific recovery rate not found for security ID "
             << securityId << ", falling back on the recovery rate for credit curve Id " << creditCurveId);
        if (!creditCurveId.empty())
            recovery = market_->recoveryRate(creditCurveId, config);
    }
    Handle<Quote> spread;
    try {
        spread = market_->securitySpread(securityId, config);
    } catch (...) {
    }

    if (!hasCreditRisk) {
        defaultCurve = Handle<DefaultProbabilityTermStructure>();
    }

    // check if add results are to be generated

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    // get LGM model

    boost::shared_ptr<QuantExt::LGM> lgm = model(id, ccy, securityId, maturityDate, generateAdditionalResults);

    // get pricing engine config

    bool mesherIsStatic = parseBool(engineParameter("MesherIsStatic", {}, true));
    Size timeStepsPerYear = parseInteger(engineParameter("TimeStepsPerYear", {}, true));
    Size stateGridPoints = parseInteger(engineParameter("StateGridPoints", {}, true));
    Real mesherEpsilon = parseReal(engineParameter("MesherEpsilon", {}, true));
    Real mesherScaling = parseReal(engineParameter("MesherScaling", {}, true));

    // build engine

    return boost::make_shared<QuantExt::FdLgmCallableBondEngine>(
        Handle<QuantExt::LGM>(lgm), referenceCurve, spread, defaultCurve, recovery, mesherIsStatic, timeStepsPerYear,
        stateGridPoints, mesherEpsilon, mesherScaling, generateAdditionalResults);
}

} // namespace data
} // namespace ore
