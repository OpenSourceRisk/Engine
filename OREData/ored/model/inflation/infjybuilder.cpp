/*
 Copyright (C) 2020, 2022 Quaternion Risk Management Ltd
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

#include <ored/model/inflation/infjybuilder.hpp>
#include <ored/model/calibrationinstruments/cpicapfloor.hpp>
#include <ored/model/calibrationinstruments/yoycapfloor.hpp>
#include <ored/model/calibrationinstruments/yoyswap.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/infjyparameterization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/yoycapfloorhelper.hpp>
#include <qle/models/yoyswaphelper.hpp>
#include <qle/pricingengines/inflationcapfloorengines.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <qle/pricingengines/cpibacheliercapfloorengine.hpp>
#include <qle/utilities/inflation.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <boost/range/join.hpp>

using QuantExt::CPIBlackCapFloorEngine;
using QuantExt::CpiCapFloorHelper;
using QuantExt::FxBsParametrization;
using QuantExt::inflationTime;
using QuantExt::Lgm1fParametrization;
using QuantExt::YoYCapFloorHelper;
using QuantExt::YoYInflationUnitDisplacedBlackCapFloorEngine;
using QuantExt::YoYInflationBachelierCapFloorEngine;
using QuantExt::YoYInflationBlackCapFloorEngine;
using QuantExt::YoYSwapHelper;
using QuantLib::DiscountingSwapEngine;
using QuantLib::Thirty360;
using QuantLib::YoYInflationCoupon;
using std::lower_bound;
using std::set;
using std::string;

namespace {

// Used as comparator in various sets below.
struct CloseCmp {
    bool operator() (QuantLib::Time s, QuantLib::Time t) const {
        return s < t && !QuantLib::close(s, t);
    }
};

}

namespace ore {
namespace data {

using Helpers = InfJyBuilder::Helpers;

InfJyBuilder::InfJyBuilder(const QuantLib::ext::shared_ptr<Market>& market, const QuantLib::ext::shared_ptr<InfJyData>& data,
                           const string& configuration, const string& referenceCalibrationGrid,
                           const bool dontCalibrate)
    : market_(market), configuration_(configuration), data_(data), referenceCalibrationGrid_(referenceCalibrationGrid),
      dontCalibrate_(dontCalibrate), marketObserver_(QuantLib::ext::make_shared<MarketObserver>()),
      zeroInflationIndex_(*market_->zeroInflationIndex(data_->index(), configuration_)) {

    LOG("InfJyBuilder: building model for inflation index " << data_->index());

    // Get rate curve
    rateCurve_ = market_->discountCurve(zeroInflationIndex_->currency().code(), configuration_);

    // Register with market observables except volatilities
    marketObserver_->registerWith(zeroInflationIndex_);
    marketObserver_->registerWith(rateCurve_);
    initialiseMarket();

    // Register the model builder with the market observer
    registerWith(marketObserver_);

    // Notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // Build the calibration instruments
    buildCalibrationBaskets();

    // Create the JY parameterisation.
    parameterization_ = QuantLib::ext::make_shared<QuantExt::InfJyParameterization>(
        createRealRateParam(), createIndexParam(), zeroInflationIndex_);
}

string InfJyBuilder::inflationIndex() const {
    return data_->index();
}

QuantLib::ext::shared_ptr<QuantExt::InfJyParameterization> InfJyBuilder::parameterization() const {
    calculate();
    return parameterization_;
}

Helpers InfJyBuilder::realRateBasket() const {
    calculate();
    return realRateBasket_;
}

Helpers InfJyBuilder::indexBasket() const {
    calculate();
    return indexBasket_;
}

bool InfJyBuilder::requiresRecalibration() const {
    return (data_->realRateVolatility().calibrate() || data_->realRateReversion().calibrate() ||
            data_->indexVolatility().calibrate()) &&
           (marketObserver_->hasUpdated(false) || forceCalibration_ || pricesChanged(false));
}

void InfJyBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        buildCalibrationBaskets();
    }
}

void InfJyBuilder::setCalibrationDone() const {
    marketObserver_->hasUpdated(true);
    pricesChanged(true);
}

void InfJyBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

void InfJyBuilder::buildCalibrationBaskets() const {

    // If calibration type is None, don't build any baskets.
    if (data_->calibrationType() == CalibrationType::None) {
        DLOG("InfJyBuilder: calibration type is None so no calibration baskets built.");
        return;
    }

    const auto& cbs = data_->calibrationBaskets();

    // If calibration type is BestFit, check that we have at least one calibration basket. Build up to a maximum of 
    // two calibration baskets. Log a warning if more than two are given. Arbitrarily assign the built baskets to 
    // the realRateBasket_ and indexBasket_ members. They will be combined again in any case for BestFit calibration.
    if (data_->calibrationType() == CalibrationType::BestFit) {
        QL_REQUIRE(cbs.size() > 0, "InfJyBuilder: calibration type is BestFit but no calibration baskets provided.");
        rrInstActive_ = vector<bool>(cbs[0].instruments().size(), false);
        realRateBasket_ = buildCalibrationBasket(cbs[0], rrInstActive_, rrInstExpiries_);
        if (cbs.size() > 1) {
            indexInstActive_ = vector<bool>(cbs[1].instruments().size(), false);
            indexBasket_ = buildCalibrationBasket(cbs[1], indexInstActive_, indexInstExpiries_);
        }
        if (cbs.size() > 2)
            WLOG("InfJyBuilder: only 2 calibration baskets can be processed but " << cbs.size() <<
                " were supplied. The extra baskets are ignored.");
        return;
    }

    // Make sure that the calibration type is now Bootstrap.
    QL_REQUIRE(data_->calibrationType() == CalibrationType::Bootstrap, "InfJyBuilder: expected the calibration " <<
        "type to be one of None, BestFit or Bootstrap.");

    const VolatilityParameter& idxVolatility = data_->indexVolatility();
    const ReversionParameter& rrReversion = data_->realRateReversion();
    const VolatilityParameter& rrVolatility = data_->realRateVolatility();

    // Firstly, look at the inflation index portion i.e. are we calibrating it.
    if (idxVolatility.calibrate()) {

        DLOG("InfJyBuilder: building calibration basket for JY index bootstrap calibration.");

        // If we are not calibrating the real rate portion, then we expect exactly one calibration basket. Otherwise 
        // we need to find a basket with the 'Index' parameter.
        if (!rrReversion.calibrate() && !rrVolatility.calibrate()) {
            QL_REQUIRE(cbs.size() == 1, "InfJyBuilder: calibrating only JY index volatility using Bootstrap so " <<
                "expected exactly one basket but got " << cbs.size() << ".");
            const auto& cb = cbs[0];
            if (!cb.parameter().empty() && cb.parameter() != "Index") {
                WLOG("InfJyBuilder: calibrating only JY index volatility using Bootstrap so expected the " <<
                    "calibration basket parameter to be 'Index' but got '" << cb.parameter() << "'.");
            }
            indexInstActive_ = vector<bool>(cb.instruments().size(), false);
            indexBasket_ = buildCalibrationBasket(cb, indexInstActive_, indexInstExpiries_);
        } else {
            DLOG("InfJyBuilder: need a calibration basket with parameter equal to 'Index'.");
            const auto& cb = calibrationBasket("Index");
            indexInstActive_ = vector<bool>(cb.instruments().size(), false);
            indexBasket_ = buildCalibrationBasket(cb, indexInstActive_, indexInstExpiries_);
        }
    }

    // Secondly, look at the real rate portion i.e. are we calibrating it.
    if (rrReversion.calibrate() || rrVolatility.calibrate()) {

        DLOG("InfJyBuilder: building calibration basket for JY real rate bootstrap calibration.");
        QL_REQUIRE(!(rrReversion.calibrate() && rrVolatility.calibrate()), "InfJyBuilder: calibrating both the " <<
            "real rate reversion and real rate volatility using Bootstrap is not supported.");

        // If we are not calibrating the index portion, then we expect exactly one calibration basket. Otherwise 
        // we need to find a basket with the 'RealRate' parameter.
        if (!idxVolatility.calibrate()) {
            QL_REQUIRE(cbs.size() == 1, "InfJyBuilder: calibrating only JY real rate using Bootstrap so " <<
                "expected exactly one basket but got " << cbs.size() << ".");
            const auto& cb = cbs[0];
            if (!cb.parameter().empty() && cb.parameter() != "RealRate") {
                WLOG("InfJyBuilder: calibrating only JY real rate using Bootstrap so expected the " <<
                    "calibration basket parameter to be 'RealRate' but got '" << cb.parameter() << "'.");
            }
            rrInstActive_ = vector<bool>(cb.instruments().size(), false);
            realRateBasket_ = buildCalibrationBasket(cb, rrInstActive_, rrInstExpiries_, rrReversion.calibrate());
        } else {
            DLOG("InfJyBuilder: need a calibration basket with parameter equal to 'RealRate'.");
            const auto& cb = calibrationBasket("RealRate");
            rrInstActive_ = vector<bool>(cb.instruments().size(), false);
            realRateBasket_ = buildCalibrationBasket(cb, rrInstActive_, rrInstExpiries_, rrReversion.calibrate());
        }
    }

}

Helpers InfJyBuilder::buildCalibrationBasket(const CalibrationBasket& cb,
    vector<bool>& active, Array& expiries, bool forRealRateReversion) const {

    QL_REQUIRE(!cb.empty(), "InfJyBuilder: calibration basket should not be empty.");

    const auto& ci = cb.instruments();
    QL_REQUIRE(ci.size() == active.size(), "InfJyBuilder: expected the active instruments vector " <<
        "size to equal the number of calibration instruments");
    fill(active.begin(), active.end(), false);

    if (cb.instrumentType() == "CpiCapFloor") {
        return buildCpiCapFloorBasket(cb, active, expiries);
    } else if (cb.instrumentType() == "YoYCapFloor") {
        return buildYoYCapFloorBasket(cb, active, expiries);
    } else if (cb.instrumentType() == "YoYSwap") {
        return buildYoYSwapBasket(cb, active, expiries, forRealRateReversion);
    } else {
        QL_FAIL("InfJyBuilder: expected calibration instrument to be one of CpiCapFloor, YoYCapFloor or YoYSwap");
    }
}

Helpers InfJyBuilder::buildCpiCapFloorBasket(const CalibrationBasket& cb,
    vector<bool>& active, Array& expiries) const {

    DLOG("InfJyBuilder: start building the CPI cap floor calibration basket.");

    QL_REQUIRE(!cpiVolatility_.empty(), "InfJyBuilder: need a non-empty CPI cap floor volatility structure " <<
        "to build a CPI cap floor calibration basket.");

    // Procedure is to create a CPI cap floor as described by each instrument in the calibration basket. We then value 
    // each of the CPI cap floor instruments using market data and an engine and pass the NPV as the market premium to 
    // helper that we create.

    Helpers helpers;

    // Create the engine
    auto zts = zeroInflationIndex_->zeroInflationTermStructure();
    
    QuantLib::ext::shared_ptr<QuantExt::CPICapFloorEngine> engine;
    bool isLogNormalVol = QuantExt::ZeroInflation::isCPIVolSurfaceLogNormal(cpiVolatility_.currentLink());
    if (isLogNormalVol) {
        engine = QuantLib::ext::make_shared<QuantExt::CPIBlackCapFloorEngine>(rateCurve_, cpiVolatility_);
    } else {
        engine = QuantLib::ext::make_shared<QuantExt::CPIBachelierCapFloorEngine>(rateCurve_, cpiVolatility_);
    }
    // CPI cap floor calibration instrument details. Assumed to equal those from the index and market structures.
    // Some of these should possibly come from conventions.
    // Also some variables used in the loop below.
    auto calendar = zeroInflationIndex_->fixingCalendar();
    auto baseDate = zts->baseDate();
    auto baseCpi = dontCalibrate_ ? 100.0 : zeroInflationIndex_->fixing(baseDate);
    auto bdc = cpiVolatility_->businessDayConvention();
    auto obsLag = cpiVolatility_->observationLag();
    
    Handle<ZeroInflationIndex> inflationIndex(zeroInflationIndex_);
    Date today = Settings::instance().evaluationDate();
    Real nominal = 1.0;

    // Avoid instruments with duplicate expiry times in the loop below
    set<Time, CloseCmp> expiryTimes;

    // Reference calibration dates if any. If they are given, we only include one calibration instrument from each 
    // period in the grid. Logic copied from other builders.
    auto rcDates = referenceCalibrationDates();
    auto prevRcDate = Date::minDate();

    // Add calibration instruments to the helpers vector.
    const auto& ci = cb.instruments();

    auto observationInterpolation = cpiVolatility_->indexIsInterpolated() ? CPI::Linear : CPI::Flat;

    for (Size i = 0; i < ci.size(); ++i) {

        auto cpiCapFloor = QuantLib::ext::dynamic_pointer_cast<CpiCapFloor>(ci[i]);
        QL_REQUIRE(cpiCapFloor, "InfJyBuilder: expected CpiCapFloor calibration instrument.");
        auto maturity = optionMaturity(cpiCapFloor->maturity(), calendar);

        // Deal with reference calibration date grid stuff.
        auto rcDate = lower_bound(rcDates.begin(), rcDates.end(), maturity);
        if (!(rcDate == rcDates.end() || *rcDate > prevRcDate)) {
            active[i] = false;
            continue;
        }

        if (rcDate != rcDates.end())
            prevRcDate = *rcDate;

        // Build the CPI calibration instrument in order to calculate its NPV.

        /* FIXME - the maturity date is not adjusted on eval date changes even if given as a tenor
                 - if the strike is atm, the value will not be updated on eval date changes */
        Real strikeValue =
            cpiCapFloorStrikeValue(cpiCapFloor->strike(), *zeroInflationIndex_->zeroInflationTermStructure(), maturity);
        Option::Type capfloor = cpiCapFloor->type() == CapFloor::Cap ? Option::Call : Option::Put;
        auto inst = QuantLib::ext::make_shared<CPICapFloor>(capfloor, nominal, today, baseCpi, maturity, calendar, bdc, calendar, bdc,
                                            strikeValue, zeroInflationIndex_, obsLag, observationInterpolation);
        inst->setPricingEngine(engine);

        auto fixingDate = inst->fixingDate();
        auto t = inflationTime(fixingDate, *zts, false);

        // Build the helper using the NPV as the premium.
        Real premium;
        if(dontCalibrate_)
            premium = 0.01;
        else if(t <= 0.0)
            premium = 0.0;
        else
            premium = inst->NPV();

        auto helper = QuantLib::ext::make_shared<CpiCapFloorHelper>(capfloor, baseCpi, maturity, calendar, bdc, calendar, bdc,
                                                            strikeValue, inflationIndex, obsLag, premium,
                                                            observationInterpolation);

        // if time is not positive or market prem is zero deactivate helper
        if (t < 0.0 || QuantLib::close_enough(t, 0.0) || QuantLib::close_enough(premium, 0.0)) {
            active[i] = false;
            continue;
        }

        auto p = expiryTimes.insert(t);
        QL_REQUIRE(data_->ignoreDuplicateCalibrationExpiryTimes() || p.second,
                   "InfJyBuilder: a CPI cap floor calibration "
                       << "instrument with the expiry time, " << t << ", was already added.");

        if(p.second)
            helpers.push_back(helper);

        TLOG("InfJyBuilder: " << (p.second ?
                                  "added CPICapFloor helper" :
                                  "skipped CPICapFloor helper due to duplicate expiry time (" + std::to_string(t) + ")") <<
             ": index = " << data_->index() <<
             ", type = " << cpiCapFloor->type() <<
             ", expiry = " << io::iso_date(maturity) <<
             ", base CPI = " << baseCpi <<
             ", strike = " << strikeValue <<
             ", obs lag = " << obsLag <<
             ", market premium = " << premium);
    }

    // Populate the expiry times array with the unique sorted expiry times.
    expiries = Array(expiryTimes.begin(), expiryTimes.end());

    DLOG("InfJyBuilder: finished building the CPI cap floor calibration basket.");

    return helpers;
}

