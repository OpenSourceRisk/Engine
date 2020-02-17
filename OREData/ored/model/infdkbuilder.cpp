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

#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/infdkparametrization.hpp>

#include <ored/model/infdkbuilder.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace data {

InfDkBuilder::InfDkBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<InfDkData>& data,
                           const std::string& configuration)
    : market_(market), configuration_(configuration), data_(data) {

    LOG("DkBuilder for " << data_->infIndex());

    marketObserver_ = boost::make_shared<MarketObserver>();

    // get market data
    std::string infIndex = data_->infIndex();
    inflationIndex_ = boost::dynamic_pointer_cast<ZeroInflationIndex>(
        *market_->zeroInflationIndex(data_->infIndex(), configuration_));
    QL_REQUIRE(inflationIndex_, "DkBuilder: requires ZeroInflationIndex, got " << data_->infIndex());
    infPrice_ = market_->cpiInflationCapFloorPriceSurface(infIndex, configuration_);

    // register with market observables except vols
    marketObserver_->registerWith(inflationIndex_);

    // register the builder with the market observer
    registerWith(marketObserver_);

    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // build option basket and derive parametrization from it
    if (data_->calibrateA() || data_->calibrateH())
        buildCapFloorBasket();

    Array aTimes(data_->aTimes().begin(), data_->aTimes().end());
    Array hTimes(data_->hTimes().begin(), data_->hTimes().end());
    Array alpha(data_->aValues().begin(), data_->aValues().end());
    Array h(data_->hValues().begin(), data_->hValues().end());

    if (data_->aParamType() == ParamType::Constant) {
        QL_REQUIRE(data_->aTimes().size() == 0, "empty alpha times expected");
        QL_REQUIRE(data_->aValues().size() == 1, "initial alpha array should have size 1");
    } else if (data_->aParamType() == ParamType::Piecewise) {
        if (data_->calibrateA() && data_->calibrationType() == CalibrationType::Bootstrap) { // override
            if (data_->aTimes().size() > 0) {
                DLOG("overriding alpha time grid with option expiries");
            }
            QL_REQUIRE(optionExpiries_.size() > 0, "empty option expiries");
            aTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            alpha = Array(aTimes.size() + 1, data_->aValues()[0]);
        } else { // use input time grid and input alpha array otherwise
            QL_REQUIRE(alpha.size() == aTimes.size() + 1, "alpha grids do not match");
        }
    } else
        QL_FAIL("Alpha type case not covered");

    if (data_->hParamType() == ParamType::Constant) {
        QL_REQUIRE(data_->hValues().size() == 1, "reversion grid size 1 expected");
        QL_REQUIRE(data_->hTimes().size() == 0, "empty reversion time grid expected");
    } else if (data_->hParamType() == ParamType::Piecewise) {
        if (data_->calibrateH() && data_->calibrationType() == CalibrationType::Bootstrap) { // override
            if (data_->hTimes().size() > 0) {
                DLOG("overriding H time grid with option expiries");
            }
            QL_REQUIRE(optionExpiries_.size() > 0, "empty option expiries");
            hTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            h = Array(hTimes.size() + 1, data_->hValues()[0]);
        } else { // use input time grid and input alpha array otherwise
            QL_REQUIRE(h.size() == hTimes.size() + 1, "H grids do not match");
        }
    } else
        QL_FAIL("H type case not covered");

    DLOG("before calibration: alpha times = " << aTimes << " values = " << alpha);
    DLOG("before calibration:     h times = " << hTimes << " values = " << h)

    if (data_->reversionType() == LgmData::ReversionType::HullWhite &&
        data_->volatilityType() == LgmData::VolatilityType::HullWhite) {
        DLOG("INF parametrization: InfDkPiecewiseConstantHullWhiteAdaptor");
        parametrization_ = boost::make_shared<InfDkPiecewiseConstantHullWhiteAdaptor>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->infIndex());
    } else if (data_->reversionType() == LgmData::ReversionType::HullWhite) {
        DLOG("INF parametrization for " << data_->infIndex() << ": InfDkPiecewiseConstant");
        parametrization_ = boost::make_shared<InfDkPiecewiseConstantParametrization>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->infIndex());
    } else {
        parametrization_ = boost::make_shared<InfDkPiecewiseLinearParametrization>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->infIndex());
        DLOG("INF parametrization for " << data_->infIndex() << ": InfDkPiecewiseLinear");
    }

    DLOG("alpha times size: " << aTimes.size());
    DLOG("lambda times size: " << hTimes.size());

    DLOG("Apply shift horizon and scale");

    QL_REQUIRE(data_->shiftHorizon() >= 0.0, "shift horizon must be non negative");
    QL_REQUIRE(data_->scaling() > 0.0, "scaling must be positive");

    if (data_->shiftHorizon() > 0.0) {
        DLOG("Apply shift horizon " << data_->shiftHorizon() << " to the " << data_->infIndex() << " DK model");
        parametrization_->shift() = data_->shiftHorizon();
    }

    if (data_->scaling() != 1.0) {
        DLOG("Apply scaling " << data_->scaling() << " to the " << data_->infIndex() << " DK model");
        parametrization_->scaling() = data_->scaling();
    }
}

