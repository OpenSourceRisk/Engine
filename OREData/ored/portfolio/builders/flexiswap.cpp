/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/flexiswap.hpp>
#include <qle/pricingengines/numericlgmflexiswapengine.hpp>

#include <ored/model/lgmbuilder.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/pricingengines/swap/discountingswapengine.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<QuantExt::LGM> FlexiSwapBGSLGMGridEngineBuilderBase::model(const string& id, const string& key,
                                                                             const std::vector<Date>& expiries,
                                                                             const Date& maturity,
                                                                             const std::vector<Real>& strikes) {

    // TODO this is the same as in LGMBermudanSwaptionEngineBuilder::model(), factor the model building out

    DLOG("Get model data");
    auto calibration = parseCalibrationType(modelParameter("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(modelParameter("CalibrationStrategy"));
    std::string referenceCalibrationGrid = modelParameter("ReferenceCalibrationGrid", {}, false, "");
    Real lambda = parseReal(modelParameter("Reversion"));
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

    auto data = QuantLib::ext::make_shared<IrLgmData>();

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
            for (Size i = 0; i < expiryDates.size(); ++i) {
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

    // Build model
    DLOG("Build LGM model");
    QuantLib::ext::shared_ptr<LgmBuilder> calib = QuantLib::ext::make_shared<LgmBuilder>(
        market_, data, configuration(MarketContext::irCalibration), tolerance, continueOnCalibrationError,
        referenceCalibrationGrid, generateAdditionalResults, id);

    // In some cases, we do not want to calibrate the model
    QuantLib::ext::shared_ptr<QuantExt::LGM> model;
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

QuantLib::ext::shared_ptr<PricingEngine> FlexiSwapLGMGridEngineBuilder::engineImpl(const string& id, const string& id2,
                                                                           const string& key,
                                                                           const std::vector<Date>& expiries,
                                                                           const Date& maturity,
                                                                           const std::vector<Real>& strikes) {
    DLOG("Building LGM Grid Flexi Swap engine for trade " << id);

    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm = model(id, key, expiries, maturity, strikes);

    DLOG("Get engine data");
    Real sy = parseReal(engineParameter("sy"));
    Size ny = parseInteger(engineParameter("ny"));
    Real sx = parseReal(engineParameter("sx"));
    Size nx = parseInteger(engineParameter("nx"));
    QuantExt::NumericLgmFlexiSwapEngine::Method method;
    if (engineParameter("method") == "SingleSwaptions")
        method = QuantExt::NumericLgmFlexiSwapEngine::Method::SingleSwaptions;
    else if (engineParameter("method") == "SwaptionArray")
        method = QuantExt::NumericLgmFlexiSwapEngine::Method::SwaptionArray;
    else if (engineParameter("method") == "Automatic")
        method = QuantExt::NumericLgmFlexiSwapEngine::Method::Automatic;
    else {
        QL_FAIL("FlexiSwap engine parameter method (" << engineParameter("method") << ") not recognised");
    }
    Real singleSwaptionThreshold = parseReal(engineParameter("singleSwaptionThreshold"));

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    QuantLib::ext::shared_ptr<IborIndex> index;
    string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    Handle<YieldTermStructure> dscCurve = market_->discountCurve(ccy, configuration(MarketContext::pricing));
    return QuantLib::ext::make_shared<QuantExt::NumericLgmFlexiSwapEngine>(lgm, sy, ny, sx, nx, dscCurve, method,
                                                                   singleSwaptionThreshold);
}

QuantLib::ext::shared_ptr<PricingEngine>
FlexiSwapBGSDiscountingEngineBuilderBase::engineImpl(const string& id, const string& id2, const string& key,
                                                     const std::vector<Date>& expiries, const Date& maturity,
                                                     const std::vector<Real>& strikes) {
    DLOG("Building Discounting Flexi Swap engine for trade " << id);
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    QuantLib::ext::shared_ptr<IborIndex> index;
    string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    Handle<YieldTermStructure> dscCurve = market_->discountCurve(ccy, configuration(MarketContext::pricing));
    return QuantLib::ext::make_shared<QuantLib::DiscountingSwapEngine>(dscCurve);
}

} // namespace data
} // namespace ore