Helpers InfJyBuilder::buildYoYCapFloorBasket(const CalibrationBasket& cb, vector<bool>& active, Array& expiries) const {

    DLOG("InfJyBuilder: start building the YoY cap floor calibration basket.");

    // Initial checks.
    QL_REQUIRE(yoyInflationIndex_, "InfJyBuilder: need a valid year on year inflation index "
                                       << "to build a year on year cap floor calibration basket.");
    auto yoyTs = yoyInflationIndex_->yoyInflationTermStructure();
    QL_REQUIRE(!yoyTs.empty(), "InfJyBuilder: need a valid year on year term structure "
                                   << "to build a year on year cap floor calibration basket.");
    QL_REQUIRE(!yoyVolatility_.empty(), "InfJyBuilder: need a valid year on year volatility "
                                            << "structure to build a year on year cap floor calibration basket.");

    // Procedure is to create a YoY cap floor as described by each instrument in the calibration basket. We then value
    // each of the YoY cap floor instruments using market data and an engine and pass the NPV as the market premium to
    // helper that we create.

    Helpers helpers;

    // Create the engine which depends on the type of the YoY volatility and the shift.
    QuantLib::ext::shared_ptr<PricingEngine> engine;
    auto ovsType = yoyVolatility_->volatilityType();
    if (ovsType == Normal)
        engine = QuantLib::ext::make_shared<YoYInflationBachelierCapFloorEngine>(yoyInflationIndex_, yoyVolatility_, rateCurve_);
    else if (ovsType == ShiftedLognormal && close(yoyVolatility_->displacement(), 0.0))
        engine = QuantLib::ext::make_shared<YoYInflationBlackCapFloorEngine>(yoyInflationIndex_, yoyVolatility_, rateCurve_);
    else if (ovsType == ShiftedLognormal)
        engine =
            QuantLib::ext::make_shared<YoYInflationUnitDisplacedBlackCapFloorEngine>(yoyInflationIndex_, yoyVolatility_, rateCurve_);
    else
        QL_FAIL("InfJyBuilder: can't create engine with yoy volatility type, " << ovsType << ".");

    // YoY cap floor calibration instrument details. Assumed to equal those from the index and market structures.
    // Some of these should possibly come from conventions. Also some variables used in the loop below.
    Natural settlementDays = 2;
    auto calendar = yoyInflationIndex_->fixingCalendar();
    DayCounter dc = Thirty360(Thirty360::BondBasis);
    auto bdc = Following;
    auto obsLag = yoyVolatility_->observationLag();

    // Avoid instruments with duplicate expiry times in the loop below
    set<Time, CloseCmp> expiryTimes;

    // Reference calibration dates if any. If they are given, we only include one calibration instrument from each 
    // period in the grid. Logic copied from other builders.
    auto rcDates = referenceCalibrationDates();
    auto prevRcDate = Date::minDate();

    // Add calibration instruments to the helpers vector.
    const auto& ci = cb.instruments();
    for (Size i = 0; i < ci.size(); ++i) {

        auto yoyCapFloor = QuantLib::ext::dynamic_pointer_cast<YoYCapFloor>(ci[i]);
        QL_REQUIRE(yoyCapFloor, "InfJyBuilder: expected YoYCapFloor calibration instrument.");

        /*! Get the configured strike.
            FIXME If the strike is atm, the value will not be updated on evaluation date changes */
        Date today = Settings::instance().evaluationDate();
        Date maturityDate = calendar.advance(calendar.advance(today, settlementDays * Days), yoyCapFloor->tenor(), bdc);
        Real strikeValue = yoyCapFloorStrikeValue(yoyCapFloor->strike(), *yoyTs, maturityDate);

        // Build the YoY cap floor helper.
        auto quote = QuantLib::ext::make_shared<SimpleQuote>(0.01);
        auto helper = QuantLib::ext::make_shared<YoYCapFloorHelper>(Handle<Quote>(quote), yoyCapFloor->type(), strikeValue,
            settlementDays, yoyCapFloor->tenor(), yoyInflationIndex_, obsLag, calendar, bdc, dc, calendar, bdc);

        // Deal with reference calibration date grid stuff based on maturity of helper instrument.
        auto helperInst = helper->yoyCapFloor();
        auto maturity = helperInst->maturityDate();
        auto rcDate = lower_bound(rcDates.begin(), rcDates.end(), maturity);
        if (!(rcDate == rcDates.end() || *rcDate > prevRcDate)) {
            active[i] = false;
            continue;
        }

        if (rcDate != rcDates.end())
            prevRcDate = *rcDate;

        // Price the underlying helper instrument to get its fair premium.
        helperInst->setPricingEngine(engine);

        // Update the helper's market quote with the fair rate.
        quote->setValue(dontCalibrate_ ? 0.1 : helperInst->NPV());

        // Add the helper's time to expiry.
        auto fixingDate = helperInst->lastYoYInflationCoupon()->fixingDate();
        auto t = inflationTime(fixingDate, *yoyTs, yoyInflationIndex_->interpolated());

        // if time is not positive deactivate helper
        if (t < 0.0 || QuantLib::close_enough(t, 0.0)) {
            active[i] = false;
            continue;
        }

        auto p = expiryTimes.insert(t);
        QL_REQUIRE(data_->ignoreDuplicateCalibrationExpiryTimes() || p.second,
                   "InfJyBuilder: a YoY cap floor calibration "
                       << "instrument with the expiry time, " << t << ", was already added.");

        // Add the helper to the calibration helpers.
        if(p.second)
            helpers.push_back(helper);

        TLOG("InfJyBuilder: " << (p.second ?
                                  "added YoYCapFloor helper" :
                                  "skipped YoYCapFloor helper due to duplicate expiry time (" + std::to_string(t) + ")") <<
            ": index = " << data_->index() <<
            ", type = " << yoyCapFloor->type() <<
            ", expiry = " << io::iso_date(maturity) <<
            ", strike = " << strikeValue <<
            ", obs lag = " << obsLag <<
            ", market premium = " << quote->value());
    }

    // Populate the expiry times array with the unique sorted expiry times.
    expiries = Array(expiryTimes.begin(), expiryTimes.end());

    DLOG("InfJyBuilder: finished building the YoY cap floor calibration basket.");

    return helpers;
}

