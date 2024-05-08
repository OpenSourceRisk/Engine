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
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>

#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/marketobserver.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ored/model/lgmbuilder.hpp>
#include <ored/model/structuredmodelerror.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace {

// Return swaption data
SwaptionData swaptionData(const QuantLib::ext::shared_ptr<Swaption> swaption, const Handle<YieldTermStructure>& yts,
                          const Handle<SwaptionVolatilityStructure>& svts) {

    QuantLib::ext::shared_ptr<PricingEngine> engine;
    switch (svts->volatilityType()) {
    case ShiftedLognormal:
        engine = QuantLib::ext::make_shared<BlackSwaptionEngine>(yts, svts);
        break;
    case Normal:
        engine = QuantLib::ext::make_shared<BachelierSwaptionEngine>(yts, svts);
        break;
    default:
        QL_FAIL("Could not construct swaption engine for volatility type: " << svts->volatilityType());
        break;
    }

    swaption->setPricingEngine(engine);

    SwaptionData sd;
    sd.timeToExpiry = yts->timeFromReference(swaption->exercise()->dates().back());
    sd.swapLength = swaption->result<Real>("swapLength");
    sd.strike = swaption->result<Real>("strike");
    sd.atmForward = swaption->result<Real>("atmForward");
    sd.annuity = swaption->result<Real>("annuity");
    sd.vega = swaption->result<Real>("vega");
    sd.stdDev = swaption->result<Real>("stdDev");

    return sd;
}

// Utility function to create swaption helper. Returns helper and (possibly updated) strike
template <typename E, typename T>
std::pair<QuantLib::ext::shared_ptr<SwaptionHelper>, double>
createSwaptionHelper(const E& expiry, const T& term, const Handle<SwaptionVolatilityStructure>& svts,
                     const Handle<Quote>& vol, const QuantLib::ext::shared_ptr<IborIndex>& iborIndex,
                     const Period& fixedLegTenor, const DayCounter& fixedDayCounter, const DayCounter& floatDayCounter,
                     const Handle<YieldTermStructure>& yts, BlackCalibrationHelper::CalibrationErrorType errorType,
                     Real strike, Real shift, const Size settlementDays, const RateAveraging::Type averagingMethod) {

    DLOG("LgmBuilder::createSwaptionHelper(" << expiry << ", " << term << ")");

    // hardcoded parameters to ensure a robust cailbration:

    // 1 If the helper's strike is too far away from the ATM level in terms of the relevant std dev, we move the
    //   calibration strike closer to the ATM level
    static constexpr Real maxAtmStdDev = 3.0;

    // 2 If the helper value is lower than mmv, replace it with a "more reasonable" helper. Here, we replace
    //   the helper with a helper that has the ATM strike. There are other options here.
    static constexpr Real mmv = 1.0E-20;

    // 3 Switch to PriceError if helper's market value is below smv
    static constexpr Real smv = 1.0E-8;

    // Notice: the vol that is passed in to this method is in general a dummy value, which is good enough though to
    // check 2 and 3 above. To check 1, the vol is not needed at all.

    auto vt = svts->volatilityType();
    auto helper = QuantLib::ext::make_shared<SwaptionHelper>(expiry, term, vol, iborIndex, fixedLegTenor, fixedDayCounter,
                                                     floatDayCounter, yts, errorType, strike, 1.0, vt, shift,
                                                     settlementDays, averagingMethod);
    auto sd = swaptionData(helper->swaption(), yts, svts);

    // ensure point 1 from above

    Real atmStdDev = svts->volatility(sd.timeToExpiry, sd.swapLength, sd.atmForward) * std::sqrt(sd.timeToExpiry);
    if (vt == ShiftedLognormal) {
        atmStdDev *= sd.atmForward + shift;
    }
    if (strike != Null<Real>() && std::abs(strike - sd.atmForward) > maxAtmStdDev * atmStdDev) {
        DLOG("Helper with expiry " << expiry << " and term " << term << " has a strike (" << strike
                                   << ") that is too far out of the money (atm = " << sd.atmForward << ", atmStdDev = "
                                   << atmStdDev << "). Adjusting the strike using maxAtmStdDev " << maxAtmStdDev);
        if (strike > sd.atmForward)
            strike = sd.atmForward + maxAtmStdDev * atmStdDev;
        else
            strike = sd.atmForward - maxAtmStdDev * atmStdDev;
        helper = QuantLib::ext::make_shared<SwaptionHelper>(expiry, term, vol, iborIndex, fixedLegTenor, fixedDayCounter,
                                                    floatDayCounter, yts, errorType, strike, 1.0, vt, shift,
                                                    settlementDays, averagingMethod);
    }

    // ensure point 2 from above

    auto mv = std::abs(helper->marketValue());
    if (mv < mmv) {
        DLOG("Helper with expiry " << expiry << " and term " << term << " has an absolute market value of "
                                   << std::scientific << mv << " which is lower than minimum market value " << mmv
                                   << " so switching to helper with atm rate " << sd.atmForward);
        strike = sd.atmForward;
        helper = QuantLib::ext::make_shared<SwaptionHelper>(expiry, term, vol, iborIndex, fixedLegTenor, fixedDayCounter,
                                                    floatDayCounter, yts, errorType, strike, 1.0, vt, shift,
                                                    settlementDays, averagingMethod);
    }

    // ensure point 3 from above

    mv = std::abs(helper->marketValue());
    if (errorType != BlackCalibrationHelper::PriceError && mv < smv) {
        errorType = BlackCalibrationHelper::PriceError;
        TLOG("Helper with expiry " << expiry << " and term " << term << " has an absolute market value of "
                                   << std::scientific << mv << " which is lower than " << smv
                                   << " so switching to a price error helper.");
        helper = QuantLib::ext::make_shared<SwaptionHelper>(expiry, term, vol, iborIndex, fixedLegTenor, fixedDayCounter,
                                                    floatDayCounter, yts, errorType, strike, 1.0, vt, shift,
                                                    settlementDays, averagingMethod);
    }

    DLOG("Created swaption helper with expiry " << expiry << " and term " << term << ": vol=" << vol->value()
                                                << ", index=" << iborIndex->name() << ", strike=" << strike
                                                << ", shift=" << shift);

    return std::make_pair(helper, strike);
}

} // namespace

