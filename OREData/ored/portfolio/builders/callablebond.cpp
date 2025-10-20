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
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/pricingengines/numericlgmcallablebondengine.hpp>

#include <ql/time/date.hpp>

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
        allowChangingFallbacks, allowModelFallbacks);

    // In some cases, we do not want to calibrate the model
    QuantLib::ext::shared_ptr<QuantExt::IrModel> model;
    if (globalParameters_.count("Calibrate") == 0 || parseBool(globalParameters_.at("Calibrate"))) {
        DLOG("Calibrate model (configuration " << configuration(MarketContext::irCalibration) << ")");
        model = calib->model();
    } else {
        DLOG("Skip calibration of model based on global parameters");
        calib->freeze();
        model = calib->model();
        calib->unfreeze();
    }
    engineFactory()->modelBuilders().insert(std::make_pair(id, calib));

    return model;
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

} // namespace data
} // namespace ore
