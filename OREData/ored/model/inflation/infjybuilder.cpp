/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/log.hpp>
#include <qle/models/cpicapfloorhelper.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/models/fxbspiecewiseconstantparametrization.hpp>
#include <qle/models/infjyparameterization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/yoyswaphelper.hpp>
#include <qle/pricingengines/cpiblackcapfloorengine.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

using QuantExt::CPIBlackCapFloorEngine;
using QuantExt::CpiCapFloorHelper;
using QuantExt::FxBsParametrization;
using QuantExt::Lgm1fParametrization;
using QuantExt::YoYSwapHelper;
using QuantLib::DiscountingSwapEngine;
using QuantLib::YoYInflationCoupon;
using std::lower_bound;
using std::set;
using std::string;

namespace ore {
namespace data {

using Helpers = InfJyBuilder::Helpers;

InfJyBuilder::InfJyBuilder(
    const boost::shared_ptr<Market>& market,
    const boost::shared_ptr<InfJyData>& data,
    const string& configuration,
    const string& referenceCalibrationGrid)
    : market_(market),
    configuration_(configuration),
    data_(data),
    referenceCalibrationGrid_(referenceCalibrationGrid),
    marketObserver_(boost::make_shared<MarketObserver>()),
    zeroInflationIndex_(*market_->zeroInflationIndex(data_->index(), configuration_)) {

    LOG("InfJyBuilder: building model for inflation index " << data_->index());

    // Register with market observables except volatilities
    marketObserver_->registerWith(zeroInflationIndex_);

    // Register the model builder with the market observer
    registerWith(marketObserver_);

    // Notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    // Build the calibration instruments
    buildCalibrationBaskets();

    // Create the JY parameterisation.
    parameterization_ = boost::make_shared<QuantExt::InfJyParameterization>(
        createRealRateParam(), createIndexParam(), zeroInflationIndex_);
}

string InfJyBuilder::inflationIndex() const {
    return data_->index();
}

boost::shared_ptr<QuantExt::InfJyParameterization> InfJyBuilder::parameterization() const {
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
    return (data_->realRateVolatility().calibrate() ||
        data_->realRateReversion().calibrate() ||
        data_->indexVolatility().calibrate()) &&
        (marketObserver_->hasUpdated(false) ||
            forceCalibration_);
}

void InfJyBuilder::performCalculations() const {
    if (requiresRecalibration()) {
        marketObserver_->hasUpdated(true);
        buildCalibrationBaskets();
    }
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

    const auto& ci = cb.instruments();
    QL_REQUIRE(ci.size() == active.size(), "InfJyBuilder: expected the active options vector size to equal the " <<
        "number of calibration instruments");
    fill(active.begin(), active.end(), false);

    // Procedure is to create a CPI cap floor as described by each instrument in the calibration basket. We then value 
    // each of the CPI cap floor instruments using market data and an engine and pass the NPV as the market premium to 
    // helper that we create.

    Helpers helpers;

    // Create the engine
    auto yts = zeroInflationIndex_->zeroInflationTermStructure()->nominalTermStructure();
    auto engine = boost::make_shared<CPIBlackCapFloorEngine>(yts, cpiVolatility_);

    // CPI cap floor calibration instrument details. Assumed to equal those from the index and market structures.
    // Some of these should possibly come from conventions.
    // Also some variables used in the loop below.
    auto calendar = zeroInflationIndex_->fixingCalendar();
    auto baseDate = zeroInflationIndex_->zeroInflationTermStructure()->baseDate();
    auto dc = zeroInflationIndex_->zeroInflationTermStructure()->dayCounter();
    auto baseCpi = zeroInflationIndex_->fixing(baseDate);
    auto isInterp = zeroInflationIndex_->interpolated();
    auto freq = zeroInflationIndex_->frequency();
    auto bdc = cpiVolatility_->businessDayConvention();
    auto obsLag = cpiVolatility_->observationLag();
    Handle<ZeroInflationIndex> inflationIndex(zeroInflationIndex_);
    Date today = Settings::instance().evaluationDate();
    Real nominal = 1.0;

    // Avoid instruments with duplicate expiry times in the loop below
    auto cmp = [](Time s, Time t) { return close(s, t); };
    set<Time, decltype(cmp)> expiryTimes(cmp);

    // Reference calibration dates if any. If they are given, we only include one calibration instrument from each 
    // period in the grid. Logic copied from other builders.
    auto rcDates = referenceCalibrationDates();
    auto prevRcDate = Date::minDate();

    // Add calibration instruments to the helpers vector.
    for (Size i = 0; i < ci.size(); ++i) {

        auto cpiCapFloor = boost::dynamic_pointer_cast<CpiCapFloor>(ci[i]);
        QL_REQUIRE(cpiCapFloor, "InfJyBuilder: expected CpiCapFloor calibration instrument.");
        auto maturity = calendar.advance(today, cpiCapFloor->tenor());

        // Deal with reference calibration date grid stuff.
        auto rcDate = lower_bound(rcDates.begin(), rcDates.end(), maturity);
        if (!(rcDate == rcDates.end() || *rcDate > prevRcDate)) {
            active[i] = false;
            continue;
        }

        if (rcDate != rcDates.end())
            prevRcDate = *rcDate;

        // Build the CPI calibration instrument in order to calculate its NPV.
        auto strike = boost::dynamic_pointer_cast<AbsoluteStrike>(cpiCapFloor->strike());
        QL_REQUIRE(cpiCapFloor, "Expected CpiCapFloor in inflation model data to have absolute strikes.");
        Real strikeValue = strike->strike();
        Option::Type capfloor = cpiCapFloor->type() == CapFloor::Cap ? Option::Call : Option::Put;
        auto inst = boost::make_shared<CPICapFloor>(capfloor, nominal, today, baseCpi, maturity, calendar,
                bdc, calendar, bdc, strikeValue, inflationIndex, obsLag);
        inst->setPricingEngine(engine);

        // Build the helper using the NPV as the premium.
        auto premium = inst->NPV();
        auto helper = boost::make_shared<CpiCapFloorHelper>(capfloor, baseCpi, maturity, calendar, bdc,
            calendar, bdc, strikeValue, inflationIndex, obsLag, premium);
        helpers.push_back(helper);

        // Add the helper's time to expiry.
        auto fixingDate = helper->instrument()->fixingDate();
        auto t = inflationYearFraction(freq, isInterp, dc, baseDate, fixingDate);
        auto p = expiryTimes.insert(t);
        QL_REQUIRE(p.second, "InfJyBuilder: a CPI cap floor calibration " <<
            "instrument with the expiry time, " << t << ", was already added.");

        TLOG("InfJyBuilder: added CPICapFloor helper" <<
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

Helpers InfJyBuilder::buildYoYCapFloorBasket(const CalibrationBasket& cb,
    vector<bool>& active, Array& expiries) const {

    DLOG("InfJyBuilder: start building the YoY cap floor calibration basket.");

    const auto& ci = cb.instruments();
    QL_REQUIRE(ci.size() == active.size(), "InfJyBuilder: expected the active options vector size to equal the " <<
        "number of calibration instruments");
    fill(active.begin(), active.end(), false);

    // Procedure is to create a YoY cap floor as described by each instrument in the calibration basket. We then value 
    // each of the YoY cap floor instruments using market data and an engine and pass the NPV as the market premium to 
    // helper that we create.

    Helpers helpers;

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
    auto yts = yoyTs->nominalTermStructure();
    QL_REQUIRE(!yts.empty(), "InfJyBuilder: need a valid nominal term structure " <<
        "to build a year on year swap calibration basket.");

    const auto& ci = cb.instruments();
    QL_REQUIRE(ci.size() == active.size(), "InfJyBuilder: expected the active swaps vector size to equal the " <<
        "number of calibration instruments");
    fill(active.begin(), active.end(), false);

    // Procedure is to create a YoY cap floor as described by each instrument in the calibration basket. We then value 
    // each of the YoY cap floor instruments using market data and an engine and pass the NPV as the market premium to 
    // helper that we create.

    Helpers helpers;

    // Create the engine
    auto engine = boost::make_shared<DiscountingSwapEngine>(yts);

    // YoY swap calibration instrument details. Assumed to equal those from the index and market structures.
    // Some of these should possibly come from conventions. Hardcoded some common values here.
    // Also some variables used in the loop below.
    Natural settlementDays = 2;
    auto calendar = yoyInflationIndex_->fixingCalendar();
    auto dc = yoyTs->dayCounter();
    auto bdc = cpiVolatility_->businessDayConvention();
    auto obsLag = cpiVolatility_->observationLag();
    Date today = Settings::instance().evaluationDate();

    // Avoid instruments with duplicate expiry times in the loop below
    auto cmp = [](Time s, Time t) { return close(s, t); };
    set<Time, decltype(cmp)> expiryTimes(cmp);

    // Reference calibration dates if any. If they are given, we only include one calibration instrument from each 
    // period in the grid. Logic copied from other builders.
    auto rcDates = referenceCalibrationDates();
    auto prevRcDate = Date::minDate();

    // Add calibration instruments to the helpers vector.
    for (Size i = 0; i < ci.size(); ++i) {

        auto yoySwap = boost::dynamic_pointer_cast<YoYSwap>(ci[i]);
        QL_REQUIRE(yoySwap, "InfJyBuilder: expected YoYSwap calibration instrument.");

        // Build the YoY helper.
        auto quote = boost::make_shared<SimpleQuote>(0.01);
        auto helper = boost::make_shared<YoYSwapHelper>(Handle<Quote>(quote), settlementDays, yoySwap->tenor(),
            yoyInflationIndex_, obsLag, calendar, bdc, dc, calendar, bdc, dc, calendar, bdc);

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

        // Add the helper to the calibration helpers.
        helpers.push_back(helper);

        // For JY calibration to YoY swaps, the parameter's time depends on whether you are calibrating the real rate 
        // reversion or the real rate volatility (probably don't want to calibrate the inflation index vol to YoY 
        // swaps as it only shows up via the drift). If you are calibrating to real rate reversion, you want the time 
        // to the numerator index fixing date on the last YoY swaplet on the YoY leg. If you are calibrating to real 
        // rate volatility, you want the time to the denominator index fixing date on the last YoY swaplet on the YoY 
        // leg. We use numerator fixing date - 1 * Years here for this. You can see this from the parameter 
        // dependencies in the YoY swaplet formula in Section 13 of the book (i.e. T vs. S).
        Time t = 0.0;
        QL_REQUIRE(!helperInst->yoyLeg().empty(), "InfJyBuilder: expected YoYSwap to have non-empty YoY leg.");
        auto finalYoYCoupon = boost::dynamic_pointer_cast<YoYInflationCoupon>(helperInst->yoyLeg().back());
        Date numFixingDate = finalYoYCoupon->fixingDate();
        if (forRealRateReversion) {
            t = yoyTs->timeFromReference(numFixingDate);
        } else {
            t = yoyTs->timeFromReference(numFixingDate - 1 * Years);
        }
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

boost::shared_ptr<Lgm1fParametrization<ZeroInflationTermStructure>> InfJyBuilder::createRealRateParam() const {

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

    // Create the real rate portion of the parameterization
    using QuantLib::ZeroInflationTermStructure;
    boost::shared_ptr<QuantExt::Lgm1fParametrization<ZeroInflationTermStructure>> realRateParam;
    if (rrReversion.reversionType() == RT::HullWhite && rrVolatility.volatilityType() == VT::HullWhite) {
        using QuantExt::Lgm1fPiecewiseConstantHullWhiteAdaptor;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseConstantHullWhiteAdaptor");
        realRateParam = boost::make_shared<Lgm1fPiecewiseConstantHullWhiteAdaptor<ZeroInflationTermStructure>>(
            zeroInflationIndex_->currency(), zeroInflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index());
    } else if (rrReversion.reversionType() == RT::HullWhite) {
        using QuantExt::Lgm1fPiecewiseConstantParametrization;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseConstantParametrization");
        realRateParam = boost::make_shared<Lgm1fPiecewiseConstantParametrization<ZeroInflationTermStructure>>(
            zeroInflationIndex_->currency(), zeroInflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index());
    } else {
        using QuantExt::Lgm1fPiecewiseLinearParametrization;
        DLOG("InfJyBuilder: real rate parameterization is Lgm1fPiecewiseLinearParametrization");
        realRateParam = boost::make_shared<Lgm1fPiecewiseLinearParametrization<ZeroInflationTermStructure>>(
            zeroInflationIndex_->currency(), zeroInflationIndex_->zeroInflationTermStructure(), rrVolatilityTimes,
            rrVolatilityValues, rrReversionTimes, rrReversionValues, data_->index());
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

boost::shared_ptr<FxBsParametrization> InfJyBuilder::createIndexParam() const {

    DLOG("InfJyBuilder: start creating the index parameterisation.");

    // Initial parameter setup as provided by the data_.
    const VolatilityParameter& idxVolatility = data_->indexVolatility();
    Array idxVolatilityTimes(idxVolatility.times().begin(), idxVolatility.times().end());
    Array idxVolatilityValues(idxVolatility.values().begin(), idxVolatility.values().end());

    // Perform checks and in the event of bootstrap calibration, may need to restructure the parameters.
    setupParams(idxVolatility, idxVolatilityTimes, idxVolatilityValues, indexInstExpiries_, "Index volatility");

    // Create the JY parameterization.
    using RT = LgmData::ReversionType;
    using VT = LgmData::VolatilityType;

    // Create the index portion of the parameterization
    boost::shared_ptr<QuantExt::FxBsParametrization> indexParam;

    Handle<Quote> baseCpiQuote(boost::make_shared<SimpleQuote>(
        zeroInflationIndex_->fixing(zeroInflationIndex_->zeroInflationTermStructure()->baseDate())));

    if (idxVolatility.type() == ParamType::Piecewise) {
        using QuantExt::FxBsPiecewiseConstantParametrization;
        DLOG("InfJyBuilder: index volatility parameterization is FxBsPiecewiseConstantParametrization");
        indexParam = boost::make_shared<FxBsPiecewiseConstantParametrization>(
            zeroInflationIndex_->currency(), baseCpiQuote, idxVolatilityTimes, idxVolatilityValues);
    } else if (idxVolatility.type() == ParamType::Constant) {
        using QuantExt::FxBsConstantParametrization;
        DLOG("InfJyBuilder: index volatility parameterization is FxBsConstantParametrization");
        indexParam = boost::make_shared<FxBsConstantParametrization>(
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

}
}
