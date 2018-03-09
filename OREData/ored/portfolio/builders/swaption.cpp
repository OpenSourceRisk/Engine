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
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <qle/pricingengines/numericlgmswaptionengine.hpp>

namespace ore {
namespace data {

boost::shared_ptr<PricingEngine> EuropeanSwaptionEngineBuilder::engineImpl(const Currency& ccy) {
    const string& ccyCode = ccy.code();
    Handle<YieldTermStructure> yts = market_->discountCurve(ccyCode, configuration(MarketContext::pricing));
    QL_REQUIRE(!yts.empty(), "engineFactory error: yield term structure not found for currency " << ccyCode);
    Handle<SwaptionVolatilityStructure> svts = market_->swaptionVol(ccyCode, configuration(MarketContext::pricing));
    QL_REQUIRE(!svts.empty(), "engineFactory error: swaption vol structure not found for currency " << ccyCode);

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

boost::shared_ptr<QuantExt::LGM> LGMBermudanSwaptionEngineBuilder::model(const string& id, bool isNonStandard,
                                                                         const string& ccy,
                                                                         const std::vector<Date>& expiries,
                                                                         const Date& maturity,
                                                                         const std::vector<Real>& strikes) {

    DLOG("Get model data");
    auto calibration = parseCalibrationType(modelParameters_.at("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(modelParameters_.at("CalibrationStrategy"));
    Real lambda = parseReal(modelParameters_.at("Reversion"));
    vector<Real> sigma = parseListOfValues<Real>(modelParameters_.at("Volatility"), &parseReal);
    vector<Real> sigmaTimes(0);
    if (modelParameters_.count("VolatilityTimes") > 0)
        sigmaTimes = parseListOfValues<Real>(modelParameters_.at("VolatilityTimes"), &parseReal);
    QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1, "there must be n+1 volatilities (" << sigma.size()
                                                                                         << ") for n volatility times ("
                                                                                         << sigmaTimes.size() << ")");
    Real tolerance = parseReal(modelParameters_.at("Tolerance"));
    auto reversionType = parseReversionType(modelParameters_.at("ReversionType"));
    auto volatilityType = parseVolatilityType(modelParameters_.at("VolatilityType"));

    auto data = boost::make_shared<IrLgmData>();

    // check for allowed calibration / bermudan strategy settings
    std::vector<std::pair<CalibrationType, LgmData::CalibrationStrategy>> validCalPairs = {
        {CalibrationType::None, LgmData::CalibrationStrategy::None},
        {CalibrationType::Bootstrap, LgmData::CalibrationStrategy::CoterminalATM},
        {CalibrationType::Bootstrap, LgmData::CalibrationStrategy::CoterminalDealStrike},
        {CalibrationType::BestFit, LgmData::CalibrationStrategy::CoterminalATM},
        {CalibrationType::BestFit, LgmData::CalibrationStrategy::CoterminalDealStrike}};

    QL_REQUIRE(std::find(validCalPairs.begin(), validCalPairs.end(),
                         std::make_pair(calibration, calibrationStrategy)) != validCalPairs.end(),
               "Calibration (" << calibration << ") and CalibrationStrategy (" << calibrationStrategy
                               << ") are not allowed in this combination");

    // compute horizon shift
    Real shiftHorizon = 0.5; // default value
    if (modelParameters_.find("ShiftHorizon") != modelParameters_.end()) {
        shiftHorizon = parseReal(modelParameters_.at("ShiftHorizon"));
    }
    Date today = Settings::instance().evaluationDate();
    shiftHorizon = ActualActual().yearFraction(today, maturity) * shiftHorizon;

    // Default: no calibration, constant lambda and sigma from engine configuration
    data->reset();
    data->ccy() = ccy;
    data->calibrateH() = false;
    data->hParamType() = ParamType::Constant;
    data->hValues() = {lambda};
    data->reversionType() = reversionType;
    data->calibrateA() = false;
    data->aParamType() = ParamType::Piecewise;
    data->aValues() = sigma;
    data->aTimes() = sigmaTimes;
    data->volatilityType() = volatilityType;
    data->calibrationStrategy() = calibrationStrategy;
    data->calibrationType() = calibration;
    data->shiftHorizon() = shiftHorizon;

    if (calibrationStrategy == LgmData::CalibrationStrategy::CoterminalATM ||
        calibrationStrategy == LgmData::CalibrationStrategy::CoterminalDealStrike) {
        DLOG("Build LgmData for co-terminal specification");
        vector<string> expiryDates, termDates;
        for (Size i = 0; i < expiries.size(); ++i) {
            expiryDates.push_back(to_string(expiries[i]));
            termDates.push_back(to_string(maturity));
        }
        data->optionExpiries() = expiryDates;
        data->optionTerms() = termDates;
        data->optionStrikes().resize(expiryDates.size(), "ATM");
        if (calibrationStrategy == LgmData::CalibrationStrategy::CoterminalDealStrike) {
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

    // Build and calibrate model
    DLOG("Build LGM model");
    boost::shared_ptr<LgmBuilder> calib =
        boost::make_shared<LgmBuilder>(market_, data, configuration(MarketContext::irCalibration), tolerance);
    DLOG("Calibrate model (configuration " << configuration(MarketContext::irCalibration) << ")");
    calib->recalibrate();
    modelBuilders_.insert(std::make_pair(id, calib));
    return calib->model();
}

boost::shared_ptr<PricingEngine> LGMGridBermudanSwaptionEngineBuilder::engineImpl(const string& id, bool isNonStandard,
                                                                                  const string& ccy,
                                                                                  const std::vector<Date>& expiries,
                                                                                  const Date& maturity,
                                                                                  const std::vector<Real>& strikes) {
    DLOG("Building Bermudan Swaption engine for trade " << id);

    boost::shared_ptr<QuantExt::LGM> lgm = model(id, isNonStandard, ccy, expiries, maturity, strikes);

    DLOG("Get engine data");
    Real sy = parseReal(engineParameters_.at("sy"));
    Size ny = parseInteger(engineParameters_.at("ny"));
    Real sx = parseReal(engineParameters_.at("sx"));
    Size nx = parseInteger(engineParameters_.at("nx"));

    // Build engine
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    Handle<YieldTermStructure> dscCurve = market_->discountCurve(ccy, configuration(MarketContext::pricing));
    boost::shared_ptr<PricingEngine> p;
    if (isNonStandard)
        return boost::make_shared<QuantExt::NumericLgmNonstandardSwaptionEngine>(lgm, sy, ny, sx, nx, dscCurve);
    else
        return boost::make_shared<QuantExt::NumericLgmSwaptionEngine>(lgm, sy, ny, sx, nx, dscCurve);
}

} // namespace data
} // namespace ore
