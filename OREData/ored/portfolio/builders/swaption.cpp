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

#include <ored/model/lgmbuilder.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
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
QuantLib::ext::shared_ptr<PricingEngine> buildMcEngine(
    const std::function<string(string, const std::vector<std::string>&, const bool, const string&)>& engineParameter,
    const QuantLib::ext::shared_ptr<LGM>& lgm, const Handle<YieldTermStructure>& discountCurve,
    const std::vector<Date>& simulationDates, const std::vector<Size>& externalModelIndices) {

    return QuantLib::ext::make_shared<QuantExt::McMultiLegOptionEngine>(
        lgm, parseSequenceType(engineParameter("Training.Sequence", {}, false, "SobolBrownianBridge")),
        parseSequenceType(engineParameter("Pricing.Sequence", {}, false, "SobolBrownianBridge")),
        parseInteger(engineParameter("Training.Samples", {}, true, std::string())),
        parseInteger(engineParameter("Pricing.Samples", {}, false, "0")),
        parseInteger(engineParameter("Training.Seed", {}, true, std::string())),
        parseInteger(engineParameter("Pricing.Seed", {}, false, "42")),
        parseInteger(engineParameter("Training.BasisFunctionOrder", {}, true, std::string())),
        parsePolynomType(engineParameter("Training.BasisFunction", {}, true, std::string())),
        parseSobolBrownianGeneratorOrdering(engineParameter("BrownianBridgeOrdering", {}, false, "Steps")),
        parseSobolRsgDirectionIntegers(engineParameter("SobolDirectionIntegers", {}, false, "JoeKuoD7")), discountCurve,
        simulationDates, externalModelIndices, parseBool(engineParameter("MinObsDate", {}, false, "true")),
        parseRegressorModel(engineParameter("RegressorModel", {}, false, "Simple")),
        parseRealOrNull(engineParameter("RegressionVarianceCutoff", {}, false, std::string())));
}
} // namespace

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<PricingEngine>
EuropeanSwaptionEngineBuilder::engineImpl(const string& id, const string& key, const std::vector<Date>& dates,
                                          const Date& maturity, const std::vector<Real>& strikes, const bool isAmerican,
                                          const std::string& discountCurve, const std::string& securitySpread) {
    QuantLib::ext::shared_ptr<IborIndex> index;
    string ccyCode = tryParseIborIndex(key, index) ? index->currency().code() : key;
    Handle<YieldTermStructure> yts =
        discountCurve.empty() ? market_->discountCurve(ccyCode, configuration(MarketContext::pricing))
                              : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
    if (!securitySpread.empty())
        yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
            yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
    Handle<SwaptionVolatilityStructure> svts = market_->swaptionVol(key, configuration(MarketContext::pricing));
    return QuantLib::ext::make_shared<BlackMultiLegOptionEngine>(yts, svts);
}