boost::shared_ptr<QuantExt::InfDkParametrization> InfDkBuilder::parametrization() const {
    calculate();
    return parametrization_;
}

std::vector<boost::shared_ptr<BlackCalibrationHelper>> InfDkBuilder::optionBasket() const {
    calculate();
    return optionBasket_;
}

bool InfDkBuilder::requiresRecalibration() const {
    return (data_->calibrateA() || data_->calibrateH()) &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
}

void InfDkBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        // update vol cache
        volSurfaceChanged(true);
        // reset market observer updated flag
        marketObserver_->hasUpdated(true);
    }
}

Real InfDkBuilder::optionStrike(const Size j) const {
    Strike strike = parseStrike(data_->optionStrikes()[j]);

    Real strikeValue;
    QL_REQUIRE(strike.type == Strike::Type::Absolute,
               "DkBuilder: only fixed strikes supported, got " << data_->optionStrikes()[j]);
    strikeValue = strike.value;
    return strikeValue;
}

Date InfDkBuilder::optionExpiry(const Size j) const {
    Date today = Settings::instance().evaluationDate();
    std::string expiryString = data_->optionExpiries()[j];
    Period expiry = parsePeriod(expiryString);
    Date expiryDate = inflationIndex_->fixingCalendar().advance(today, expiry);
    QL_REQUIRE(expiryDate > today, "expired calibration option expiry " << QuantLib::io::iso_date(expiryDate));
    return expiryDate;
}

bool InfDkBuilder::volSurfaceChanged(const bool updateCache) const {
    bool hasUpdated = false;

    // if cache doesn't exist resize vector
    if (infPriceCache_.size() != data_->optionExpiries().size())
        infPriceCache_ = vector<Real>(data_->optionExpiries().size());

    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        Real price = data_->capFloor() == "Cap" ? infPrice_->capPrice(optionExpiry(j), optionStrike(j))
                                                : infPrice_->floorPrice(optionExpiry(j), optionStrike(j));
        if (!close_enough(infPriceCache_[j], price)) {
            if (updateCache)
                infPriceCache_[j] = price;
            hasUpdated = true;
        }
    }
    return hasUpdated;
}

void InfDkBuilder::buildCapFloorBasket() const {
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "Inf option vector size mismatch");

    std::vector<Time> expiryTimes(data_->optionExpiries().size());
    optionBasket_.clear();
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        Date expiryDate = optionExpiry(j);
        Real strikeValue = optionStrike(j);

        Calendar fixCalendar = inflationIndex_->fixingCalendar();
        Date baseDate = inflationIndex_->zeroInflationTermStructure()->baseDate();
        Real baseCPI = inflationIndex_->fixing(baseDate);

        Option::Type capfloor;
        Real marketPrem;
        if (data_->capFloor() == "Cap") {
            capfloor = Option::Type::Call;
            marketPrem = infPrice_->capPrice(expiryDate, strikeValue);
        } else {
            capfloor = Option::Type::Put;
            marketPrem = infPrice_->floorPrice(expiryDate, strikeValue);
        }

        boost::shared_ptr<QuantExt::CpiCapFloorHelper> helper = boost::make_shared<QuantExt::CpiCapFloorHelper>(
            capfloor, baseCPI, expiryDate, fixCalendar, infPrice_->businessDayConvention(), fixCalendar,
            infPrice_->businessDayConvention(), strikeValue, Handle<ZeroInflationIndex>(inflationIndex_),
            infPrice_->observationLag(), marketPrem);

        optionBasket_.push_back(helper);
        helper->performCalculations();
        expiryTimes[j] = inflationYearFraction(inflationIndex_->frequency(), inflationIndex_->interpolated(),
                                               inflationIndex_->zeroInflationTermStructure()->dayCounter(), baseDate,
                                               helper->instrument()->fixingDate());
        DLOG("Added InflationOptionHelper " << data_->infIndex() << " " << QuantLib::io::iso_date(expiryDate));
    }

    std::sort(expiryTimes.begin(), expiryTimes.end());
    auto itExpiryTime = unique(expiryTimes.begin(), expiryTimes.end());
    expiryTimes.resize(distance(expiryTimes.begin(), itExpiryTime));

    optionExpiries_ = Array(expiryTimes.size());
    for (Size j = 0; j < expiryTimes.size(); j++)
        optionExpiries_[j] = expiryTimes[j];
}

void InfDkBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

} // namespace data
} // namespace ore
