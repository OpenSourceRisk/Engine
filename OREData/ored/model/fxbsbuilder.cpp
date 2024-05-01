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

#include <ql/experimental/fx/blackdeltacalculator.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/quotes/simplequote.hpp>

#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>

#include <ored/model/fxbsbuilder.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

FxBsBuilder::FxBsBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<FxBsData>& data,
                         const std::string& configuration, const std::string& referenceCalibrationGrid)
    : market_(market), configuration_(configuration), data_(data), referenceCalibrationGrid_(referenceCalibrationGrid) {

    optionActive_ = std::vector<bool>(data_->optionExpiries().size(), false);
    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    QuantLib::Currency ccy = ore::data::parseCurrency(data->foreignCcy());
    QuantLib::Currency domesticCcy = ore::data::parseCurrency(data->domesticCcy());
    std::string ccyPair = ccy.code() + domesticCcy.code();

    LOG("Start building FxBs model for " << ccyPair);

    // get market data
    fxSpot_ = market_->fxSpot(ccyPair, configuration_);
    ytsDom_ = market_->discountCurve(domesticCcy.code(), configuration_);
    ytsFor_ = market_->discountCurve(ccy.code(), configuration_);

    // register with market observables except vols
    marketObserver_->addObservable(fxSpot_);
    marketObserver_->addObservable(market_->discountCurve(domesticCcy.code()));
    marketObserver_->addObservable(market_->discountCurve(ccy.code()));

    // register the builder with the market observer
    registerWith(marketObserver_);

    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // build option basket and derive parametrization from it
    if (data->calibrateSigma()) {
        fxVol_ = market_->fxVol(ccyPair, configuration_);
        registerWith(fxVol_);
        buildOptionBasket();
    }

    Array sigmaTimes, sigma;
    if (data->sigmaParamType() == ParamType::Constant) {
        QL_REQUIRE(data->sigmaTimes().size() == 0, "empty sigma tme grid expected");
        QL_REQUIRE(data->sigmaValues().size() == 1, "initial sigma grid size 1 expected");
        sigmaTimes = Array(0);
        sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
    } else {
        if (data->calibrateSigma() && data->calibrationType() == CalibrationType::Bootstrap) { // override
            QL_REQUIRE(optionExpiries_.size() > 0, "optionExpiries is empty");
            sigmaTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            sigma = Array(sigmaTimes.size() + 1, data->sigmaValues()[0]);
        } else {
            // use input time grid and input alpha array otherwise
            sigmaTimes = Array(data_->sigmaTimes().begin(), data_->sigmaTimes().end());
            sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
            QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1, "sigma grids do not match");
        }
    }

    DLOG("sigmaTimes before calibration: " << sigmaTimes);
    DLOG("sigma before calibration: " << sigma);

    if (data->sigmaParamType() == ParamType::Piecewise)
        parametrization_ =
            QuantLib::ext::make_shared<QuantExt::FxBsPiecewiseConstantParametrization>(ccy, fxSpot_, sigmaTimes, sigma);
    else if (data->sigmaParamType() == ParamType::Constant)
        parametrization_ = QuantLib::ext::make_shared<QuantExt::FxBsConstantParametrization>(ccy, fxSpot_, sigma[0]);
    else
        QL_FAIL("interpolation type not supported for FX");
}

Real FxBsBuilder::error() const {
    calculate();
    return error_;
}

QuantLib::ext::shared_ptr<QuantExt::FxBsParametrization> FxBsBuilder::parametrization() const {
    calculate();
    return parametrization_;
}
std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> FxBsBuilder::optionBasket() const {
    calculate();
    return optionBasket_;
}

bool FxBsBuilder::requiresRecalibration() const {
    return data_->calibrateSigma() &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
}

void FxBsBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        // build option basket
        buildOptionBasket();
    }
}

void FxBsBuilder::setCalibrationDone() const {
    // reset market observer updated flag
    marketObserver_->hasUpdated(true);
    // update vol cache
    volSurfaceChanged(true);
}