namespace ore {
namespace data {

LgmBuilder::LgmBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<IrLgmData>& data,
                       const std::string& configuration, const Real bootstrapTolerance, const bool continueOnError,
                       const std::string& referenceCalibrationGrid, const bool setCalibrationInfo,
                       const std::string& id)
    : market_(market), configuration_(configuration), data_(data), bootstrapTolerance_(bootstrapTolerance),
      continueOnError_(continueOnError), referenceCalibrationGrid_(referenceCalibrationGrid),
      setCalibrationInfo_(setCalibrationInfo), id_(id),
      optimizationMethod_(QuantLib::ext::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8)),
      calibrationErrorType_(BlackCalibrationHelper::RelativePriceError) {

    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    string qualifier = data_->qualifier();
    currency_ = qualifier;
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(qualifier, index)) {
        currency_ = index->currency().code();
    }
    LOG("LgmCalibration for qualifier " << qualifier << " (ccy=" << currency_ << "), configuration is "
                                        << configuration_);
    Currency ccy = parseCurrency(currency_);

    requiresCalibration_ =
        (data_->calibrateA() || data_->calibrateH()) && data_->calibrationType() != CalibrationType::None;

    try {
        shortSwapIndex_ =
            market_->swapIndex(market_->shortSwapIndexBase(data_->qualifier(), configuration_), configuration_);
        swapIndex_ = market_->swapIndex(market_->swapIndexBase(data_->qualifier(), configuration_), configuration_);
        svts_ = market_->swaptionVol(data_->qualifier(), configuration_);
        // see the comment for dinscountCurve() in the interface
        modelDiscountCurve_ = RelinkableHandle<YieldTermStructure>(*swapIndex_->discountingTermStructure());
        calibrationDiscountCurve_ = Handle<YieldTermStructure>(*swapIndex_->discountingTermStructure());
    } catch (const std::exception& e) {
        StructuredModelErrorMessage(
            "Error when retrieving swap index base for qualifier '" + data_->qualifier() +
                "'. Use market discount curve instead of swap index discount curve as a fallback.",
            e.what(), id_)
            .log();
        modelDiscountCurve_ = RelinkableHandle<YieldTermStructure>(*market_->discountCurve(currency_, configuration_));
        calibrationDiscountCurve_ = Handle<YieldTermStructure>(*market_->discountCurve(currency_, configuration_));
    }

    if (requiresCalibration_) {
        registerWith(svts_);
        marketObserver_->addObservable(swapIndex_->forwardingTermStructure());
        marketObserver_->addObservable(shortSwapIndex_->forwardingTermStructure());
        marketObserver_->addObservable(shortSwapIndex_->discountingTermStructure());
    }
    // we do not register with modelDiscountCurve_, since this curve does not affect the calibration
    marketObserver_->addObservable(calibrationDiscountCurve_);
    registerWith(marketObserver_);
    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    swaptionActive_ = std::vector<bool>(data_->optionExpiries().size(), false);

    if (requiresCalibration_) {
        buildSwaptionBasket();
    }

    Array aTimes(data_->aTimes().begin(), data_->aTimes().end());
    Array hTimes(data_->hTimes().begin(), data_->hTimes().end());
    Array alpha(data_->aValues().begin(), data_->aValues().end());
    Array h(data_->hValues().begin(), data_->hValues().end());

    if (data_->aParamType() == ParamType::Constant) {
        QL_REQUIRE(data_->aTimes().size() == 0,
                   "LgmBuilder: empty volatility time grid expected for constant parameter type");
        QL_REQUIRE(data_->aValues().size() == 1,
                   "LgmBuilder: initial volatility values should have size 1 for constant parameter type");
    } else if (data_->aParamType() == ParamType::Piecewise) {
        if (data_->calibrateA() && data_->calibrationType() == CalibrationType::Bootstrap) {
            if (data_->aTimes().size() > 0) {
                DLOG("overriding alpha time grid with swaption expiries, set all initial values to first given value");
            }
            QL_REQUIRE(swaptionExpiries_.size() > 0, "empty swaptionExpiries");
            aTimes = Array(swaptionExpiries_.begin(), swaptionExpiries_.end() - 1);
            alpha = Array(aTimes.size() + 1, data_->aValues()[0]);
        } else {
            QL_REQUIRE(alpha.size() == aTimes.size() + 1,
                       "LgmBuilder: LGM volatility time and initial value array sizes do not match");
        }
    } else
        QL_FAIL("LgmBuilder: volatility parameter type not covered");

    if (data_->hParamType() == ParamType::Constant) {
        QL_REQUIRE(data_->hTimes().size() == 0,
                   "LgmBuilder: empty reversion time grid expected for constant parameter type");
        QL_REQUIRE(data_->hValues().size() == 1,
                   "LgmBuidler: initial reversion values should have size 1 for constant parameter type");
    } else if (data_->hParamType() == ParamType::Piecewise) {
        if (data_->calibrateH() && data_->calibrationType() == CalibrationType::Bootstrap) {
            if (data_->hTimes().size() > 0) {
                DLOG("overriding h time grid with swaption underlying maturities, set all initial values to first "
                     "given value");
            }
            hTimes = swaptionMaturities_;
            h = Array(hTimes.size() + 1, data_->hValues()[0]);
        } else { // use input time grid and input h array otherwise
            QL_REQUIRE(h.size() == hTimes.size() + 1, "H grids do not match");
        }
    } else
        QL_FAIL("LgmBuilder: reversion parameter type case not covered");

    DLOG("before calibration: alpha times = " << aTimes << " values = " << alpha);
    DLOG("before calibration:     h times = " << hTimes << " values = " << h);

    if (data_->reversionType() == LgmData::ReversionType::HullWhite &&
        data_->volatilityType() == LgmData::VolatilityType::HullWhite) {
        DLOG("IR parametrization for " << qualifier << ": IrLgm1fPiecewiseConstantHullWhiteAdaptor");
        parametrization_ = QuantLib::ext::make_shared<QuantExt::IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
    } else if (data_->reversionType() == LgmData::ReversionType::HullWhite &&
               data_->volatilityType() == LgmData::VolatilityType::Hagan) {
        DLOG("IR parametrization for " << qualifier << ": IrLgm1fPiecewiseConstant");
        parametrization_ = QuantLib::ext::make_shared<QuantExt::IrLgm1fPiecewiseConstantParametrization>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
    } else if (data_->reversionType() == LgmData::ReversionType::Hagan &&
               data_->volatilityType() == LgmData::VolatilityType::Hagan) {
        parametrization_ = QuantLib::ext::make_shared<QuantExt::IrLgm1fPiecewiseLinearParametrization>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
        DLOG("IR parametrization for " << qualifier << ": IrLgm1fPiecewiseLinear");
    } else {
        QL_FAIL("LgmBuilder: Reversion type Hagan and volatility type HullWhite not covered");
    }
    DLOG("alpha times size: " << aTimes.size());
    DLOG("lambda times size: " << hTimes.size());

    model_ = QuantLib::ext::make_shared<QuantExt::LGM>(parametrization_);
    params_ = model_->params();
}