Helpers InfJyBuilder::buildYoYSwapBasket(const CalibrationBasket& cb,
    vector<bool>& active, Array& expiries, bool forRealRateReversion) const {

    DLOG("InfJyBuilder: start building the YoY swap calibration basket.");

    // Initial checks.
    QL_REQUIRE(yoyInflationIndex_, "InfJyBuilder: need a valid year on year inflation index " <<
        "to build a year on year swap calibration basket.");
    auto yoyTs = yoyInflationIndex_->yoyInflationTermStructure();
    QL_REQUIRE(!yoyTs.empty(), "InfJyBuilder: need a valid year on year term structure " <<
        "to build a year on year swap calibration basket.");

    // Procedure is to create a YoY cap floor as described by each instrument in the calibration basket. We then value 
    // each of the YoY cap floor instruments using market data and an engine and pass the NPV as the market premium to 
    // helper that we create.

    Helpers helpers;

    // Create the engine
    auto engine = QuantLib::ext::make_shared<DiscountingSwapEngine>(rateCurve_);

    // YoY swap calibration instrument details. Assumed to equal those from the index and market structures.
    // Some of these should possibly come from conventions. Hardcoded some common values here.
    // Also some variables used in the loop below.
    Natural settlementDays = 2;
    auto calendar = yoyInflationIndex_->fixingCalendar();
    auto dc = Thirty360(Thirty360::BondBasis);
    auto bdc = Following;
    auto obsLag = yoyTs->observationLag();

    // Avoid instruments with duplicate expiry times in the loop below
    set<Time, CloseCmp> expiryTimes;

    // Reference calibration dates if any. If they are given, we only include one calibration instrument from each 
    // period in the grid. Logic copied from other builders.
    auto rcDates = referenceCalibrationDates();
    auto prevRcDate = Date::minDate();

    // Add calibration instruments to the helpers vector.
    const auto& ci = cb.instruments();
    for (Size i = 0; i < ci.size(); ++i) {

        auto yoySwap = QuantLib::ext::dynamic_pointer_cast<YoYSwap>(ci[i]);
        QL_REQUIRE(yoySwap, "InfJyBuilder: expected YoYSwap calibration instrument.");

        // Build the YoY helper.
        auto quote = QuantLib::ext::make_shared<SimpleQuote>(0.01);
        auto helper = QuantLib::ext::make_shared<YoYSwapHelper>(Handle<Quote>(quote), settlementDays, yoySwap->tenor(),
                                                        yoyInflationIndex_, rateCurve_, obsLag, calendar, bdc, dc,
                                                        calendar, bdc, dc, calendar, bdc);

        // Deal with reference calibration date grid stuff based on maturity of helper instrument.
        auto helperInst = helper->yoySwap();
        auto maturity = helperInst->maturityDate();
        auto rcDate = lower_bound(rcDates.begin(), rcDates.end(), maturity);
        if (!(rcDate == rcDates.end() || *rcDate > prevRcDate)) {
            active[i] = false;
            continue;
        }

        if (rcDate != rcDates.end())
            prevRcDate = *rcDate;

        // Price the underlying helper instrument to get its fair rate.
        helperInst->setPricingEngine(engine);

        // Update the helper's market quote with the fair rate.
        quote->setValue(helperInst->fairRate());

        // For JY calibration to YoY swaps, the parameter's time depends on whether you are calibrating the real rate 
        // reversion or the real rate volatility (probably don't want to calibrate the inflation index vol to YoY 
        // swaps as it only shows up via the drift). If you are calibrating to real rate reversion, you want the time 
        // to the numerator index fixing date on the last YoY swaplet on the YoY leg. If you are calibrating to real 
        // rate volatility, you want the time to the denominator index fixing date on the last YoY swaplet on the YoY 
        // leg. We use numerator fixing date - 1 * Years here for this. You can see this from the parameter 
        // dependencies in the YoY swaplet formula in Section 13 of the book (i.e. T vs. S).
        // If t is not positive, we log a message and skip this helper.
        Time t = 0.0;
        QL_REQUIRE(!helperInst->yoyLeg().empty(), "InfJyBuilder: expected YoYSwap to have non-empty YoY leg.");
        auto finalYoYCoupon = QuantLib::ext::dynamic_pointer_cast<YoYInflationCoupon>(helperInst->yoyLeg().back());
        Date numFixingDate = finalYoYCoupon->fixingDate();
        if (forRealRateReversion) {
            t = inflationTime(numFixingDate, *yoyTs, yoyInflationIndex_->interpolated());
        } else {
            auto denFixingDate = numFixingDate - 1 * Years;
            t = inflationTime(denFixingDate, *yoyTs, yoyInflationIndex_->interpolated());
        }

        if (t < 0 || close_enough(t, 0.0)) {
            DLOG("The year on year swap with maturity tenor, " << yoySwap->tenor() << ", and date, " << maturity <<
                ", has a non-positive parameter time, " << t << ", so skipping this as a calibration instrument.");
            continue;
        }

        // Add the helper to the calibration helpers.
        helpers.push_back(helper);

        auto p = expiryTimes.insert(t);
        QL_REQUIRE(p.second, "InfJyBuilder: a YoY swap calibration instrument with the expiry " <<
            "time, " << t << ", was already added.");

        TLOG("InfJyBuilder: added year on year swap helper" <<
            ": index = " << data_->index() <<
            ", maturity = " << io::iso_date(maturity) <<
            ", obs lag = " << obsLag <<
            ", market rate = " << quote->value());
    }

    // Populate the expiry times array with the unique sorted expiry times.
    expiries = Array(expiryTimes.begin(), expiryTimes.end());

    DLOG("InfJyBuilder: finished building the YoY swap calibration basket.");

    return helpers;
}

