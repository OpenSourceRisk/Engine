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
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

#include <qle/models/hwconstantparametrization.hpp>
#include <qle/models/marketobserver.hpp>

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

HwBuilder::HwBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<HwModelData>& data,
                     const IrModel::Measure measure, const HwModel::Discretization discretization,
                     const bool evaluateBankAccount, const std::string& configuration, const Real bootstrapTolerance,
                     const bool continueOnError, const std::string& referenceCalibrationGrid,
                     const bool setCalibrationInfo)
    : market_(market), configuration_(configuration), data_(data), measure_(measure), discretization_(discretization),
      /*bootstrapTolerance_(bootstrapTolerance), continueOnError_(continueOnError),*/
      referenceCalibrationGrid_(referenceCalibrationGrid), /*setCalibrationInfo_(setCalibrationInfo),*/
      optimizationMethod_(QuantLib::ext::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8))
      /*,calibrationErrorType_(BlackCalibrationHelper::RelativePriceError)*/ {

    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    string qualifier = data_->qualifier();
    currency_ = qualifier;
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(qualifier, index)) {
        currency_ = index->currency().code();
    }
    LOG("HwCalibration for qualifier " << qualifier << " (ccy=" << currency_ << "), configuration is "
                                       << configuration_);
    Currency ccy = parseCurrency(currency_);

    requiresCalibration_ =
        (data_->calibrateSigma() || data_->calibrateKappa()) && data_->calibrationType() != CalibrationType::None;

    QL_REQUIRE(!requiresCalibration_, "HwBuilder: HwModel does not support calibration at the moment.");

    // the discount curve underlying the model might be relinked to a different curve outside this builder
    // the calibration curve should always stay the same though, therefore we create a different handle for this
    modelDiscountCurve_ = RelinkableHandle<YieldTermStructure>(*market_->discountCurve(currency_, configuration_));
    calibrationDiscountCurve_ = Handle<YieldTermStructure>(*modelDiscountCurve_);

    if (requiresCalibration_) {
        svts_ = market_->swaptionVol(data_->qualifier(), configuration_);
        swapIndex_ = market_->swapIndex(market_->swapIndexBase(data_->qualifier(), configuration_), configuration_);
        shortSwapIndex_ =
            market_->swapIndex(market_->shortSwapIndexBase(data_->qualifier(), configuration_), configuration_);
        registerWith(svts_);
        marketObserver_->addObservable(swapIndex_->forwardingTermStructure());
        marketObserver_->addObservable(swapIndex_->discountingTermStructure());
        marketObserver_->addObservable(shortSwapIndex_->forwardingTermStructure());
        marketObserver_->addObservable(shortSwapIndex_->discountingTermStructure());
    }
    marketObserver_->addObservable(calibrationDiscountCurve_);
    registerWith(marketObserver_);
    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    swaptionActive_ = std::vector<bool>(data_->optionExpiries().size(), false);

    if (requiresCalibration_) {
        // buildSwaptionBasket();
    }

    Array sigmaTimes(data_->sigmaTimes().begin(), data_->sigmaTimes().end());
    Array kappaTimes(data_->kappaTimes().begin(), data_->kappaTimes().end());
    std::vector<Matrix> sigma(data_->sigmaValues().begin(), data_->sigmaValues().end());
    std::vector<Array> kappa(data_->kappaValues().begin(), data_->kappaValues().end());

    QL_REQUIRE(data_->sigmaType() == ParamType::Constant,
               "HwBuilder: only constant sigma is supported at the moment");
    QL_REQUIRE(data_->kappaType() == ParamType::Constant,
               "HwBuilder: only constant sigma is supported at the moment");

    if (data_->sigmaType() == ParamType::Constant) {
        QL_REQUIRE(data_->sigmaTimes().size() == 0,
                   "HwBuilder: empty volatility time grid expected for constant parameter type");
        QL_REQUIRE(data_->sigmaValues().size() == 1,
                   "HwBuilder: initial volatility values should have size 1 for constant parameter type");
    } else if (data_->sigmaType() == ParamType::Piecewise) {
        if (data_->calibrateSigma() && data_->calibrationType() == CalibrationType::Bootstrap) {
            if (data_->sigmaTimes().size() > 0) {
                DLOG("overriding alpha time grid with swaption expiries, set all initial values to first given value");
            }
            QL_REQUIRE(swaptionExpiries_.size() > 0, "empty swaptionExpiries");
            sigmaTimes = Array(swaptionExpiries_.begin(), swaptionExpiries_.end() - 1);
            sigma = std::vector<Matrix>(sigmaTimes.size() + 1, data_->sigmaValues()[0]);
        } else {
            QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1,
                       "HwBBuilder: hw volatility time and initial value array sizes do not match");
        }
    } else {
        QL_FAIL("HwBuilder: volatility parameter type not covered");
    }

    if (data_->kappaType() == ParamType::Constant) {
        QL_REQUIRE(data_->kappaTimes().size() == 0,
                   "HwBuilder: empty reversion time grid expected for constant parameter type");
        QL_REQUIRE(data_->kappaValues().size() == 1,
                   "HwBuidler: initial reversion values should have size 1 for constant parameter type");
    } else if (data_->kappaType() == ParamType::Piecewise) {
        if (data_->calibrateKappa() && data_->calibrationType() == CalibrationType::Bootstrap) {
            if (data_->kappaTimes().size() > 0) {
                DLOG("overriding h time grid with swaption underlying maturities, set all initial values to first "
                     "given value");
            }
            kappaTimes = swaptionMaturities_;
            kappa = std::vector<Array>(kappaTimes.size() + 1, data_->kappaValues()[0]);
        } else { // use input time grid and input h array otherwise
            QL_REQUIRE(kappa.size() == kappaTimes.size() + 1, "HwBuilder:: hw kappa grids do not match");
        }
    } else
        QL_FAIL("HwBuilder: reversion parameter type case not covered");

    DLOGGERSTREAM("before calibration: sigma times = " << sigmaTimes);
    DLOGGERSTREAM("before calibration: kappa times = " << kappaTimes);

    parametrization_ =
        QuantLib::ext::make_shared<QuantExt::IrHwConstantParametrization>(ccy, modelDiscountCurve_, sigma[0], kappa[0]);

    DLOG("alpha times size: " << sigmaTimes.size());
    DLOG("lambda times size: " << kappaTimes.size());

    model_ = QuantLib::ext::make_shared<QuantExt::HwModel>(parametrization_, measure_, discretization_, evaluateBankAccount_);
    params_ = model_->params();
}

Real HwBuilder::error() const {
    calculate();
    return error_;
}

QuantLib::ext::shared_ptr<QuantExt::HwModel> HwBuilder::model() const {
    calculate();
    return model_;
}

QuantLib::ext::shared_ptr<QuantExt::IrHwParametrization> HwBuilder::parametrization() const {
    calculate();
    return parametrization_;
}

std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> HwBuilder::swaptionBasket() const {
    calculate();
    return swaptionBasket_;
}

bool HwBuilder::requiresRecalibration() const {
    // TODO
    // return requiresCalibration_ &&
    //        (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
    return false;
}

void HwBuilder::performCalculations() const {

    DLOG("Recalibrate HW model for qualifier " << data_->qualifier() << " currency " << currency_);

    // TODO...

} // performCalculations()

void HwBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

} // namespace data
} // namespace ore
