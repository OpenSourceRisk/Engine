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

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/quotes/simplequote.hpp>

#include <qle/models/commodityschwartzparametrization.hpp>
#include <qle/models/futureoptionhelper.hpp>
#include <qle/pricingengines/commodityschwartzfutureoptionengine.hpp>

#include <ored/model/commodityschwartzmodelbuilder.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

CommoditySchwartzModelBuilder::CommoditySchwartzModelBuilder(
                         const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<CommoditySchwartzData>& data,
                         const QuantLib::Currency& baseCcy, const std::string& configuration,
                         const std::string& referenceCalibrationGrid)
    : market_(market), configuration_(configuration), data_(data), referenceCalibrationGrid_(referenceCalibrationGrid),
      baseCcy_(baseCcy) {

    optionActive_ = std::vector<bool>(data->optionExpiries().size(), false);
    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    QuantLib::Currency ccy = ore::data::parseCurrency(data->currency());
    string name = data->name();

    LOG("Start building CommoditySchwartz model for " << name);

    // get market data
    std::string fxCcyPair = ccy.code() + baseCcy_.code();
    fxSpot_ = market_->fxRate(fxCcyPair, configuration_);
    curve_ = market_->commodityPriceCurve(name, configuration_);
    vol_ = market_->commodityVolatility(name, configuration_);

    // register with market observables except vols
    marketObserver_->registerWith(fxSpot_);
    marketObserver_->registerWith(curve_);

    // register the builder with the vol and the market observer
    registerWith(vol_);
    registerWith(marketObserver_);

    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // build option basket and derive parametrization from it
    if (data->calibrateSigma() || data->calibrateKappa() || data->calibrateSeasonality())
        buildOptionBasket();

    Array seasonalityTimes_a, seasonalityValues_a;

    if (data->seasonalityParamType()  == ParamType::Constant ){
        QL_REQUIRE(data->seasonalityTimes().size() == 0, "empty seasonality time grid expected");
        QL_REQUIRE(data->seasonalityValues().size() == 1, "initial seasonality grid size 1 expected");
        seasonalityTimes_a = Array(0);
        seasonalityValues_a = Array(data->seasonalityValues().begin(), data->seasonalityValues().end());
    } else {
        if (data->calibrateSeasonality()) { // override
            QL_REQUIRE(optionExpiries_.size() > 0, "optionExpiries is empty");
            // the last expiry is taken, as a(T) is calibrated not \int_{0}^T \sigma_{T-1}(u)du
            seasonalityTimes_a = Array(optionExpiries_.begin(), optionExpiries_.end()); 
            seasonalityValues_a = Array(seasonalityTimes_a.size() + 1, data->seasonalityValues()[0]);
        } else {
            // use input time grid and input alpha array otherwise
            seasonalityTimes_a = Array(data->seasonalityTimes().begin(), data->seasonalityTimes().end());
            seasonalityValues_a = Array(data->seasonalityValues().begin(), data->seasonalityValues().end());
            QL_REQUIRE(seasonalityValues_a.size() == seasonalityTimes_a.size() + 1, "seasonality grids do not match");
        }
    }
    parametrization_ = QuantLib::ext::make_shared<QuantExt::CommoditySchwartzParametrization>(ccy, name, curve_, fxSpot_,
        data->sigmaValue(), data->kappaValue(),
        data->driftFreeState(),
        seasonalityTimes_a, seasonalityValues_a);
    model_ = QuantLib::ext::make_shared<QuantExt::CommoditySchwartzModel>(parametrization_);
    params_ = model_->params();
}

QuantLib::ext::shared_ptr<QuantExt::CommoditySchwartzModel> CommoditySchwartzModelBuilder::model() const {
    calculate();
    return model_;
}

Real CommoditySchwartzModelBuilder::error() const {
    calculate();
    return error_;
}

QuantLib::ext::shared_ptr<QuantExt::CommoditySchwartzParametrization> CommoditySchwartzModelBuilder::parametrization() const {
    calculate();
    return parametrization_;
}
std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> CommoditySchwartzModelBuilder::optionBasket() const {
    calculate();
    return optionBasket_;
}

bool CommoditySchwartzModelBuilder::requiresRecalibration() const {
    return (data_->calibrateSigma() || data_->calibrateKappa()) &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
}

void CommoditySchwartzModelBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        DLOG("COM model requires recalibration");
        buildOptionBasket();
    }
}

