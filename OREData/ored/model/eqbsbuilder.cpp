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
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

EqBsBuilder::EqBsBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<EqBsData>& data,
                         const QuantLib::Currency& baseCcy, const std::string& configuration)
    : market_(market), configuration_(configuration), data_(data), baseCcy_(baseCcy) {

    marketObserver_ = boost::make_shared<MarketObserver>();
    QuantLib::Currency ccy = ore::data::parseCurrency(data->currency());
    string eqName = data->eqName();

    // get market data
    std::string fxCcyPair = ccy.code() + baseCcy_.code();
    eqSpot_ = market_->equitySpot(eqName, configuration_);
    fxSpot_ = market_->fxSpot(fxCcyPair, configuration_);
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
        parametrization_ = boost::make_shared<QuantExt::EqBsPiecewiseConstantParametrization>(
            ccy, eqName, eqSpot_, fxSpot_, sigmaTimes, sigma, ytsRate_, ytsDiv_);
    else if (data->sigmaParamType() == ParamType::Constant)
        parametrization_ = boost::make_shared<QuantExt::EqBsConstantParametrization>(ccy, eqName, eqSpot_, fxSpot_,
                                                                                     sigma[0], ytsRate_, ytsDiv_);
    else
        QL_FAIL("interpolation type not supported for Equity");

}

Real EqBsBuilder::error() const {
    calculate();
    return error_;
}

boost::shared_ptr<QuantExt::EqBsParametrization> EqBsBuilder::parametrization() const {
    calculate();
    return parametrization_;
}
std::vector<boost::shared_ptr<BlackCalibrationHelper>> EqBsBuilder::optionBasket() const {
    calculate();
    return optionBasket_;
}

bool EqBsBuilder::requiresRecalibration() const {
    bool res = data_->calibrateSigma() &&
        (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
    return data_->calibrateSigma() &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
}

void EqBsBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        // update vol cache
        volSurfaceChanged(true);
        // reset market observer updated flag
        marketObserver_->hasUpdated(true);
    }
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
    if (eqVolCache_.size() != data_->optionExpiries().size())
        eqVolCache_ = vector<Real>(data_->optionExpiries().size());

    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        Real vol = eqVol_->blackVol(optionExpiry(j), optionStrike(j));
        if (!close_enough(eqVolCache_[j], vol)) {
            if (updateCache)
                eqVolCache_[j] = vol;
            hasUpdated = true;
        }
    }
    return hasUpdated;
}

void EqBsBuilder::buildOptionBasket() const {
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "Eq option vector size mismatch");
    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        // may wish to calibrate against specific futures expiry dates...
        Date expiryDate = optionExpiry(j);
        Real strikeValue = optionStrike(j);
        Handle<Quote> volQuote(boost::make_shared<SimpleQuote>(eqVol_->blackVol(expiryDate, strikeValue)));
        boost::shared_ptr<QuantExt::FxEqOptionHelper> helper = boost::make_shared<QuantExt::FxEqOptionHelper>(
            expiryDate, strikeValue, eqSpot_, volQuote, ytsRate_, ytsDiv_);
        optionBasket_.push_back(helper);
        helper->performCalculations();
        expiryTimes[j] = ytsRate_->timeFromReference(helper->option()->exercise()->date(0));
        LOG("Added EquityOptionHelper " << data_->eqName() << " " << QuantLib::io::iso_date(expiryDate) << " "
                                        << volQuote->value());
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
