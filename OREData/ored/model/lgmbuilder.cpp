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
#include <ored/model/structuredmodelerror.hpp>
#include <ored/model/structuredmodelwarning.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

LgmBuilder::LgmBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                       const QuantLib::ext::shared_ptr<IrLgmData>& data, const std::string& configuration,
                       const Real bootstrapTolerance, const bool continueOnError,
                       const std::string& referenceCalibrationGrid, const bool setCalibrationInfo,
                       const std::string& id, BlackCalibrationHelper::CalibrationErrorType calibrationErrorType,
                       const bool allowChangingFallbacksUnderScenarios, const bool allowModelFallbacks)
    : IrModelBuilder(market, data, data->optionExpiries(), data->optionTerms(), data->optionStrikes(), configuration,
                     bootstrapTolerance, continueOnError, referenceCalibrationGrid, calibrationErrorType,
                     allowChangingFallbacksUnderScenarios, allowModelFallbacks,
                     (data->calibrateA() || data->calibrateH()) && data->calibrationType() != CalibrationType::None,
                     "LGM", id),
      setCalibrationInfo_(setCalibrationInfo) {}

void LgmBuilder::initParametrization() const {

    if (parametrizationInitialized_)
        return;

    auto lgmData = QuantLib::ext::dynamic_pointer_cast<LgmData>(data_);

    Array aTimes(lgmData->aTimes().begin(), lgmData->aTimes().end());
    Array hTimes(lgmData->hTimes().begin(), lgmData->hTimes().end());
    Array alpha(lgmData->aValues().begin(), lgmData->aValues().end());
    Array h(lgmData->hValues().begin(), lgmData->hValues().end());

    if (lgmData->aParamType() == ParamType::Constant) {
        QL_REQUIRE(lgmData->aTimes().size() == 0,
                   "LgmBuilder: empty volatility time grid expected for constant parameter type");
        QL_REQUIRE(lgmData->aValues().size() == 1,
                   "LgmBuilder: initial volatility values should have size 1 for constant parameter type");
    } else if (lgmData->aParamType() == ParamType::Piecewise) {
        if (lgmData->calibrateA() && lgmData->calibrationType() == CalibrationType::Bootstrap) {
            if (lgmData->aTimes().size() > 0) {
                DLOG("overriding alpha time grid with swaption expiries, set all initial values to first given value");
            }
            QL_REQUIRE(!swaptionExpiries_.empty(), "empty swaptionExpiries");
            QL_REQUIRE(!lgmData->aValues().empty(),
                       "LgmBuilder: LGM volatility has empty initial values, requires one initial value");
            aTimes = Array(swaptionExpiries_.begin(), std::next(swaptionExpiries_.end(), -1));
            alpha = Array(aTimes.size() + 1, lgmData->aValues()[0]);
        } else {
            QL_REQUIRE(alpha.size() == aTimes.size() + 1,
                       "LgmBuilder: LGM volatility time and initial value array sizes do not match");
        }
    } else
        QL_FAIL("LgmBuilder: volatility parameter type not covered");

    if (lgmData->hParamType() == ParamType::Constant) {
        QL_REQUIRE(lgmData->hTimes().size() == 0,
                   "LgmBuilder: empty reversion time grid expected for constant parameter type");
        QL_REQUIRE(lgmData->hValues().size() == 1,
                   "LgmBuidler: initial reversion values should have size 1 for constant parameter type");
    } else if (lgmData->hParamType() == ParamType::Piecewise) {
        if (lgmData->calibrateH() && lgmData->calibrationType() == CalibrationType::Bootstrap) {
            if (lgmData->hTimes().size() > 0) {
                DLOG("overriding h time grid with swaption underlying maturities, set all initial values to first "
                     "given value");
            }
            hTimes = Array(swaptionMaturities_.begin(), swaptionMaturities_.end());
            h = Array(hTimes.size() + 1, lgmData->hValues()[0]);
        } else { // use input time grid and input h array otherwise
            QL_REQUIRE(h.size() == hTimes.size() + 1, "H grids do not match");
        }
    } else
        QL_FAIL("LgmBuilder: reversion parameter type case not covered");

    Currency ccy = parseCurrency(currency_);

    DLOG("before calibration: alpha times = " << aTimes << " values = " << alpha);
    DLOG("before calibration:     h times = " << hTimes << " values = " << h);

    if (lgmData->reversionType() == LgmData::ReversionType::HullWhite &&
        lgmData->volatilityType() == LgmData::VolatilityType::HullWhite) {
        DLOG("IR parametrization for " << data_->qualifier() << ": IrLgm1fPiecewiseConstantHullWhiteAdaptor");
        parametrization_ = QuantLib::ext::make_shared<QuantExt::IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
    } else if (lgmData->reversionType() == LgmData::ReversionType::HullWhite &&
               lgmData->volatilityType() == LgmData::VolatilityType::Hagan) {
        DLOG("IR parametrization for " << data_->qualifier() << ": IrLgm1fPiecewiseConstant");
        parametrization_ = QuantLib::ext::make_shared<QuantExt::IrLgm1fPiecewiseConstantParametrization>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
    } else if (lgmData->reversionType() == LgmData::ReversionType::Hagan &&
               lgmData->volatilityType() == LgmData::VolatilityType::Hagan) {
        parametrization_ = QuantLib::ext::make_shared<QuantExt::IrLgm1fPiecewiseLinearParametrization>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
        DLOG("IR parametrization for " << data_->qualifier() << ": IrLgm1fPiecewiseLinear");
    } else {
        QL_FAIL("LgmBuilder: Reversion type Hagan and volatility type HullWhite not covered");
    }
    DLOG("alpha times size: " << aTimes.size());
    DLOG("lambda times size: " << hTimes.size());

    auto lgmParametrization = QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(parametrization_);

    DLOG("Apply shift horizon and scale (if not 0.0 and 1.0 respectively)");

    QL_REQUIRE(lgmData->shiftHorizon() >= 0.0, "shift horizon must be non negative");
    QL_REQUIRE(lgmData->scaling() > 0.0, "scaling must be positive");

    if (lgmData->shiftHorizon() > 0.0) {
        Real value = -lgmParametrization->H(lgmData->shiftHorizon());
        DLOG("Apply shift horizon " << lgmData->shiftHorizon() << " (C=" << value << ") to the " << lgmData->qualifier()
                                    << " LGM model");
        lgmParametrization->shift() = value;
    }

    if (lgmData->scaling() != 1.0) {
        DLOG("Apply scaling " << lgmData->scaling() << " to the " << lgmData->qualifier() << " LGM model");
        lgmParametrization->scaling() = lgmData->scaling();
    }

    model_ = QuantLib::ext::make_shared<QuantExt::LGM>(lgmParametrization);
    params_ = model_->params();

    parametrizationInitialized_ = true;
} // initiParametrization()