const CalibrationBasket& InfJyBuilder::calibrationBasket(const string& parameter) const {

    for (const auto& cb : data_->calibrationBaskets()) {
        if (cb.parameter() == parameter) {
            return cb;
        }
    }

    QL_FAIL("InfJyBuilder: unable to find calibration basket with parameter value equal to '" << parameter << "'.");
}

QuantLib::ext::shared_ptr<Lgm1fParametrization<ZeroInflationTermStructure>> InfJyBuilder::createRealRateParam() const {

    DLOG("InfJyBuilder: start creating the real rate parameterisation.");

    // Initial parameter setup as provided by the data_.
    const ReversionParameter& rrReversion = data_->realRateReversion();
    const VolatilityParameter& rrVolatility = data_->realRateVolatility();
    Array rrVolatilityTimes(rrVolatility.times().begin(), rrVolatility.times().end());
    Array rrVolatilityValues(rrVolatility.values().begin(), rrVolatility.values().end());
    Array rrReversionTimes(rrReversion.times().begin(), rrReversion.times().end());
    Array rrReversionValues(rrReversion.values().begin(), rrReversion.values().end());

    // Perform checks and in the event of bootstrap calibration, may need to restructure the parameters.
    setupParams(rrReversion, rrReversionTimes, rrReversionValues, rrInstExpiries_, "RealRate reversion");
    setupParams(rrVolatility, rrVolatilityTimes, rrVolatilityValues, rrInstExpiries_, "RealRate volatility");

    // Create the JY parameterization.
    using RT = LgmData::ReversionType;
    using VT = LgmData::VolatilityType;

    // Real rate parameter constraints
    const auto& cc = data_->calibrationConfiguration();
    auto rrVolConstraint = cc.constraint("RealRateVolatility");
    auto rrRevConstraint = cc.constraint("RealRateReversion");

    // Create the real rate portion of the parameterization
    using QuantLib::ZeroInflationTermStructure;
    QuantLib::ext::shared_ptr<QuantExt::Lgm1fParametrization<ZeroInflationTermStructure>> realRateParam;
    if (rrReversion.reversionType() == RT::HullWhite && rrVolatility.volatilityType() == VT::HullWhite) {
        using QuantExt::Lgm1fPiecewiseConstantHullWhiteAdaptor;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseConstantHullWhiteAdaptor");
        realRateParam = QuantLib::ext::make_shared<Lgm1fPiecewiseConstantHullWhiteAdaptor<ZeroInflationTermStructure>>(
            zeroInflationIndex_->currency(), zeroInflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index(), rrVolConstraint, rrRevConstraint);
    } else if (rrReversion.reversionType() == RT::HullWhite && rrVolatility.volatilityType() == VT::Hagan) {
        using QuantExt::Lgm1fPiecewiseConstantParametrization;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseConstantParametrization");
        realRateParam = QuantLib::ext::make_shared<Lgm1fPiecewiseConstantParametrization<ZeroInflationTermStructure>>(
            zeroInflationIndex_->currency(), zeroInflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index(), rrVolConstraint, rrRevConstraint);
    } else if (rrReversion.reversionType() == RT::Hagan && rrVolatility.volatilityType() == VT::Hagan) {
        using QuantExt::Lgm1fPiecewiseLinearParametrization;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseLinearParametrization");
        realRateParam = QuantLib::ext::make_shared<Lgm1fPiecewiseLinearParametrization<ZeroInflationTermStructure>>(
            zeroInflationIndex_->currency(), zeroInflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index(), rrVolConstraint, rrRevConstraint);
    } else {
        QL_FAIL("InfJyBuilder: reversion type Hagan and volatility type HullWhite not supported.");
    }

    Time horizon = data_->reversionTransformation().horizon();
    if (horizon >= 0.0) {
        DLOG("InfJyBuilder: apply shift horizon " << horizon << " to the JY real rate parameterisation for index " <<
            data_->index() << ".");
        realRateParam->shift() = horizon;
    } else {
        WLOG("InfJyBuilder: ignoring negative horizon, " << horizon <<
            ", passed to the JY real rate parameterisation for index " << data_->index() << ".");
    }

    Real scaling = data_->reversionTransformation().scaling();
    if (scaling > 0.0) {
        DLOG("InfJyBuilder: apply scaling " << scaling << " to the JY real rate parameterisation for index " <<
            data_->index() << ".");
        realRateParam->scaling() = scaling;
    } else {
        WLOG("Ignoring non-positive scaling, " << scaling <<
            ", passed to the JY real rate parameterisation for index " << data_->index() << ".");
    }

    DLOG("InfJyBuilder: finished creating the real rate parameterisation.");

    return realRateParam;
}

