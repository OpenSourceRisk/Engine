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
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>

#include <ored/model/fxbsbuilder.hpp>
#include <ored/model/structuredmodelerror.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

FxBsBuilder::FxBsBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                         const QuantLib::ext::shared_ptr<FxBsData>& data, const std::string& configuration,
                         const std::string& referenceCalibrationGrid,
                         const std::string& id)
    : market_(market), configuration_(configuration), data_(data), referenceCalibrationGrid_(referenceCalibrationGrid),
      id_(id) {

    optionActive_ = std::vector<bool>(data_->optionExpiries().size(), false);
    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    QuantLib::Currency ccy = ore::data::parseCurrency(data->foreignCcy());
    QuantLib::Currency domesticCcy = ore::data::parseCurrency(data->domesticCcy());
    std::string ccyPair = ccy.code() + domesticCcy.code();

    LOG("Start building FxBs model for " << ccyPair);

    // try to get market objects, if sth fails, we fall back to a default and log a structured error

    Handle<YieldTermStructure> dummyYts(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));

    try {
        fxSpot_ = market_->fxSpot(ccyPair, configuration_);
    } catch (const std::exception& e) {
        processException("fx spot", e);
        fxSpot_ = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));
    }

    try {
        ytsDom_ = market_->discountCurve(domesticCcy.code(), configuration_);
    } catch (const std::exception& e) {
        processException("domestic discount curve", e);
        ytsDom_ = dummyYts;
    }

    try {
        ytsFor_ = market_->discountCurve(ccy.code(), configuration_);
    } catch (const std::exception& e) {
        processException("foreign discount curve", e);
        ytsFor_ = dummyYts;
    }

    // register with market observables except vols
    marketObserver_->addObservable(fxSpot_);
    marketObserver_->addObservable(ytsDom_);
    marketObserver_->addObservable(ytsFor_);

    // register the builder with the market observer
    registerWith(marketObserver_);

    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // build option basket and derive parametrization from it
    if (data->calibrateSigma()) {
        try {
            fxVol_ = market_->fxVol(ccyPair, configuration_);
        } catch (const std::exception& e) {
            processException("fx vol surface", e);
            fxVol_ = Handle<BlackVolTermStructure>(QuantLib::ext::make_shared<BlackConstantVol>(
                0, NullCalendar(), 0.0010, Actual365Fixed()));
        }
        registerWith(fxVol_);
        buildOptionBasket();
    }

    Array sigmaTimes, sigma;
    if (data->sigmaParamType() == ParamType::Constant) {
        QL_REQUIRE(data->sigmaTimes().size() == 0, "empty sigma time grid expected");
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
            sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
            sigmaTimes = Array(data_->sigmaTimes().begin(), data_->sigmaTimes().end());
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

void FxBsBuilder::processException(const std::string& s, const std::exception& e) {
    const std::string& qualifier = data_->foreignCcy() + '/' + data_->domesticCcy();
    StructuredModelErrorMessage("Error while building FX-BS model for qualifier '" + qualifier + "', context '" +
                                s + "'. Using a fallback, results depending on this object will be invalid.",
                                e.what(), id_)
        .log();
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
            QuantLib::ext::shared_ptr<QuantExt::FxEqOptionHelper> helper =
                QuantLib::ext::make_shared<QuantExt::FxEqOptionHelper>(expiryDate, strikeValue, fxSpot_, quote, ytsDom_,
                                                                       ytsFor_);
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
