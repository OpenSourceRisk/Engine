/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/model/hwbuilder.hpp>
#include <ored/model/structuredmodelerror.hpp>
#include <ored/model/structuredmodelwarning.hpp>
#include <ored/model/utilities.hpp>

#include <qle/models/hwconstantparametrization.hpp>
#include <qle/models/hwpiecewisestatisticalparametrization.hpp>
#include <qle/pricingengines/analytichwswaptionengine.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

HwBuilder::HwBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                     const QuantLib::ext::shared_ptr<HwModelData>& data, const IrModel::Measure measure,
                     const HwModel::Discretization discretization, const bool evaluateBankAccount,
                     const std::string& configuration, Real bootstrapTolerance, const bool continueOnError,
                     const std::string& referenceCalibrationGrid, const bool setCalibrationInfo, const std::string& id,
                     BlackCalibrationHelper::CalibrationErrorType calibrationErrorType,
                     const bool allowChangingFallbacksUnderScenarios, const bool allowModelFallbacks)
    : IrModelBuilder(market, data, data->optionExpiries(), data->optionTerms(), data->optionStrikes(), configuration,
                     bootstrapTolerance, continueOnError, referenceCalibrationGrid, calibrationErrorType,
                     allowChangingFallbacksUnderScenarios, allowModelFallbacks,
                     (data->calibrateSigma() || data->calibrateKappa()) &&
                         data->calibrationType() != CalibrationType::None,
                     "HW", "HW"),
      setCalibrationInfo_(setCalibrationInfo), measure_(measure), discretization_(discretization),
      evaluateBankAccount_(evaluateBankAccount) {}

void HwBuilder::initParametrization() const {

    if (parametrizationInitialized_)
        return;

    auto hwData = QuantLib::ext::dynamic_pointer_cast<HwModelData>(data_);

    Currency ccy = parseCurrency(currency_);

    if (hwData->calibrationType() == CalibrationType::StatisticalWithRiskNeutralVolatility) {

        DLOG("HwBuilder: building a HwPiecewiseStatisticalParametrization.");

        std::vector<QuantLib::Array> loadings;
        for (auto const& l : hwData->pcaLoadings())
            loadings.push_back(Array(l.begin(), l.end()));

        Array times, values;

        if (hwData->calibratePcaSigma0()) {

            // if we calibrate, we overwrite the times with swaption times and use the front initial value as a guess

            times = Array(swaptionExpiries_.begin(), std::next(swaptionExpiries_.end(), -1));
            values = Array(times.size() + 1, hwData->pcaSigma0Values().front());

        } else {

            // if we don't calibrate, we use the times and values from the parametrization

            times = Array(hwData->pcaSigma0Times().begin(), hwData->pcaSigma0Times().end());
            values = Array(hwData->pcaSigma0Values().begin(), hwData->pcaSigma0Values().end());
        }

        parametrization_ = QuantLib::ext::make_shared<IrHwPiecewiseStatisticalParametrization>(
            ccy, modelDiscountCurve_, Array(times.begin(), times.end()),
            Array(hwData->pcaSigma0Values().begin(), hwData->pcaSigma0Values().end()), hwData->kappaValues().front(),
            Array(hwData->pcaSigmaRatios().begin(), hwData->pcaSigmaRatios().end()), loadings);

    } else {

        DLOG("HwBuilder: building a HwPiecewiseParametrization.");

        QL_REQUIRE(hwData->sigmaTimes() == hwData->kappaTimes() || hwData->kappaTimes().empty(),
                   "HwBuilder: sigma and kapp time grid must be identical or kappa must be constant");

        Array times(hwData->sigmaTimes().begin(), hwData->sigmaTimes().end());

        std::vector<Matrix> sigma(hwData->sigmaValues().begin(), hwData->sigmaValues().end());
        std::vector<Array> kappa(hwData->kappaValues().begin(), hwData->kappaValues().end());

        sigma.resize(times.size() + 1, sigma.back());
        kappa.resize(times.size() + 1, kappa.back());

        QL_REQUIRE(!hwData->calibrateSigma(), "HwBuilder: calibration of sigma is not supported.");
        QL_REQUIRE(!hwData->calibrateKappa(), "HwBuilder: calibration of kappa is not supported.");

        parametrization_ = QuantLib::ext::make_shared<QuantExt::IrHwPiecewiseParametrization>(ccy, modelDiscountCurve_,
                                                                                              times, sigma, kappa);
    }

    model_ = QuantLib::ext::make_shared<QuantExt::HwModel>(
        QuantLib::ext::dynamic_pointer_cast<IrHwParametrization>(parametrization_), measure_, discretization_,
        evaluateBankAccount_);
    params_ = model_->params();
    parametrizationInitialized_ = true;

} // initiParametrization()

void HwBuilder::calibrate() const {

    auto hwData = QuantLib::ext::dynamic_pointer_cast<HwModelData>(data_);
    auto hwModel = QuantLib::ext::dynamic_pointer_cast<HwModel>(model_);
    auto hwParametrization = QuantLib::ext::dynamic_pointer_cast<IrHwParametrization>(parametrization_);

    // call into the actual calibration routines
    HwCalibrationInfo calibrationInfo;
    error_ = QL_MAX_REAL;
    std::string errorTemplate =
        std::string("Failed to calibrate HW Model. ") +
        (continueOnError_ ? std::string("Calculation will proceed.") : std::string("Calculation will be aborted."));

    try {

        if (hwData->calibratePcaSigma0()) {

            hwModel->calibrateVolatilitiesIterativeStatisticalWithRiskNeutralVolatility(
                swaptionBasket_, *optimizationMethod_, endCriteria_);

            DLOG("HW " << hwData->qualifier() << " calibration errors:");
            error_ = getCalibrationError(swaptionBasket_);
        }

    } catch (const std::exception& e) {
        // just log a warning, we check below if we meet the bootstrap tolerance and handle the result there
        StructuredModelErrorMessage(errorTemplate, e.what(), id_).log();
    }

    calibrationInfo.rmse = error_;
    if (fabs(error_) < bootstrapTolerance_) {
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
                auto d = getCalibrationDetails(calibrationInfo, swaptionBasket_, hwParametrization);
                DLOGGERSTREAM(d);
            } catch (const std::exception& e) {
                WLOG("An error occurred: " << e.what());
            }
            DLOGGERSTREAM("Parameter details (with parameter time grid)");
            DLOGGERSTREAM(getCalibrationDetails(calibrationInfo, swaptionBasket_, hwParametrization))
            DLOGGERSTREAM("rmse = " << error_);
            calibrationInfo.valid = true;
        }
    } else {
        std::string exceptionMessage = "HullWhite (" + hwData->qualifier() + ") calibration target function value (" +
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
            auto d = getCalibrationDetails(calibrationInfo, swaptionBasket_, hwParametrization);
            WLOGGERSTREAM(d);
        } catch (const std::exception& e) {
            WLOG("An error occurred: " << e.what());
        }
        WLOGGERSTREAM("rmse = " << error_);
        calibrationInfo.valid = true;
        if (!continueOnError_) {
            QL_FAIL(exceptionMessage);
        }
    }

    hwModel->setCalibrationInfo(calibrationInfo);

} // calibrate()

QuantLib::ext::shared_ptr<PricingEngine> HwBuilder::getPricingEngine() const {
    auto hwModel = QuantLib::ext::dynamic_pointer_cast<HwModel>(model_);
    auto engine = QuantLib::ext::make_shared<QuantExt::AnalyticHwSwaptionEngine>(hwModel, calibrationDiscountCurve_);
    return engine;
}

} // namespace data
} // namespace ore
