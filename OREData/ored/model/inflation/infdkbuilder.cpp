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
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/pricingengines/cpibacheliercapfloorengine.hpp>
#include <qle/utilities/inflation.hpp>

#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/model/inflation/infdkbuilder.hpp>
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

InfDkBuilder::InfDkBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<InfDkData>& data,
                           const std::string& configuration, const std::string& referenceCalibrationGrid,
                           const bool dontCalibrate)
    : market_(market), configuration_(configuration), data_(data), referenceCalibrationGrid_(referenceCalibrationGrid),
      dontCalibrate_(dontCalibrate) {

    LOG("DkBuilder for " << data_->index());

    if (!data_->calibrationBaskets().empty()) {
        QL_REQUIRE(data_->calibrationBaskets().size() == 1, "If there is a calibration basket, expect exactly 1.");
        const CalibrationBasket& cb = data_->calibrationBaskets()[0];
        QL_REQUIRE(!cb.empty(), "If there is a calibration basket, expect it to be non-empty.");
        optionActive_ = vector<bool>(cb.instruments().size(), false);
    }

    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();

    // get market data
    inflationIndex_ =
        QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndex>(*market_->zeroInflationIndex(data_->index(), configuration_));
    QL_REQUIRE(inflationIndex_, "DkBuilder: requires ZeroInflationIndex, got " << data_->index());
    rateCurve_ = market_->discountCurve(inflationIndex_->currency().code(), configuration_);
    infVol_ = market_->cpiInflationCapFloorVolatilitySurface(data_->index(), configuration_);

    // register with market observables except vols
    marketObserver_->registerWith(inflationIndex_);
    marketObserver_->registerWith(rateCurve_);

    // register the builder with the market observer
    registerWith(marketObserver_);
    registerWith(infVol_);
    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // build option basket and derive parametrization from it
    const ReversionParameter& reversion = data_->reversion();
    const VolatilityParameter& volatility = data_->volatility();
    if (volatility.calibrate() || reversion.calibrate())
        buildCapFloorBasket();

    Array aTimes(volatility.times().begin(), volatility.times().end());
    Array hTimes(reversion.times().begin(), reversion.times().end());
    Array alpha(volatility.values().begin(), volatility.values().end());
    Array h(reversion.values().begin(), reversion.values().end());

    if (volatility.type() == ParamType::Constant) {
        QL_REQUIRE(volatility.times().size() == 0, "empty alpha times expected");
        QL_REQUIRE(volatility.values().size() == 1, "initial alpha array should have size 1");
    } else if (volatility.type() == ParamType::Piecewise) {
        if (volatility.calibrate() && data_->calibrationType() == CalibrationType::Bootstrap) {
            if (volatility.times().size() > 0) {
                DLOG("overriding alpha time grid with option expiries");
            }
            QL_REQUIRE(optionExpiries_.size() > 0, "empty option expiries");
            aTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            alpha = Array(aTimes.size() + 1, volatility.values()[0]);
        } else { // use input time grid and input alpha array otherwise
            QL_REQUIRE(alpha.size() == aTimes.size() + 1, "alpha grids do not match");
        }
    } else {
        QL_FAIL("Expected DK model data volatility parameter to be Constant or Piecewise.");
    }

    if (reversion.type() == ParamType::Constant) {
        QL_REQUIRE(reversion.values().size() == 1, "reversion grid size 1 expected");
        QL_REQUIRE(reversion.times().size() == 0, "empty reversion time grid expected");
    } else if (reversion.type() == ParamType::Piecewise) {
        if (reversion.calibrate() && data_->calibrationType() == CalibrationType::Bootstrap) {
            if (reversion.times().size() > 0) {
                DLOG("overriding H time grid with option expiries");
            }
            QL_REQUIRE(optionExpiries_.size() > 0, "empty option expiries");
            hTimes = Array(optionExpiries_.begin(), optionExpiries_.end() - 1);
            h = Array(hTimes.size() + 1, reversion.values()[0]);
        } else { // use input time grid and input alpha array otherwise
            QL_REQUIRE(h.size() == hTimes.size() + 1, "H grids do not match");
        }
    } else {
        QL_FAIL("Expected DK model data reversion parameter to be Constant or Piecewise.");
    }

    DLOG("before calibration: alpha times = " << aTimes << " values = " << alpha);
    DLOG("before calibration:     h times = " << hTimes << " values = " << h)

    if (reversion.reversionType() == LgmData::ReversionType::HullWhite &&
        volatility.volatilityType() == LgmData::VolatilityType::HullWhite) {
        DLOG("INF parametrization: InfDkPiecewiseConstantHullWhiteAdaptor");
        parametrization_ = QuantLib::ext::make_shared<InfDkPiecewiseConstantHullWhiteAdaptor>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->index());
    } else if (reversion.reversionType() == LgmData::ReversionType::HullWhite) {
        DLOG("INF parametrization for " << data_->index() << ": InfDkPiecewiseConstant");
        parametrization_ = QuantLib::ext::make_shared<InfDkPiecewiseConstantParametrization>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->index());
    } else {
        parametrization_ = QuantLib::ext::make_shared<InfDkPiecewiseLinearParametrization>(
            inflationIndex_->currency(), inflationIndex_->zeroInflationTermStructure(), aTimes, alpha, hTimes, h,
            data_->index());
        DLOG("INF parametrization for " << data_->index() << ": InfDkPiecewiseLinear");
    }

    DLOG("alpha times size: " << aTimes.size());
    DLOG("lambda times size: " << hTimes.size());

    DLOG("Apply shift horizon and scale");
    Time horizon = data_->reversionTransformation().horizon();
    Real scaling = data_->reversionTransformation().scaling();
    QL_REQUIRE(horizon >= 0.0, "shift horizon must be non negative");
    QL_REQUIRE(scaling > 0.0, "scaling must be positive");

    if (horizon > 0.0) {
        DLOG("Apply shift horizon " << horizon << " to the " << data_->index() << " DK model");
        parametrization_->shift() = horizon;
    }

    if (scaling != 1.0) {
        DLOG("Apply scaling " << scaling << " to the " << data_->index() << " DK model");
        parametrization_->scaling() = scaling;
    }
}