QuantLib::ext::shared_ptr<FxBsParametrization> InfJyBuilder::createIndexParam() const {

    DLOG("InfJyBuilder: start creating the index parameterisation.");

    // Initial parameter setup as provided by the data_.
    const VolatilityParameter& idxVolatility = data_->indexVolatility();
    Array idxVolatilityTimes(idxVolatility.times().begin(), idxVolatility.times().end());
    Array idxVolatilityValues(idxVolatility.values().begin(), idxVolatility.values().end());

    // Perform checks and in the event of bootstrap calibration, may need to restructure the parameters.
    setupParams(idxVolatility, idxVolatilityTimes, idxVolatilityValues, indexInstExpiries_, "Index volatility");

    // Create the index portion of the parameterization
    QuantLib::ext::shared_ptr<QuantExt::FxBsParametrization> indexParam;

    Handle<Quote> baseCpiQuote(QuantLib::ext::make_shared<SimpleQuote>(
        dontCalibrate_ ? 100
                       : zeroInflationIndex_->fixing(zeroInflationIndex_->zeroInflationTermStructure()->baseDate())));

    // Index volatility parameter constraints
    const auto& cc = data_->calibrationConfiguration();
    auto idxVolConstraint = cc.constraint("IndexVolatility");

    if (idxVolatility.type() == ParamType::Piecewise) {
        using QuantExt::FxBsPiecewiseConstantParametrization;
        DLOG("InfJyBuilder: index volatility parameterization is FxBsPiecewiseConstantParametrization");
        indexParam = QuantLib::ext::make_shared<FxBsPiecewiseConstantParametrization>(
            zeroInflationIndex_->currency(), baseCpiQuote, idxVolatilityTimes, idxVolatilityValues, idxVolConstraint);
    } else if (idxVolatility.type() == ParamType::Constant) {
        using QuantExt::FxBsConstantParametrization;
        DLOG("InfJyBuilder: index volatility parameterization is FxBsConstantParametrization");
        indexParam = QuantLib::ext::make_shared<FxBsConstantParametrization>(
            zeroInflationIndex_->currency(), baseCpiQuote, idxVolatilityValues[0]);
    } else {
        QL_FAIL("InfJyBuilder: index volatility parameterization needs to be Piecewise or Constant.");
    }

    DLOG("InfJyBuilder: finished creating the index parameterisation.");

    return indexParam;
}