QuantLib::ext::shared_ptr<QuantExt::LGM> LGMSwaptionEngineBuilder::model(const string& id, const string& key,
                                                                 const std::vector<Date>& expiries,
                                                                 const Date& maturity, const std::vector<Real>& strikes,
                                                                 const bool isAmerican) {
    QuantLib::ext::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;

    DLOG("Get model data");
    auto calibration = parseCalibrationType(modelParameter("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(modelParameter("CalibrationStrategy"));
    // required for american options to set up calibration basket
    std::string referenceCalibrationGrid = modelParameter("ReferenceCalibrationGrid", {}, isAmerican, "");
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

    auto floatSpreadMapping = parseFloatSpreadMapping(modelParameter("FloatSpreadMapping", {}, false, "proRata"));

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
    data->floatSpreadMapping() = floatSpreadMapping;

    std::vector<Date> effExpiries;
    std::vector<Real> effStrikes;
    if (!isAmerican) {
        effExpiries = expiries;
        effStrikes = strikes;
    } else {
        QL_REQUIRE(expiries.size() == 2 && strikes.size() == 2,
                   "LGMBermudanAmericanSwaptionEngineBuilder::model(): expected 2 expiries and strikes for exercise "
                   "style 'American', got "
                       << expiries.size() << " expiries and " << strikes.size() << " strikes.");
        // keep one calibration instrument per reference grid interval
        DateGrid grid(referenceCalibrationGrid);
        std::copy_if(grid.dates().begin(), grid.dates().end(), std::back_inserter(effExpiries),
                     [&expiries](const Date& d) { return d >= expiries[0] && d < expiries[1]; });
        // simple linear interpolation of calibration strikes between endpoints, this can be refined obviously
        effStrikes.resize(effExpiries.size(), Null<Real>());
        if (strikes[0] != Null<Real>() && strikes[1] != Null<Real>()) {
            Real t0 = Actual365Fixed().yearFraction(today, expiries[0]);
            Real t1 = Actual365Fixed().yearFraction(today, expiries[1]);
            for (Size i = 0; i < effExpiries.size(); ++i) {
                Real t = Actual365Fixed().yearFraction(today, effExpiries[i]);
                effStrikes[i] = strikes[0] + (strikes[1] - strikes[0]) / (t1 - t0) * (t - t0);
            }
        }
    }

    if (calibrationStrategy == CalibrationStrategy::CoterminalATM ||
        calibrationStrategy == CalibrationStrategy::CoterminalDealStrike) {
        DLOG("Build LgmData for co-terminal specification");
        vector<string> expiryDates, termDates;
        for (Size i = 0; i < effExpiries.size(); ++i) {
            expiryDates.push_back(to_string(effExpiries[i]));
            termDates.push_back(to_string(maturity));
        }
        data->optionExpiries() = expiryDates;
        data->optionTerms() = termDates;
        data->optionStrikes().resize(expiryDates.size(), "ATM");
        if (calibrationStrategy == CalibrationStrategy::CoterminalDealStrike) {
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

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    // Build and calibrate model
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

QuantLib::ext::shared_ptr<PricingEngine>
LGMGridSwaptionEngineBuilder::engineImpl(const string& id, const string& key, const std::vector<Date>& expiries,
                                         const Date& maturity, const std::vector<Real>& strikes, const bool isAmerican,
                                         const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building LGM Grid Bermudan/American Swaption engine for trade " << id);

    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm = model(id, key, expiries, maturity, strikes, isAmerican);

    DLOG("Get engine data");
    Real sy = parseReal(engineParameter("sy"));
    Size ny = parseInteger(engineParameter("ny"));
    Real sx = parseReal(engineParameter("sx"));
    Size nx = parseInteger(engineParameter("nx"));

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    QuantLib::ext::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
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
LGMFDSwaptionEngineBuilder::engineImpl(const string& id, const string& key, const std::vector<Date>& expiries,
                                       const Date& maturity, const std::vector<Real>& strikes, const bool isAmerican,
                                       const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building LGM FD Bermudan/American Swaption engine for trade " << id);

    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm = model(id, key, expiries, maturity, strikes, isAmerican);

    DLOG("Get engine data");
    QuantLib::FdmSchemeDesc scheme = parseFdmSchemeDesc(engineParameter("Scheme"));
    Size stateGridPoints = parseInteger(engineParameter("StateGridPoints"));
    Size timeStepsPerYear = parseInteger(engineParameter("TimeStepsPerYear"));
    Real mesherEpsilon = parseReal(engineParameter("MesherEpsilon"));

    Real maxTime = lgm->termStructure()->timeFromReference(maturity);

    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    QuantLib::ext::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
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
LGMMCSwaptionEngineBuilder::engineImpl(const string& id, const string& key, const std::vector<Date>& expiries,
                                       const Date& maturity, const std::vector<Real>& strikes, const bool isAmerican,
                                       const std::string& discountCurve, const std::string& securitySpread) {
    DLOG("Building MC Bermudan/American Swaption engine for trade " << id);

    auto lgm = model(id, key, expiries, maturity, strikes, isAmerican);

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    QuantLib::ext::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    Handle<YieldTermStructure> yts =
        discountCurve.empty() ? market_->discountCurve(ccy, configuration(MarketContext::pricing))
                              : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
    if (!securitySpread.empty())
        yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
            yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
    return buildMcEngine([this](const std::string& p, const std::vector<std::string>& q, const bool m,
                                const std::string& d) { return this->engineParameter(p, q, m, d); },
                         lgm, yts, std::vector<Date>(), std::vector<Size>());
} // LgmMc engineImpl()

QuantLib::ext::shared_ptr<PricingEngine>
LGMAmcSwaptionEngineBuilder::engineImpl(const string& id, const string& key, const std::vector<Date>& expiries,
                                        const Date& maturity, const std::vector<Real>& strikes, const bool isAmerican,
                                        const std::string& discountCurve, const std::string& securitySpread) {
    QuantLib::ext::shared_ptr<IborIndex> index;
    std::string ccy = tryParseIborIndex(key, index) ? index->currency().code() : key;
    Currency curr = parseCurrency(ccy);
    DLOG("Building AMC Bermudan/American Swaption engine for key " << key << ", ccy " << ccy
                                                                   << " (from externally given CAM)");

    QL_REQUIRE(cam_ != nullptr, "LgmCamBermudanAmericanSwaptionEngineBuilder::engineImpl: cam is null");
    Size currIdx = cam_->ccyIndex(curr);
    auto lgm = cam_->lgm(currIdx);
    std::vector<Size> modelIndex(1, cam_->pIdx(CrossAssetModel::AssetType::IR, currIdx));

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    Handle<YieldTermStructure> yts =
        discountCurve.empty() ? market_->discountCurve(ccy, configuration(MarketContext::pricing))
                              : indexOrYieldCurve(market_, discountCurve, configuration(MarketContext::pricing));
    if (!securitySpread.empty())
        yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(
            yts, market_->securitySpread(securitySpread, configuration(MarketContext::pricing))));
    return buildMcEngine([this](const std::string& p, const std::vector<std::string>& q, const bool m,
                                const std::string& d) { return this->engineParameter(p, q, m, d); },
                         lgm, yts, simulationDates_, modelIndex);
} // LgmCam engineImpl

} // namespace data
} // namespace ore
