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
#include <ored/model/utilities.hpp>

#include <qle/models/hwconstantparametrization.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

HwBuilder::HwBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                     const QuantLib::ext::shared_ptr<HwModelData>& data, const IrModel::Measure measure,
                     const HwModel::Discretization discretization, const bool evaluateBankAccount,
                     const std::string& configuration, Real bootstrapTolerance, const bool continueOnError,
                     const std::string& referenceCalibrationGrid, const bool setCalibrationInfo)
    : IrModelBuilder(market, data, data->optionExpiries(), data->optionTerms(), data->optionStrikes(), configuration,
                     bootstrapTolerance, continueOnError, referenceCalibrationGrid,
                     BlackCalibrationHelper::RelativePriceError, false, false,
                     (data->calibrateSigma() || data->calibrateKappa()) &&
                         data->calibrationType() != CalibrationType::None,
                     "HW", "HW"),
      setCalibrationInfo_(setCalibrationInfo), measure_(measure), discretization_(discretization),
      evaluateBankAccount_(evaluateBankAccount) {}

void HwBuilder::initParametrization() const {

    if (parametrizationInitialized_)
        return;

    auto hwData = QuantLib::ext::dynamic_pointer_cast<HwModelData>(data_);

    Array sigmaTimes(hwData->sigmaTimes().begin(), hwData->sigmaTimes().end());
    Array kappaTimes(hwData->kappaTimes().begin(), hwData->kappaTimes().end());
    std::vector<Matrix> sigma(hwData->sigmaValues().begin(), hwData->sigmaValues().end());
    std::vector<Array> kappa(hwData->kappaValues().begin(), hwData->kappaValues().end());

    QL_REQUIRE(hwData->sigmaType() == ParamType::Constant, "HwBuilder: only constant sigma is supported at the moment");
    QL_REQUIRE(hwData->kappaType() == ParamType::Constant, "HwBuilder: only constant sigma is supported at the moment");

    if (hwData->sigmaType() == ParamType::Constant) {
        QL_REQUIRE(hwData->sigmaTimes().size() == 0,
                   "HwBuilder: empty volatility time grid expected for constant parameter type");
        QL_REQUIRE(hwData->sigmaValues().size() == 1,
                   "HwBuilder: initial volatility values should have size 1 for constant parameter type");
    } else if (hwData->sigmaType() == ParamType::Piecewise) {
        if (hwData->calibrateSigma() && hwData->calibrationType() == CalibrationType::Bootstrap) {
            if (hwData->sigmaTimes().size() > 0) {
                DLOG("overriding alpha time grid with swaption expiries, set all initial values to first given value");
            }
            QL_REQUIRE(swaptionExpiries_.size() > 0, "empty swaptionExpiries");
            sigmaTimes = Array(swaptionExpiries_.begin(), std::next(swaptionExpiries_.end(), -1));
            sigma = std::vector<Matrix>(sigmaTimes.size() + 1, hwData->sigmaValues()[0]);
        } else {
            QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1,
                       "HwBBuilder: hw volatility time and initial value array sizes do not match");
        }
    } else {
        QL_FAIL("HwBuilder: volatility parameter type not covered");
    }

    if (hwData->kappaType() == ParamType::Constant) {
        QL_REQUIRE(hwData->kappaTimes().size() == 0,
                   "HwBuilder: empty reversion time grid expected for constant parameter type");
        QL_REQUIRE(hwData->kappaValues().size() == 1,
                   "HwBuidler: initial reversion values should have size 1 for constant parameter type");
    } else if (hwData->kappaType() == ParamType::Piecewise) {
        if (hwData->calibrateKappa() && hwData->calibrationType() == CalibrationType::Bootstrap) {
            if (hwData->kappaTimes().size() > 0) {
                DLOG("overriding h time grid with swaption underlying maturities, set all initial values to first "
                     "given value");
            }
            kappaTimes = Array(swaptionMaturities_.begin(), swaptionMaturities_.end());
            kappa = std::vector<Array>(kappaTimes.size() + 1, hwData->kappaValues()[0]);
        } else { // use input time grid and input h array otherwise
            QL_REQUIRE(kappa.size() == kappaTimes.size() + 1, "HwBuilder:: hw kappa grids do not match");
        }
    } else
        QL_FAIL("HwBuilder: reversion parameter type case not covered");

    DLOGGERSTREAM("before calibration: sigma times = " << sigmaTimes);
    DLOGGERSTREAM("before calibration: kappa times = " << kappaTimes);

    Currency ccy = parseCurrency(currency_);

    parametrization_ =
        QuantLib::ext::make_shared<QuantExt::IrHwConstantParametrization>(ccy, modelDiscountCurve_, sigma[0], kappa[0]);

    DLOG("alpha times size: " << sigmaTimes.size());
    DLOG("lambda times size: " << kappaTimes.size());

    auto hwParametrization = QuantLib::ext::dynamic_pointer_cast<IrHwParametrization>(parametrization_);

    model_ = QuantLib::ext::make_shared<QuantExt::HwModel>(hwParametrization, measure_, discretization_,
                                                           evaluateBankAccount_);
    params_ = model_->params();

    parametrizationInitialized_ = true;
} // initiParametrization()

void HwBuilder::calibrate() const {

    // TODO

} // calibrate()

QuantLib::ext::shared_ptr<PricingEngine> HwBuilder::getPricingEngine() const {

    // TODO

    return nullptr;
}

} // namespace data
} // namespace ore