Real LgmBuilder::error() const {
    calculate();
    return error_;
}

QuantLib::ext::shared_ptr<QuantExt::LGM> LgmBuilder::model() const {
    calculate();
    return model_;
}

QuantLib::ext::shared_ptr<QuantExt::IrLgm1fParametrization> LgmBuilder::parametrization() const {
    calculate();
    return parametrization_;
}

std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> LgmBuilder::swaptionBasket() const {
    calculate();
    return swaptionBasket_;
}

bool LgmBuilder::requiresRecalibration() const {
    return requiresCalibration_ &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
}

void LgmBuilder::performCalculations() const {

    DLOG("Recalibrate LGM model for qualifier " << data_->qualifier() << " currency " << currency_);

    if (!requiresRecalibration()) {
        DLOG("Skipping calibration as nothing has changed");
        return;
    }

    // reset lgm observer's updated flag
    marketObserver_->hasUpdated(true);

    if (swaptionBasketRefDate_ != calibrationDiscountCurve_->referenceDate()) {
        // build swaption basket if required, i.e. if reference date has changed since last build
        buildSwaptionBasket();
        volSurfaceChanged(true);
        updateSwaptionBasketVols();
    } else {
        // otherwise just update vols
        volSurfaceChanged(true);
        updateSwaptionBasketVols();
    }

    for (Size j = 0; j < swaptionBasket_.size(); j++) {
        auto engine = QuantLib::ext::make_shared<QuantExt::AnalyticLgmSwaptionEngine>(model_, calibrationDiscountCurve_,
                                                                                      data_->floatSpreadMapping());
        engine->enableCache(!data_->calibrateH(), !data_->calibrateA());
        swaptionBasket_[j]->setPricingEngine(engine);
        // necessary if notifications are disabled (observation mode = Disable)
        swaptionBasket_[j]->update();
    }

    // reset model parameters to ensure identical results on identical market data input
    model_->setParams(params_);
    parametrization_->shift() = 0.0;
    parametrization_->scaling() = 1.0;

    LgmCalibrationInfo calibrationInfo;
    error_ = QL_MAX_REAL;
    std::string errorTemplate =
        std::string("Failed to calibrate LGM Model. ") +
        (continueOnError_ ? std::string("Calculation will proceed anyway - using the calibration as is!")
                          : std::string("Calculation will aborted."));
    try {
        if (data_->calibrateA() && !data_->calibrateH() && data_->calibrationType() == CalibrationType::Bootstrap) {
            DLOG("call calibrateVolatilitiesIterative for volatility calibration (bootstrap)");
            model_->calibrateVolatilitiesIterative(swaptionBasket_, *optimizationMethod_, endCriteria_);
        } else if (data_->calibrateH() && !data_->calibrateA() &&
                   data_->calibrationType() == CalibrationType::Bootstrap) {
            DLOG("call calibrateReversionsIterative for reversion calibration (bootstrap)");
            model_->calibrateVolatilitiesIterative(swaptionBasket_, *optimizationMethod_, endCriteria_);
        } else {
            QL_REQUIRE(data_->calibrationType() != CalibrationType::Bootstrap,
                       "LgmBuidler: Calibration type Bootstrap can be used with volatilities and reversions calibrated "
                       "simultaneously. Either choose BestFit oder fix one of these parameters.");
            if (data_->calibrateA() && !data_->calibrateH()) {
                DLOG("call calibrateVolatilities for (global) volatility calibration")
                model_->calibrateVolatilities(swaptionBasket_, *optimizationMethod_, endCriteria_);
            } else if (data_->calibrateH() && !data_->calibrateA()) {
                DLOG("call calibrateReversions for (global) reversion calibration")
                model_->calibrateReversions(swaptionBasket_, *optimizationMethod_, endCriteria_);
            } else {
                DLOG("call calibrate for global volatility and reversion calibration");
                model_->calibrate(swaptionBasket_, *optimizationMethod_, endCriteria_);
            }
        }
        TLOG("LGM " << data_->qualifier() << " calibration errors:");
        error_ = getCalibrationError(swaptionBasket_);
    } catch (const std::exception& e) {
        // just log a warning, we check below if we meet the bootstrap tolerance and handle the result there
        StructuredModelErrorMessage(errorTemplate, e.what(), id_).log();
    }
    calibrationInfo.rmse = error_;
    if (fabs(error_) < bootstrapTolerance_ ||
        (data_->calibrationType() == CalibrationType::BestFit && error_ != QL_MAX_REAL)) {
        // we check the log level here to avoid unnecessary computations
        if (Log::instance().filter(ORE_DATA) || setCalibrationInfo_) {
            TLOGGERSTREAM("Basket details:");
            try {
                auto d = getBasketDetails(calibrationInfo);
                TLOGGERSTREAM(d);
            } catch (const std::exception& e) {
                WLOG("An error occurred: " << e.what());
            }
            TLOGGERSTREAM("Calibration details (with time grid = calibration swaption expiries):");
            try {
                auto d = getCalibrationDetails(calibrationInfo, swaptionBasket_, parametrization_);
                TLOGGERSTREAM(d);
            } catch (const std::exception& e) {
                WLOG("An error occurred: " << e.what());
            }
            TLOGGERSTREAM("Parameter details (with parameter time grid)");
            TLOGGERSTREAM(getCalibrationDetails(calibrationInfo, swaptionBasket_, parametrization_))
            TLOGGERSTREAM("rmse = " << error_);
            calibrationInfo.valid = true;
        }
    } else {
        std::string exceptionMessage = "LGM (" + data_->qualifier() + ") calibration error " + std::to_string(error_) +
                                       " exceeds tolerance " + std::to_string(bootstrapTolerance_);
        StructuredModelErrorMessage(errorTemplate, exceptionMessage, id_).log();
        WLOGGERSTREAM("Basket details:");
        try {
            auto d = getBasketDetails(calibrationInfo);
            WLOGGERSTREAM(d);
        } catch (const std::exception& e) {
            WLOG("An error occurred: " << e.what());
        }
        WLOGGERSTREAM("Calibration details (with time grid = calibration swaption expiries):");
        try {
            auto d = getCalibrationDetails(calibrationInfo, swaptionBasket_, parametrization_);
            WLOGGERSTREAM(d);
        } catch (const std::exception& e) {
            WLOG("An error occurred: " << e.what());
        }
        WLOGGERSTREAM("Parameter details (with parameter time grid)");
        WLOGGERSTREAM(getCalibrationDetails(parametrization_));
        WLOGGERSTREAM("rmse = " << error_);
        calibrationInfo.valid = true;
        if (!continueOnError_) {
            QL_FAIL(exceptionMessage);
        }
    }
    model_->setCalibrationInfo(calibrationInfo);

    DLOG("Apply shift horizon and scale (if not 0.0 and 1.0 respectively)");

    QL_REQUIRE(data_->shiftHorizon() >= 0.0, "shift horizon must be non negative");
    QL_REQUIRE(data_->scaling() > 0.0, "scaling must be positive");

    if (data_->shiftHorizon() > 0.0) {
        Real value = -parametrization_->H(data_->shiftHorizon());
        DLOG("Apply shift horizon " << data_->shiftHorizon() << " (C=" << value << ") to the " << data_->qualifier()
                                    << " LGM model");
        parametrization_->shift() = value;
    }

    if (data_->scaling() != 1.0) {
        DLOG("Apply scaling " << data_->scaling() << " to the " << data_->qualifier() << " LGM model");
        parametrization_->scaling() = data_->scaling();
    }
} // performCalculations()

