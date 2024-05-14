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

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/quotes/simplequote.hpp>

#include <qle/models/eqbsconstantparametrization.hpp>
#include <qle/models/eqbspiecewiseconstantparametrization.hpp>
#include <qle/models/fxeqoptionhelper.hpp>

#include <ored/model/eqbsbuilder.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

EqBsBuilder::EqBsBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<EqBsData>& data,
                         const QuantLib::Currency& baseCcy, const std::string& configuration,
                         const std::string& referenceCalibrationGrid)
    : market_(market), configuration_(configuration), data_(data), referenceCalibrationGrid_(referenceCalibrationGrid),
      baseCcy_(baseCcy) {

    optionActive_ = std::vector<bool>(data_->optionExpiries().size(), false);
    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    QuantLib::Currency ccy = ore::data::parseCurrency(data->currency());
    string eqName = data->eqName();

    LOG("Start building EqBs model for " << eqName);

    // get market data
    std::string fxCcyPair = ccy.code() + baseCcy_.code();
    eqSpot_ = market_->equitySpot(eqName, configuration_);
    fxSpot_ = market_->fxRate(fxCcyPair, configuration_);
    // FIXME using the "discount curve" here instead of the equityReferenceRateCurve?
    ytsRate_ = market_->discountCurve(ccy.code(), configuration_);
    ytsDiv_ = market_->equityDividendCurve(eqName, configuration_);
    eqVol_ = market_->equityVol(eqName, configuration_);

    // register with market observables except vols
    marketObserver_->registerWith(eqSpot_);
    marketObserver_->registerWith(fxSpot_);
    marketObserver_->registerWith(ytsRate_);
    marketObserver_->registerWith(ytsDiv_);

    // register the builder with the vol and the market observer
    registerWith(eqVol_);
    registerWith(marketObserver_);

    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // build option basket and derive parametrization from it
    if (data->calibrateSigma())
        buildOptionBasket();

    Array sigmaTimes, sigma;
    if (data->sigmaParamType() == ParamType::Constant) {
        QL_REQUIRE(data->sigmaTimes().size() == 0, "empty sigma time grid expected");
        QL_REQUIRE(data->sigmaValues().size() == 1, "initial sigma grid size 1 expected");
        sigmaTimes = Array(0);
        sigma = Array(data_->sigmaValues().begin(), data_->sigmaValues().end());
    } else {
        if (data->calibrateSigma()) { // override
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

    // Quotation needs to be consistent with FX spot quotation in the FX calibration basket
    if (data->sigmaParamType() == ParamType::Piecewise)
        parametrization_ = QuantLib::ext::make_shared<QuantExt::EqBsPiecewiseConstantParametrization>(
            ccy, eqName, eqSpot_, fxSpot_, sigmaTimes, sigma, ytsRate_, ytsDiv_);
    else if (data->sigmaParamType() == ParamType::Constant)
        parametrization_ = QuantLib::ext::make_shared<QuantExt::EqBsConstantParametrization>(ccy, eqName, eqSpot_, fxSpot_,
                                                                                     sigma[0], ytsRate_, ytsDiv_);
    else
        QL_FAIL("interpolation type not supported for Equity");
}

Real EqBsBuilder::error() const {
    calculate();
    return error_;
}

QuantLib::ext::shared_ptr<QuantExt::EqBsParametrization> EqBsBuilder::parametrization() const {
    calculate();
    return parametrization_;
}
std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> EqBsBuilder::optionBasket() const {
    calculate();
    return optionBasket_;
}

bool EqBsBuilder::requiresRecalibration() const {
    return data_->calibrateSigma() &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
}

void EqBsBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        // build option basket
        buildOptionBasket();
    }
}

void EqBsBuilder::setCalibrationDone() const {
    // reset market observer updated flag
    marketObserver_->hasUpdated(true);
    // update vol cache
    volSurfaceChanged(true);
}

Real EqBsBuilder::optionStrike(const Size j) const {
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

Date EqBsBuilder::optionExpiry(const Size j) const {
    Date today = Settings::instance().evaluationDate();
    std::string expiryString = data_->optionExpiries()[j];
    bool expiryDateBased;
    Period expiryPb;
    Date expiryDb;
    parseDateOrPeriod(expiryString, expiryDb, expiryPb, expiryDateBased);
    Date expiryDate = expiryDateBased ? expiryDb : today + expiryPb;
    return expiryDate;
}

bool EqBsBuilder::volSurfaceChanged(const bool updateCache) const {
    bool hasUpdated = false;

    // if cache doesn't exist resize vector
    if (eqVolCache_.size() != optionBasket_.size())
        eqVolCache_ = vector<Real>(optionBasket_.size(), Null<Real>());

    Size optionCounter = 0;
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        if (!optionActive_[j])
            continue;
        Real vol = eqVol_->blackVol(optionExpiry(j), optionStrike(j));
        if (!close_enough(eqVolCache_[optionCounter], vol)) {
            if (updateCache)
                eqVolCache_[optionCounter] = vol;
            hasUpdated = true;
        }
        optionCounter++;
    }
    return hasUpdated;
}

void EqBsBuilder::buildOptionBasket() const {
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "Eq option vector size mismatch");

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
            Handle<Quote> volQuote(QuantLib::ext::make_shared<SimpleQuote>(eqVol_->blackVol(expiryDate, strikeValue)));
            QuantLib::ext::shared_ptr<QuantExt::FxEqOptionHelper> helper = QuantLib::ext::make_shared<QuantExt::FxEqOptionHelper>(
                expiryDate, strikeValue, eqSpot_, volQuote, ytsRate_, ytsDiv_);
            optionBasket_.push_back(helper);
            helper->performCalculations();
            expiryTimes.push_back(ytsRate_->timeFromReference(helper->option()->exercise()->date(0)));
            DLOG("Added EquityOptionHelper " << data_->eqName() << " " << QuantLib::io::iso_date(expiryDate) << " "
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

void EqBsBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

} // namespace data
} // namespace ore