void LgmBuilder::calibrate() const {

    auto lgmData = QuantLib::ext::dynamic_pointer_cast<LgmData>(data_);
    auto lgmModel = QuantLib::ext::dynamic_pointer_cast<LGM>(model_);
    auto lgmParametrization = QuantLib::ext::dynamic_pointer_cast<IrLgm1fParametrization>(parametrization_);

    // precheck if initial vol values are high enough to produce a signal for the optimizer
    if (lgmData->calibrateA() && lgmData->calibrationType() == CalibrationType::Bootstrap) {
        DLOG("running precheck whether initial modelVol values are high enough to produce a signal for the "
             "optimizer.");
        Array tunedParams(params_);
        for (Size j = 0; j < swaptionBasket_.size(); ++j) {
            constexpr double minRatio = 1E-4;
            constexpr Size maxAttempts = 10;
            constexpr double growFactor = 1.3;
            if (swaptionBasket_[j]->modelValue() / swaptionBasket_[j]->marketValue() < minRatio) {
                DLOG("swaption #" << j << ": modelValue (" << swaptionBasket_[j]->modelValue() << ") < " << minRatio
                                  << " x marketValue (" << swaptionBasket_[j]->marketValue()
                                  << "). Trying to increase modelVol.");
                auto fixedParams = lgmModel->MoveVolatility(j);
                auto it = std::find(fixedParams.begin(), fixedParams.end(), false);
                if (it != fixedParams.end()) {
                    Size idx = std::distance(fixedParams.begin(), it);
                    for (Size attempts = 0;
                         attempts < maxAttempts &&
                         swaptionBasket_[j]->modelValue() / swaptionBasket_[j]->marketValue() < minRatio;
                         ++attempts) {
                        tunedParams[idx] *= growFactor;
                        lgmModel->setParams(tunedParams);
                        lgmModel->generateArguments();
                    }
                    if (swaptionBasket_[j]->modelValue() / swaptionBasket_[j]->marketValue() < minRatio) {
                        DLOG("swaption #" << j << ": increasing modelVol did not bring modelValue / marketValue below "
                                          << minRatio << ". Continue with original modelVol");
                        tunedParams[idx] = params_[idx];
                        lgmModel->setParams(tunedParams);
                    }
                    DLOG("swaption #" << j << ": change modelVol " << params_[idx] << " -> " << tunedParams[idx]
                                      << ": new modelValue = " << swaptionBasket_[j]->modelValue()
                                      << ", new ratio to marketValue = "
                                      << swaptionBasket_[j]->modelValue() / swaptionBasket_[j]->marketValue());
                }
            }
        }
    }

    // call into the actual calibration routines
    LgmCalibrationInfo calibrationInfo;
    error_ = QL_MAX_REAL;
    std::string errorTemplate =
        std::string("Failed to calibrate LGM Model. ") +
        (continueOnError_ ? std::string("Calculation will proceed.") : std::string("Calculation will be aborted."));
    try {
        if (lgmData->calibrateA() && !lgmData->calibrateH() &&
            lgmData->calibrationType() == CalibrationType::Bootstrap) {
            DLOG("call calibrateVolatilitiesIterative for volatility calibration (bootstrap)");
            lgmModel->calibrateVolatilitiesIterative(swaptionBasket_, *optimizationMethod_, endCriteria_);
        } else if (lgmData->calibrateH() && !lgmData->calibrateA() &&
                   lgmData->calibrationType() == CalibrationType::Bootstrap) {
            DLOG("call calibrateReversionsIterative for reversion calibration (bootstrap)");
            lgmModel->calibrateVolatilitiesIterative(swaptionBasket_, *optimizationMethod_, endCriteria_);
        } else {
            QL_REQUIRE(lgmData->calibrationType() != CalibrationType::Bootstrap,
                       "LgmBuidler: Calibration type Bootstrap can be used with volatilities and reversions calibrated "
                       "simultaneously. Either choose BestFit oder fix one of these parameters.");
            if (lgmData->calibrateA() && !lgmData->calibrateH()) {
                DLOG("call calibrateVolatilities for (global) volatility calibration")
                lgmModel->calibrateVolatilities(swaptionBasket_, *optimizationMethod_, endCriteria_);
            } else if (lgmData->calibrateH() && !lgmData->calibrateA()) {
                DLOG("call calibrateReversions for (global) reversion calibration")
                lgmModel->calibrateReversions(swaptionBasket_, *optimizationMethod_, endCriteria_);
            } else {
                DLOG("call calibrate for global volatility and reversion calibration");
                lgmModel->calibrate(swaptionBasket_, *optimizationMethod_, endCriteria_);
            }
        }
        DLOG("LGM " << lgmData->qualifier() << " calibration errors:");
        error_ = getCalibrationError(swaptionBasket_);
    } catch (const std::exception& e) {
        // just log a warning, we check below if we meet the bootstrap tolerance and handle the result there
        StructuredModelErrorMessage(errorTemplate, e.what(), id_).log();
    }
    calibrationInfo.rmse = error_;
    if (fabs(error_) < bootstrapTolerance_ ||
        (lgmData->calibrationType() == CalibrationType::BestFit && error_ != QL_MAX_REAL)) {
        // we check the log level here to avoid unnecessary computations
        if (Log::instance().filter(ORE_DEBUG) || setCalibrationInfo_) {
            DLOGGERSTREAM("Basket details:");
            try {
                auto d = getBasketDetails(calibrationInfo.swaptionData);
                DLOGGERSTREAM(d);
            } catch (const std::exception& e) {
                WLOG("An error occurred: " << e.what());
            }
            DLOGGERSTREAM("Calibration details (with time grid = calibration swaption expiries):");
            try {
                auto d = getCalibrationDetails(calibrationInfo, swaptionBasket_, lgmParametrization);
                DLOGGERSTREAM(d);
            } catch (const std::exception& e) {
                WLOG("An error occurred: " << e.what());
            }
            DLOGGERSTREAM("Parameter details (with parameter time grid)");
            DLOGGERSTREAM(getCalibrationDetails(calibrationInfo, swaptionBasket_, lgmParametrization))
            DLOGGERSTREAM("rmse = " << error_);
            calibrationInfo.valid = true;
        }
    } else {
        std::string exceptionMessage = "LGM (" + lgmData->qualifier() + ") calibration target function value (" +
                                       std::to_string(error_) + ") exceeds notification threshold (" +
                                       std::to_string(bootstrapTolerance_) + ").";
        StructuredModelWarningMessage(errorTemplate, exceptionMessage, id_).log();
        WLOGGERSTREAM("Basket details:");
        try {
            auto d = getBasketDetails(calibrationInfo.swaptionData);
            WLOGGERSTREAM(d);
        } catch (const std::exception& e) {
            WLOG("An error occurred: " << e.what());
        }
        WLOGGERSTREAM("Calibration details (with time grid = calibration swaption expiries):");
        try {
            auto d = getCalibrationDetails(calibrationInfo, swaptionBasket_, lgmParametrization);
            WLOGGERSTREAM(d);
        } catch (const std::exception& e) {
            WLOG("An error occurred: " << e.what());
        }
        WLOGGERSTREAM("Parameter details (with parameter time grid)");
        WLOGGERSTREAM(getCalibrationDetails(lgmParametrization));
        WLOGGERSTREAM("rmse = " << error_);
        calibrationInfo.valid = true;
        if (!continueOnError_) {
            QL_FAIL(exceptionMessage);
        }
    }
    lgmModel->setCalibrationInfo(calibrationInfo);

} // calibrate()

QuantLib::ext::shared_ptr<PricingEngine> LgmBuilder::getPricingEngine() const {
    auto lgmData = QuantLib::ext::dynamic_pointer_cast<LgmData>(data_);
    auto lgmModel = QuantLib::ext::dynamic_pointer_cast<LGM>(model_);
    auto engine = QuantLib::ext::make_shared<QuantExt::AnalyticLgmSwaptionEngine>(lgmModel, calibrationDiscountCurve_,
                                                                                  lgmData->floatSpreadMapping());
    engine->enableCache(!lgmData->calibrateH(), !lgmData->calibrateA());
    return engine;
}

} // namespace data
} // namespace ore