void LgmBuilder::getExpiryAndTerm(const Size j, Period& expiryPb, Period& termPb, Date& expiryDb, Date& termDb,
                                  Real& termT, bool& expiryDateBased, bool& termDateBased) const {
    std::string expiryString = data_->optionExpiries()[j];
    std::string termString = data_->optionTerms()[j];
    parseDateOrPeriod(expiryString, expiryDb, expiryPb, expiryDateBased);
    parseDateOrPeriod(termString, termDb, termPb, termDateBased);
    if (termDateBased) {
        Date tmpExpiry = expiryDateBased ? expiryDb : svts_->optionDateFromTenor(expiryPb);
        Date tmpStart = swapIndex_->iborIndex()->valueDate(swapIndex_->iborIndex()->fixingCalendar().adjust(tmpExpiry));
        // ensure that we have a term >= 1 Month, otherwise QL might throw "non-positive swap length (0)  given" from
        // the black swaption engine during calibration helper pricing; also notice that we use the swap length
        // calculated in the svts (i.e. a length rounded to whole months) to read the volatility from the cube, which is
        // consistent with what is done in BlackSwaptionEngine (although one might ask whether an interpolated
        // volatility would be more appropriate)
        termDb = std::max(termDb, tmpStart + 1 * Months);
        termT = svts_->swapLength(tmpStart, termDb);
    } else {
        termT = svts_->swapLength(termPb);
        // same as above, make sure the underlying term is at least >= 1 Month, but since Period::operator<
        // throws in certain circumstances, we do the comparison based on termT here:
        if (termT < 1.0 / 12.0) {
            termT = 1.0 / 12.0;
            termPb = 1 * Months;
        }
    }
}