void InfJyBuilder::setupParams(const ModelParameter& param, Array& times, Array& values,
    const Array& expiries, const string& paramName) const {

    DLOG("InfJyBuilder: start setting up parameters for " << paramName);

    if (param.type() == ParamType::Constant) {
        QL_REQUIRE(param.times().size() == 0, "InfJyBuilder: parameter is constant so empty times expected");
        QL_REQUIRE(param.values().size() == 1, "InfJyBuilder: parameter is constant so initial value array " <<
            "should have 1 element.");
    } else if (param.type() == ParamType::Piecewise) {

        if (param.calibrate() && data_->calibrationType() == CalibrationType::Bootstrap) {
            QL_REQUIRE(!expiries.empty(), "InfJyBuilder: calibration instrument expiries are empty.");
            QL_REQUIRE(!values.empty(), "InfJyBuilder: expected at least one initial value.");
            DLOG("InfJyBuilder: overriding initial times " << times << " with option calibration instrument " <<
                "expiries " << expiries << ".");
            times = Array(expiries.begin(), expiries.end() - 1);
            values = Array(times.size() + 1, values[0]);
        } else {
            QL_REQUIRE(values.size() == times.size() + 1, "InfJyBuilder: size of values grid, " << values.size() <<
                ", should be 1 greater than the size of the times grid, " << times.size() << ".");
        }

    } else {
        QL_FAIL("Expected " << paramName << " parameter to be Constant or Piecewise.");
    }

    DLOG("InfJyBuilder: finished setting up parameters for " << paramName);
}