QuantLib::ext::shared_ptr<QuantExt::InfDkParametrization> InfDkBuilder::parametrization() const {
    calculate();
    return parametrization_;
}

std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> InfDkBuilder::optionBasket() const {
    calculate();
    return optionBasket_;
}

bool InfDkBuilder::requiresRecalibration() const {
    return (data_->volatility().calibrate() || data_->reversion().calibrate()) &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
}

void InfDkBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        // build option basket
        buildCapFloorBasket();
    }
}

void InfDkBuilder::setCalibrationDone() const {
    // reset market observer updated flag
    marketObserver_->hasUpdated(true);
    // update vol cache
    volSurfaceChanged(true);
}

Date InfDkBuilder::optionMaturityDate(const Size j) const {
    Date today = Settings::instance().evaluationDate();
    const auto& ci = data_->calibrationBaskets()[0].instruments();
    QL_REQUIRE(j < ci.size(), "InfDkBuilder::optionMaturityDate(" << j << "): out of bounds, got " << ci.size()
                                                                  << " calibration instruments");
    auto cf = QuantLib::ext::dynamic_pointer_cast<CpiCapFloor>(ci.at(j));
    QL_REQUIRE(cf, "InfDkBuilder::optionMaturityDate("
                       << j << "): expected CpiCapFloor calibration instruments, could not cast");
    Date res = optionMaturity(cf->maturity(), inflationIndex_->fixingCalendar());
    QL_REQUIRE(res > today, "expired calibration option expiry " << io::iso_date(res));
    return res;
}

Real InfDkBuilder::optionStrikeValue(const Size j) const {
    const auto& ci = data_->calibrationBaskets()[0].instruments();
    QL_REQUIRE(j < ci.size(), "InfDkBuilder::optionMaturityDate(" << j << "): out of bounds, got " << ci.size()
                                                                  << " calibration instruments");
    auto cf = QuantLib::ext::dynamic_pointer_cast<CpiCapFloor>(ci.at(j));
    QL_REQUIRE(cf,
               "InfDkBuilder::optionStrike(" << j << "): expected CpiCapFloor calibration instruments, could not cast");
    return cpiCapFloorStrikeValue(cf->strike(), *inflationIndex_->zeroInflationTermStructure(), optionMaturityDate(j));
}