Real FxBsBuilder::optionStrike(const Size j) const {
    Date expiryDate = optionExpiry(j);
    ore::data::Strike strike = ore::data::parseStrike(data_->optionStrikes()[j]);
    Real strikeValue;
    BlackDeltaCalculator bdc(Option::Type::Call, DeltaVolQuote::DeltaType::Spot, fxSpot_->value(),
                             ytsDom_->discount(expiryDate), ytsFor_->discount(expiryDate),
                             fxVol_->blackVol(expiryDate, Null<Real>()) * sqrt(fxVol_->timeFromReference(expiryDate)));

    // TODO: Extend strike type coverage
    if (strike.type == ore::data::Strike::Type::ATMF)
        strikeValue = bdc.atmStrike(DeltaVolQuote::AtmFwd);
    else if (strike.type == ore::data::Strike::Type::Absolute)
        strikeValue = strike.value;
    else
        QL_FAIL("strike type ATMF or Absolute expected");
    Handle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(fxVol_->blackVol(expiryDate, strikeValue)));
    return strikeValue;
}

Date FxBsBuilder::optionExpiry(const Size j) const {
    Date today = Settings::instance().evaluationDate();
    std::string expiryString = data_->optionExpiries()[j];
    bool expiryDateBased;
    Period expiryPb;
    Date expiryDb;
    parseDateOrPeriod(expiryString, expiryDb, expiryPb, expiryDateBased);
    Date expiryDate = expiryDateBased ? expiryDb : today + expiryPb;
    return expiryDate;
}

bool FxBsBuilder::volSurfaceChanged(const bool updateCache) const {
    bool hasUpdated = false;

    // if cache doesn't exist resize vector
    if (fxVolCache_.size() != optionBasket_.size())
        fxVolCache_ = vector<Real>(optionBasket_.size(), Null<Real>());

    Size optionCounter = 0;
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        if (!optionActive_[j])
            continue;
        Real vol = fxVol_->blackVol(optionExpiry(j), optionStrike(j));
        if (!close_enough(fxVolCache_[optionCounter], vol)) {
            if (updateCache)
                fxVolCache_[optionCounter] = vol;
            hasUpdated = true;
        }
        optionCounter++;
    }
    return hasUpdated;
}

void FxBsBuilder::buildOptionBasket() const {
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "fx option vector size mismatch");

    DLOG("build reference date grid '" << referenceCalibrationGrid_ << "'");
    Date lastRefCalDate = Date::minDate();
    std::vector<Date> referenceCalibrationDates;
    if (!referenceCalibrationGrid_.empty())
        referenceCalibrationDates = DateGrid(referenceCalibrationGrid_).dates();

    optionBasket_.clear();
    std::vector<Time> expiryTimes;
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        Date expiryDate = optionExpiry(j);

        // check if we want to keep the helper when a reference calibration grid is given
        auto refCalDate =
            std::lower_bound(referenceCalibrationDates.begin(), referenceCalibrationDates.end(), expiryDate);
        if (refCalDate == referenceCalibrationDates.end() || *refCalDate > lastRefCalDate) {
            optionActive_[j] = true;
            Real strikeValue = optionStrike(j);
            Handle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(fxVol_->blackVol(expiryDate, strikeValue)));
            QuantLib::ext::shared_ptr<QuantExt::FxEqOptionHelper> helper = QuantLib::ext::make_shared<QuantExt::FxEqOptionHelper>(
                expiryDate, strikeValue, fxSpot_, quote, ytsDom_, ytsFor_);
            optionBasket_.push_back(helper);
            helper->performCalculations();
            expiryTimes.push_back(ytsDom_->timeFromReference(helper->option()->exercise()->date(0)));
            DLOG("Added FxEqOptionHelper " << (data_->foreignCcy() + data_->domesticCcy()) << " "
                                           << QuantLib::io::iso_date(expiryDate) << " " << helper->strike() << " "
                                           << quote->value());
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

void FxBsBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}
} // namespace data
} // namespace ore