vector<Date> InfJyBuilder::referenceCalibrationDates() const {

    TLOG("InfJyBuilder: start building reference date grid '" << referenceCalibrationGrid_ << "'.");
    
    vector<Date> res;
    if (!referenceCalibrationGrid_.empty())
        res = DateGrid(referenceCalibrationGrid_).dates();

    TLOG("InfJyBuilder: finished building reference date grid.");

    return res;
}

void InfJyBuilder::initialiseMarket() {

    TLOG("InfJyBuilder: start initialising market data members.");

    // Try catches are not nice but Market does not have a method for checking if a structure exists so it is 
    // unfortunately necessary.
    try {
        cpiVolatility_ = market_->cpiInflationCapFloorVolatilitySurface(data_->index(), configuration_);
    } catch (...) {
        DLOG("InfJyBuilder: the market does not have a CPI cap floor volatility surface.");
    }

    try {
        yoyInflationIndex_ = *market_->yoyInflationIndex(data_->index(), configuration_);
        marketObserver_->registerWith(yoyInflationIndex_);
    } catch (...) {
        DLOG("InfJyBuilder: the market does not have a YoY inflation index.");
    }

    try {
        yoyVolatility_ = market_->yoyCapFloorVol(data_->index(), configuration_);
    } catch (...) {
        DLOG("InfJyBuilder: the market does not have a YoY cap floor volatility surface.");
    }

    TLOG("InfJyBuilder: finished initialising market data members.");
}