void CommoditySchwartzModelBuilder::setCalibrationDone() const {
    // reset market observer updated flag
    marketObserver_->hasUpdated(true);
    // update vol cache
    volSurfaceChanged(true);
}

Real CommoditySchwartzModelBuilder::optionStrike(const Size j) const {
    ore::data::Strike strike = ore::data::parseStrike(data_->optionStrikes()[j]);
    Real strikeValue;
    // TODO: Extend strike type coverage
    if (strike.type == ore::data::Strike::Type::ATMF)
        strikeValue = Null<Real>();
    else if (strike.type == ore::data::Strike::Type::Absolute)
        strikeValue = strike.value;
    else
        QL_FAIL("strike type ATMF or Absolute expected");
    return strikeValue;
}

Date CommoditySchwartzModelBuilder::optionExpiry(const Size j) const {
    Date today = Settings::instance().evaluationDate();
    std::string expiryString = data_->optionExpiries()[j];
    bool expiryDateBased;
    Period expiryPb;
    Date expiryDb;
    parseDateOrPeriod(expiryString, expiryDb, expiryPb, expiryDateBased);
    Date expiryDate = expiryDateBased ? expiryDb : today + expiryPb;
    return expiryDate;
}

bool CommoditySchwartzModelBuilder::volSurfaceChanged(const bool updateCache) const {
    bool hasUpdated = false;

    // if cache doesn't exist resize vector
    if (volCache_.size() != optionBasket_.size())
        volCache_ = vector<Real>(optionBasket_.size(), Null<Real>());

    Size optionCounter = 0;
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        if (!optionActive_[j])
            continue;
        Real vol = vol_->blackVol(optionExpiry(j), optionStrike(j));
        if (!close_enough(volCache_[optionCounter], vol)) {
            if (updateCache)
                volCache_[optionCounter] = vol;
            hasUpdated = true;
        }
        optionCounter++;
    }
    return hasUpdated;
}

void CommoditySchwartzModelBuilder::buildOptionBasket() const {

    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "Commodity option vector size mismatch");

    optionActive_ = std::vector<bool>(data_->optionExpiries().size(), false);

    DLOG("build reference date grid '" << referenceCalibrationGrid_ << "'");
    Date lastRefCalDate = Date::minDate();
    std::vector<Date> referenceCalibrationDates;
    if (!referenceCalibrationGrid_.empty())
        referenceCalibrationDates = DateGrid(referenceCalibrationGrid_).dates();

    std::vector<Time> expiryTimes;
    optionBasket_.clear();
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        // may wish to calibrate against specific futures expiry dates...
        Date expiryDate = optionExpiry(j);

        // check if we want to keep the helper when a reference calibration grid is given
        auto refCalDate =
            std::lower_bound(referenceCalibrationDates.begin(), referenceCalibrationDates.end(), expiryDate);
        if (refCalDate == referenceCalibrationDates.end() || *refCalDate > lastRefCalDate) {
            optionActive_[j] = true;
            Real strikeValue = optionStrike(j);
            Handle<Quote> volQuote(QuantLib::ext::make_shared<SimpleQuote>(vol_->blackVol(expiryDate, strikeValue)));
            QuantLib::ext::shared_ptr<QuantExt::FutureOptionHelper> helper = QuantLib::ext::make_shared<QuantExt::FutureOptionHelper>(
                expiryDate, strikeValue, curve_, volQuote, data_->calibrationErrorType());
            optionBasket_.push_back(helper);
            helper->performCalculations();
            expiryTimes.push_back(curve_->timeFromReference(helper->option()->exercise()->date(0)));
            DLOG("Added FutureOptionHelper " << data_->name() << " " << QuantLib::io::iso_date(expiryDate) << " "
                                             << volQuote->value());
            if (refCalDate != referenceCalibrationDates.end())
                lastRefCalDate = *refCalDate;
        } else {
            optionActive_[j] = false;
        }
    }

    std::sort(expiryTimes.begin(), expiryTimes.end());
    auto itExpiryTime = unique(expiryTimes.begin(), expiryTimes.end());
    expiryTimes.resize(distance(expiryTimes.begin(), itExpiryTime));

    optionExpiries_ = Array(expiryTimes.size());
    for (Size j = 0; j < expiryTimes.size(); j++)
        optionExpiries_[j] = expiryTimes[j];
}

void CommoditySchwartzModelBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

} // namespace data
} // namespace ore