bool InfDkBuilder::volSurfaceChanged(const bool updateCache) const {
    bool hasUpdated = false;
    if(dontCalibrate_)
        return false;

    QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine> engine;

    bool isLogNormalVol = QuantExt::ZeroInflation::isCPIVolSurfaceLogNormal(infVol_.currentLink());
    if (isLogNormalVol) {
        engine = QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(rateCurve_, infVol_);
    } else {
        engine = QuantLib::ext::make_shared<QuantExt::CPIBachelierCapFloorEngine>(rateCurve_, infVol_);
    }

    Calendar fixCalendar = inflationIndex_->fixingCalendar();
    BusinessDayConvention bdc = infVol_->businessDayConvention();
    Date baseDate = inflationIndex_->zeroInflationTermStructure()->baseDate();
    Real baseCPI = inflationIndex_->fixing(baseDate);
    Period lag = infVol_->observationLag();

    // if cache doesn't exist resize vector
    if (infPriceCache_.size() != optionBasket_.size())
        infPriceCache_ = vector<Real>(optionBasket_.size(), Null<Real>());

    // Handle on calibration instruments. No checks this time.
    const auto& ci = data_->calibrationBaskets()[0].instruments();

    Real nominal = 1.0;
    Size optionCounter = 0;
    Date today = Settings::instance().evaluationDate();
    for (Size j = 0; j < ci.size(); j++) {

        if (!optionActive_[j])
            continue;

        auto cpiCapFloor = QuantLib::ext::dynamic_pointer_cast<CpiCapFloor>(ci[j]);
        QL_REQUIRE(cpiCapFloor, "Expected CpiCapFloor calibration instruments in DK inflation model data.");

        Date expiryDate = optionMaturityDate(j);
        auto strikeValue = optionStrikeValue(j);

        Option::Type capfloor = cpiCapFloor->type() == CapFloor::Cap ? Option::Call : Option::Put;

        QuantLib::ext::shared_ptr<CPICapFloor> h =
            QuantLib::ext::make_shared<CPICapFloor>(capfloor, nominal, today, baseCPI, expiryDate, fixCalendar, bdc,
                                            fixCalendar, bdc, strikeValue, inflationIndex_, lag);
        h->setPricingEngine(engine);
        Real price = h->NPV();
        if (!close_enough(infPriceCache_[optionCounter], price)) {
            if (updateCache)
                infPriceCache_[optionCounter] = price;
            hasUpdated = true;
        }
        optionCounter++;
    }
    return hasUpdated;
}