bool InfJyBuilder::pricesChanged(bool updateCache) const {
    if(dontCalibrate_)
        return false;

    // Build the calibration instruments again before checking the market price below.
    // Don't need to do this if updateCache is true, because only called above after buildCalibrationBaskets().
    if (!updateCache)
        buildCalibrationBaskets();

    // Resize the cache to match the number of calibration instruments
    auto numInsts = realRateBasket_.size() + indexBasket_.size();
    if (priceCache_.size() != numInsts)
        priceCache_ = vector<Real>(numInsts, Null<Real>());

    // Check if any market prices have changed. Return true if they have and false if they have not.
    // If asked to update the cached prices, via updateCache being true, update the prices, if necessary.
    bool result = false;
    Size ctr = 0;
    for (const auto& ci : boost::range::join(realRateBasket_, indexBasket_)) {
        auto mp = marketPrice(ci);
        if (!close_enough(priceCache_[ctr], mp)) {
            if (updateCache)
                priceCache_[ctr] = mp;
            result = true;
        }
        ctr++;
    }

    return result;
}

Real InfJyBuilder::marketPrice(const QuantLib::ext::shared_ptr<CalibrationHelper>& helper) const {

    if (auto h = QuantLib::ext::dynamic_pointer_cast<CpiCapFloorHelper>(helper)) {
        return h->marketValue();
    }

    if (auto h = QuantLib::ext::dynamic_pointer_cast<YoYCapFloorHelper>(helper)) {
        return h->marketValue();
    }

    if (QuantLib::ext::shared_ptr<YoYSwapHelper> h = QuantLib::ext::dynamic_pointer_cast<YoYSwapHelper>(helper)) {
        return h->marketRate();
    }

    QL_FAIL("InfJyBuilder: unrecognised calibration instrument for JY calibration.");
}

}
}