Real LgmBuilder::getStrike(const Size j) const {
    DLOG("LgmBuilder::getStrike(" << j << "): '" << data_->optionStrikes()[j] << "'");
    Strike strike = parseStrike(data_->optionStrikes()[j]);
    Real strikeValue;
    // TODO: Extend strike type coverage
    if (strike.type == Strike::Type::ATM)
        strikeValue = Null<Real>();
    else if (strike.type == Strike::Type::Absolute)
        strikeValue = strike.value;
    else
        QL_FAIL("strike type ATM or Absolute expected");
    return strikeValue;
}

bool LgmBuilder::volSurfaceChanged(const bool updateCache) const {
    bool hasUpdated = false;

    // create cache if not equal to required size
    if (swaptionVolCache_.size() != swaptionBasket_.size())
        swaptionVolCache_ = vector<Real>(swaptionBasket_.size(), Null<Real>());

    Size swaptionCounter = 0;
    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        if (!swaptionActive_[j])
            continue;

        Real volCache = swaptionVolCache_.at(swaptionCounter);

        bool expiryDateBased, termDateBased;
        Period expiryPb, termPb;
        Date expiryDb, termDb;
        Real termT;

        getExpiryAndTerm(j, expiryPb, termPb, expiryDb, termDb, termT, expiryDateBased, termDateBased);
        Real strikeValue = swaptionStrike_.at(swaptionCounter);

        Real vol;
        if (expiryDateBased && termDateBased) {
            vol = svts_->volatility(expiryDb, termT, strikeValue);
        } else if (expiryDateBased && !termDateBased) {
            vol = svts_->volatility(expiryDb, termPb, strikeValue);
        } else if (!expiryDateBased && termDateBased) {
            vol = svts_->volatility(expiryPb, termT, strikeValue);
        } else {
            // !expiryDateBased && !termDateBased
            vol = svts_->volatility(expiryPb, termPb, strikeValue);
        }

        if (!close_enough(volCache, vol)) {
            if (updateCache)
                swaptionVolCache_[swaptionCounter] = vol;
            hasUpdated = true;
        }
        swaptionCounter++;
    }
    return hasUpdated;
}