void InfDkBuilder::buildCapFloorBasket() const {

    // Checks that there is a basket.
    QL_REQUIRE(!data_->calibrationBaskets().empty(), "No calibration basket provided in inflation DK model data.");
    const CalibrationBasket& cb = data_->calibrationBaskets()[0];
    const auto& ci = cb.instruments();

    QL_REQUIRE(ci.size() == optionActive_.size(), "Expected the option active vector size to equal the "
                                                      << "number of calibration instruments");
    fill(optionActive_.begin(), optionActive_.end(), false);

    DLOG("build reference date grid '" << referenceCalibrationGrid_ << "'");
    Date lastRefCalDate = Date::minDate();
    std::vector<Date> referenceCalibrationDates;
    if (!referenceCalibrationGrid_.empty())
        referenceCalibrationDates = DateGrid(referenceCalibrationGrid_).dates();

    QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine> engine;

    bool isLogNormalVol = QuantExt::ZeroInflation::isCPIVolSurfaceLogNormal(infVol_.currentLink());
    if (isLogNormalVol) {
        engine = QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(rateCurve_, infVol_);
    } else {
        engine = QuantLib::ext::make_shared<QuantExt::CPIBachelierCapFloorEngine>(rateCurve_, infVol_);
    }
       
    Calendar fixCalendar = inflationIndex_->fixingCalendar();
    Date baseDate = inflationIndex_->zeroInflationTermStructure()->baseDate();
    Real baseCPI = dontCalibrate_ ? 100. : inflationIndex_->fixing(baseDate);
    BusinessDayConvention bdc = infVol_->businessDayConvention();
    Period lag = infVol_->observationLag();
    Handle<ZeroInflationIndex> hIndex(inflationIndex_);
    Date startDate = Settings::instance().evaluationDate();
    bool useInterpolatedCPIFixings = infVol_->indexIsInterpolated();
    Real nominal = 1.0;
    vector<Time> expiryTimes;
    optionBasket_.clear();
    for (Size j = 0; j < ci.size(); j++) {

        auto cpiCapFloor = QuantLib::ext::dynamic_pointer_cast<CpiCapFloor>(ci[j]);
        QL_REQUIRE(cpiCapFloor, "Expected CpiCapFloor calibration instruments in DK inflation model data.");

        Date expiryDate = optionMaturityDate(j);

        // check if we want to keep the helper when a reference calibration grid is given
        auto refCalDate =
            std::lower_bound(referenceCalibrationDates.begin(), referenceCalibrationDates.end(), expiryDate);
        if (refCalDate == referenceCalibrationDates.end() || *refCalDate > lastRefCalDate) {
            Real strikeValue = optionStrikeValue(j);
            Option::Type capfloor = cpiCapFloor->type() == CapFloor::Cap ? Option::Call : Option::Put;
            QuantLib::ext::shared_ptr<CPICapFloor> cf =
                QuantLib::ext::make_shared<CPICapFloor>(capfloor, nominal, startDate, baseCPI, expiryDate, fixCalendar, bdc,
                                                fixCalendar, bdc, strikeValue, inflationIndex_, lag);
            cf->setPricingEngine(engine);
            Real tte = inflationYearFraction(inflationIndex_->frequency(), useInterpolatedCPIFixings,
                                             inflationIndex_->zeroInflationTermStructure()->dayCounter(), baseDate,
                                             cf->fixingDate());

            Real tteFromBase = infVol_->timeFromBase(expiryDate);

            Real marketPrem;
            if (dontCalibrate_)
                marketPrem = 0.1;
            else if (tte <= 0 || tteFromBase <= 0)
                marketPrem = 0.00;
            else
                marketPrem = cf->NPV();

            QuantLib::ext::shared_ptr<QuantExt::CpiCapFloorHelper> helper =
                QuantLib::ext::make_shared<QuantExt::CpiCapFloorHelper>(capfloor, baseCPI, expiryDate, fixCalendar, bdc,
                                                                        fixCalendar, bdc, strikeValue, hIndex, lag,
                                                                        marketPrem);

            // we might produce duplicate expiry times even if the fixing dates are all different
            if (marketPrem > 0.0 && tte > 0 && tteFromBase > 0 &&
                std::find_if(expiryTimes.begin(), expiryTimes.end(),
                             [tte](Real x) { return QuantLib::close_enough(x, tte); }) == expiryTimes.end()) {
                optionBasket_.push_back(helper);
                helper->performCalculations();
                expiryTimes.push_back(tte);
                DLOG("Added InflationOptionHelper index="
                     << data_->index() << ", type=" << (capfloor == Option::Type::Call ? "Cap" : "Floor") << ", expiry="
                     << QuantLib::io::iso_date(expiryDate) << ", baseCPI=" << baseCPI << ", strike=" << strikeValue
                     << ", lag=" << lag << ", marketPremium=" << marketPrem << ", tte=" << tte);
                optionActive_[j] = true;
                if (refCalDate != referenceCalibrationDates.end())
                    lastRefCalDate = *refCalDate;
            } else {
                if (data_->ignoreDuplicateCalibrationExpiryTimes()) {
                    DLOG("Skipped InflationOptionHelper index="
                         << data_->index() << ", type=" << (capfloor == Option::Type::Call ? "Cap" : "Floor")
                         << ", expiry=" << QuantLib::io::iso_date(expiryDate) << ", baseCPI=" << baseCPI
                         << ", strike=" << strikeValue << ", lag=" << lag << ", marketPremium=" << marketPrem
                         << ", tte=" << tte << " since we already have a helper with the same expiry time.");
                } else {
                    QL_FAIL("InfDkBuilder: a CPI cap floor calibration instrument with the expiry time, "
                            << tte << ", was already added.");
                }
            }
        }
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