void LgmBuilder::updateSwaptionBasketVols() const {
    for (Size j = 0; j < swaptionBasketVols_.size(); ++j)
        swaptionBasketVols_.at(j)->setValue(swaptionVolCache_.at(j));
}

void LgmBuilder::buildSwaptionBasket() const {

    DLOG("build swaption basket");

    QL_REQUIRE(data_->optionExpiries().size() == data_->optionTerms().size(), "swaption vector size mismatch");
    QL_REQUIRE(data_->optionExpiries().size() == data_->optionStrikes().size(), "swaption vector size mismatch");

    std::ostringstream log;

    std::vector<Time> expiryTimes;
    std::vector<Time> maturityTimes;
    swaptionBasket_.clear();
    swaptionBasketVols_.clear();
    swaptionVolCache_.clear();
    swaptionStrike_.clear();

    DLOG("build reference date grid '" << referenceCalibrationGrid_ << "'");
    Date lastRefCalDate = Date::minDate();
    std::vector<Date> referenceCalibrationDates;
    if (!referenceCalibrationGrid_.empty())
        referenceCalibrationDates = DateGrid(referenceCalibrationGrid_).dates();

    for (Size j = 0; j < data_->optionExpiries().size(); j++) {
        bool expiryDateBased, termDateBased;
        Period expiryPb, termPb;
        Date expiryDb, termDb;
        Real termT = Null<Real>();

        getExpiryAndTerm(j, expiryPb, termPb, expiryDb, termDb, termT, expiryDateBased, termDateBased);
        Real strikeValue = getStrike(j);

        // rounded to whole years, only used to distinguish between short and long
        // swap tenors, which in practice always are multiples of whole years
        Period termTmp = static_cast<Size>(termT + 0.5) * Years;
        auto iborIndex = termTmp > shortSwapIndex_->tenor() ? swapIndex_->iborIndex() : shortSwapIndex_->iborIndex();
        auto fixedLegTenor =
            termTmp > shortSwapIndex_->tenor() ? swapIndex_->fixedLegTenor() : shortSwapIndex_->fixedLegTenor();
        auto fixedDayCounter =
            termTmp > shortSwapIndex_->tenor() ? swapIndex_->dayCounter() : shortSwapIndex_->dayCounter();
        auto floatDayCounter = termTmp > shortSwapIndex_->tenor() ? swapIndex_->iborIndex()->dayCounter()
                                                                  : shortSwapIndex_->iborIndex()->dayCounter();
        Size settlementDays = Null<Size>();
        RateAveraging::Type averagingMethod = RateAveraging::Compound;
        if (auto on = dynamic_pointer_cast<OvernightIndexedSwapIndex>(*swapIndex_)) {
            settlementDays = on->fixingDays();
            averagingMethod = on->averagingMethod();
        }

        Real dummyQuote = svts_->volatilityType() == Normal ? 0.0020 : 0.10;
        auto volQuote = QuantLib::ext::make_shared<SimpleQuote>(dummyQuote);
        Handle<Quote> vol = Handle<Quote>(volQuote);
        QuantLib::ext::shared_ptr<SwaptionHelper> helper;
        Real updatedStrike;

        if (expiryDateBased && termDateBased) {
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryDb, termT) : 0.0;
            std::tie(helper, updatedStrike) = createSwaptionHelper(
                expiryDb, termDb, svts_, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter,
                calibrationDiscountCurve_, calibrationErrorType_, strikeValue, shift, settlementDays, averagingMethod);
        }
        if (expiryDateBased && !termDateBased) {
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryDb, termPb) : 0.0;
            std::tie(helper, updatedStrike) = createSwaptionHelper(
                expiryDb, termPb, svts_, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter,
                calibrationDiscountCurve_, calibrationErrorType_, strikeValue, shift, settlementDays, averagingMethod);
        }
        if (!expiryDateBased && termDateBased) {
            Date expiry = svts_->optionDateFromTenor(expiryPb);
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryPb, termT) : 0.0;
            std::tie(helper, updatedStrike) = createSwaptionHelper(
                expiry, termDb, svts_, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter,
                calibrationDiscountCurve_, calibrationErrorType_, strikeValue, shift, settlementDays, averagingMethod);
        }
        if (!expiryDateBased && !termDateBased) {
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryPb, termPb) : 0.0;
            std::tie(helper, updatedStrike) = createSwaptionHelper(
                expiryPb, termPb, svts_, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter,
                calibrationDiscountCurve_, calibrationErrorType_, strikeValue, shift, settlementDays, averagingMethod);
        }

        // check if we want to keep the helper when a reference calibration grid is given
        Date expiryDate = helper->swaption()->exercise()->date(0);
        auto refCalDate =
            std::lower_bound(referenceCalibrationDates.begin(), referenceCalibrationDates.end(), expiryDate);
        if (refCalDate == referenceCalibrationDates.end() || *refCalDate > lastRefCalDate) {
            swaptionActive_[j] = true;
            swaptionBasketVols_.push_back(volQuote);
            swaptionBasket_.push_back(helper);
            swaptionStrike_.push_back(updatedStrike);
            expiryTimes.push_back(calibrationDiscountCurve_->timeFromReference(expiryDate));
            Date matDate = helper->underlyingSwap() ? helper->underlyingSwap()->maturityDate()
                                                    : helper->underlyingOvernightIndexedSwap()->maturityDate();
            maturityTimes.push_back(calibrationDiscountCurve_->timeFromReference(matDate));
            if (refCalDate != referenceCalibrationDates.end())
                lastRefCalDate = *refCalDate;
        }
    }

    std::sort(expiryTimes.begin(), expiryTimes.end());
    auto itExpiryTime = unique(expiryTimes.begin(), expiryTimes.end());
    expiryTimes.resize(distance(expiryTimes.begin(), itExpiryTime));

    swaptionExpiries_ = Array(expiryTimes.size());
    for (Size j = 0; j < expiryTimes.size(); j++)
        swaptionExpiries_[j] = expiryTimes[j];

    std::sort(maturityTimes.begin(), maturityTimes.end());
    auto itMaturityTime = unique(maturityTimes.begin(), maturityTimes.end());
    maturityTimes.resize(distance(maturityTimes.begin(), itMaturityTime));

    swaptionMaturities_ = Array(maturityTimes.size());
    for (Size j = 0; j < maturityTimes.size(); j++)
        swaptionMaturities_[j] = maturityTimes[j];

    swaptionBasketRefDate_ = calibrationDiscountCurve_->referenceDate();
}

std::string LgmBuilder::getBasketDetails(LgmCalibrationInfo& info) const {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(16) << "expiry" << std::setw(16) << "swapLength"
        << std::setw(16) << "strike" << std::setw(16) << "atmForward" << std::setw(16) << "annuity" << std::setw(16)
        << "vega" << std::setw(16) << "vol\n";
    info.swaptionData.clear();
    for (Size j = 0; j < swaptionBasket_.size(); ++j) {
        auto swp = QuantLib::ext::static_pointer_cast<SwaptionHelper>(swaptionBasket_[j])->swaption();
        auto sd = swaptionData(swp, calibrationDiscountCurve_, svts_);
        log << std::right << std::setw(3) << j << std::setw(16) << sd.timeToExpiry << std::setw(16) << sd.swapLength
            << std::setw(16) << sd.strike << std::setw(16) << sd.atmForward << std::setw(16) << sd.annuity
            << std::setw(16) << sd.vega << std::setw(16) << std::setw(16) << sd.stdDev / std::sqrt(sd.timeToExpiry)
            << "\n";
        info.swaptionData.push_back(sd);
    }
    return log.str();
}

void LgmBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

} // namespace data
} // namespace ore
