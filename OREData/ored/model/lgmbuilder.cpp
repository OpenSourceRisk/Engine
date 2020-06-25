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
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ored/model/lgmbuilder.hpp>
#include <ored/model/marketobserver.hpp>
#include <ored/model/structuredmodelerror.hpp>
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

LgmBuilder::LgmBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<IrLgmData>& data,
                       const std::string& configuration, const Real bootstrapTolerance, const bool continueOnError,
                       const std::string& referenceCalibrationGrid)
    : market_(market), configuration_(configuration), data_(data), bootstrapTolerance_(bootstrapTolerance),
      continueOnError_(continueOnError), referenceCalibrationGrid_(referenceCalibrationGrid),
      optimizationMethod_(boost::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8)),
      calibrationErrorType_(BlackCalibrationHelper::RelativePriceError) {

    marketObserver_ = boost::make_shared<MarketObserver>();
    QuantLib::Currency ccy = parseCurrency(data_->ccy());
    LOG("LgmCalibration for ccy " << ccy << ", configuration is " << configuration_);

    // the discount curve underlying the model might be relinked to a different curve outside this builder
    // the calibration curve should always stay the same though, therefore we create a different handle for this
    modelDiscountCurve_ = RelinkableHandle<YieldTermStructure>(*market_->discountCurve(ccy.code(), configuration_));
    calibrationDiscountCurve_ = Handle<YieldTermStructure>(*modelDiscountCurve_);

    if (data_->calibrateA() || data_->calibrateH()) {
        svts_ = market_->swaptionVol(data_->ccy(), configuration_);
        swapIndex_ = market_->swapIndex(market_->swapIndexBase(data_->ccy(), configuration_), configuration_);
        shortSwapIndex_ = market_->swapIndex(market_->shortSwapIndexBase(data_->ccy(), configuration_), configuration_);
        registerWith(svts_);
        marketObserver_->addObservable(swapIndex_->forwardingTermStructure());
        marketObserver_->addObservable(swapIndex_->discountingTermStructure());
        marketObserver_->addObservable(shortSwapIndex_->forwardingTermStructure());
        marketObserver_->addObservable(shortSwapIndex_->discountingTermStructure());
    }
    marketObserver_->addObservable(calibrationDiscountCurve_);
    registerWith(marketObserver_);
    // notify observers of all market data changes, not only when not calculated
    alwaysForwardNotifications();

    swaptionActive_ = std::vector<bool>(data_->optionExpiries().size(), false);

    buildSwaptionBasket();

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
                DLOG("overriding alpha time grid with swaption expiries");
            }
            QL_REQUIRE(swaptionExpiries_.size() > 0, "empty swaptionExpiries");
            aTimes = Array(swaptionExpiries_.begin(), swaptionExpiries_.end() - 1);
            alpha = Array(aTimes.size() + 1, data_->aValues()[0]); // constant array
        } else { // use input time grid and input alpha array otherwise
            QL_REQUIRE(alpha.size() == aTimes.size() + 1, "alpha grids do not match");
        }
    } else
        QL_FAIL("Alpha type case not covered");

    if (data_->hParamType() == ParamType::Constant) {
        QL_REQUIRE(data_->hValues().size() == 1, "reversion grid size 1 expected");
        QL_REQUIRE(data_->hTimes().size() == 0, "empty reversion time grid expected");
    } else if (data_->hParamType() == ParamType::Piecewise) {
        if (data_->calibrateH()) { // override
            if (data_->hTimes().size() > 0) {
                DLOG("overriding h time grid with swaption underlying maturities");
            }
            hTimes = swaptionMaturities_;
            h = Array(hTimes.size() + 1, data_->hValues()[0]); // constant array
        } else {                                               // use input time grid and input h array otherwise
            QL_REQUIRE(h.size() == hTimes.size() + 1, "H grids do not match");
        }
    } else
        QL_FAIL("H type case not covered");

    DLOG("before calibration: alpha times = " << aTimes << " values = " << alpha);
    DLOG("before calibration:     h times = " << hTimes << " values = " << h);

    if (data_->reversionType() == LgmData::ReversionType::HullWhite &&
        data_->volatilityType() == LgmData::VolatilityType::HullWhite) {
        DLOG("IR parametrization for " << ccy << ": IrLgm1fPiecewiseConstantHullWhiteAdaptor");
        parametrization_ = boost::make_shared<QuantExt::IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
    } else if (data_->reversionType() == LgmData::ReversionType::HullWhite &&
               data_->volatilityType() == LgmData::VolatilityType::Hagan) {
        DLOG("IR parametrization for " << ccy << ": IrLgm1fPiecewiseConstant");
        parametrization_ = boost::make_shared<QuantExt::IrLgm1fPiecewiseConstantParametrization>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
    } else if (data_->reversionType() == LgmData::ReversionType::Hagan &&
               data_->volatilityType() == LgmData::VolatilityType::Hagan) {
        parametrization_ = boost::make_shared<QuantExt::IrLgm1fPiecewiseLinearParametrization>(
            ccy, modelDiscountCurve_, aTimes, alpha, hTimes, h);
        DLOG("IR parametrization for " << ccy << ": IrLgm1fPiecewiseLinear");
    } else {
        QL_FAIL("Reversion type Hagan and volatility type HullWhite not covered");
    }
    DLOG("alpha times size: " << aTimes.size());
    DLOG("lambda times size: " << hTimes.size());

    model_ = boost::make_shared<QuantExt::LGM>(parametrization_);
    params_ = model_->params();
}

Real LgmBuilder::error() const {
    calculate();
    return error_;
}

boost::shared_ptr<QuantExt::LGM> LgmBuilder::model() const {
    calculate();
    return model_;
}

boost::shared_ptr<QuantExt::IrLgm1fParametrization> LgmBuilder::parametrization() const {
    calculate();
    return parametrization_;
}

std::vector<boost::shared_ptr<BlackCalibrationHelper>> LgmBuilder::swaptionBasket() const {
    calculate();
    return swaptionBasket_;
}

bool LgmBuilder::requiresRecalibration() const {
    return (data_->calibrateA() || data_->calibrateH()) &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_);
}

void LgmBuilder::performCalculations() const {

    DLOG("Recalibrate LGM model for currency " << data_->ccy());

    if (requiresRecalibration()) {

        // reset lgm observer's updated flag
        marketObserver_->hasUpdated(true);

        if ((data_->calibrateA() || data_->calibrateH())) {
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
        }

        for (Size j = 0; j < swaptionBasket_.size(); j++) {
            auto engine = boost::make_shared<QuantExt::AnalyticLgmSwaptionEngine>(model_, calibrationDiscountCurve_);
            engine->enableCache(!data_->calibrateH(), !data_->calibrateA());
            swaptionBasket_[j]->setPricingEngine(engine);
            // necessary if notifications are disabled (observation mode = Disable)
            swaptionBasket_[j]->update();
        }

        // reset model parameters, this ensures that a calibration gives the same
        // result if the input market data is the same
        model_->setParams(params_);

        if (data_->calibrationType() != CalibrationType::None) {
            if (data_->calibrateA() && !data_->calibrateH()) {
                if (data_->aParamType() == ParamType::Piecewise &&
                    data_->calibrationType() == CalibrationType::Bootstrap) {
                    DLOG("call calibrateVolatilitiesIterative for alpha calibration");
                    model_->calibrateVolatilitiesIterative(swaptionBasket_, *optimizationMethod_, endCriteria_);
                } else {
                    DLOG("call calibrateGlobal for alpha calibration");
                    model_->calibrate(swaptionBasket_, *optimizationMethod_, endCriteria_);
                }
            } else {
                if (!data_->calibrateA() && !data_->calibrateH()) {
                    DLOG("skip LGM calibration (both calibrate volatility and reversion are false)");
                } else {
                    DLOG("call calibrateGlobal");
                    model_->calibrate(swaptionBasket_, *optimizationMethod_, endCriteria_);
                }
            }
            TLOG("LGM " << data_->ccy() << " calibration errors:");
            error_ = getCalibrationError(swaptionBasket_);
            if (data_->calibrationType() == CalibrationType::Bootstrap &&
                (data_->calibrateA() || data_->calibrateH())) {
                if (fabs(error_) < bootstrapTolerance_) {
                    // we check the log level here to avoid unncessary computations
                    if (Log::instance().filter(ORE_DATA)) {
                        TLOGGERSTREAM << "Basket details:";
                        TLOGGERSTREAM << getBasketDetails();
                        TLOGGERSTREAM << "Calibration details:";
                        TLOGGERSTREAM << getCalibrationDetails(swaptionBasket_, parametrization_);
                        TLOGGERSTREAM << "rmse = " << error_;
                    }
                } else {
                    std::string exceptionMessage = "LGM (" + data_->ccy() + ") calibration error " +
                                                   std::to_string(error_) + " exceeds tolerance " +
                                                   std::to_string(bootstrapTolerance_);
                    WLOG(StructuredModelErrorMessage("Failed to calibrate LGM Model", exceptionMessage));
                    WLOGGERSTREAM << "Basket details:";
                    WLOGGERSTREAM << getBasketDetails();
                    WLOGGERSTREAM << "Calibration details:";
                    WLOGGERSTREAM << getCalibrationDetails(swaptionBasket_, parametrization_);
                    WLOGGERSTREAM << "rmse = " << error_;
                    if (!continueOnError_) {
                        QL_FAIL(exceptionMessage);
                    }
                }
            }
        } else {
            DLOG("skip LGM calibration (calibration type is none)");
        }

        DLOG("Apply shift horizon and scale (if not 0.0 and 1.0 respectively)");

        QL_REQUIRE(data_->shiftHorizon() >= 0.0, "shift horizon must be non negative");
        QL_REQUIRE(data_->scaling() > 0.0, "scaling must be positive");

        parametrization_->shift() = 0.0;
        parametrization_->scaling() = 1.0;

        if (data_->shiftHorizon() > 0.0) {
            Real value = -parametrization_->H(data_->shiftHorizon());
            DLOG("Apply shift horizon " << data_->shiftHorizon() << " (C=" << value << ") to the " << data_->ccy()
                                        << " LGM model");
            parametrization_->shift() = value;
        }

        if (data_->scaling() != 1.0) {
            DLOG("Apply scaling " << data_->scaling() << " to the " << data_->ccy() << " LGM model");
            parametrization_->scaling() = data_->scaling();
        }
    } else {
        DLOG("Skipping calibration as nothing has changed");
    }
}

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
        // the black swaption engine during calibration helper pricing; also notice that we use the swap legnth
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
        Real strikeValue = getStrike(j);

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

    // minimum allowed market value of helper before switching to PriceError
    static constexpr Real minMarketValue = 1.0E-8;

    Handle<YieldTermStructure> yts = market_->discountCurve(data_->ccy(), configuration_);

    std::vector<Time> expiryTimes;
    std::vector<Time> maturityTimes;
    swaptionBasket_.clear();
    swaptionBasketVols_.clear();
    swaptionVolCache_.clear();

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

        Real dummyQuote = svts_->volatilityType() == Normal ? 0.0020 : 0.10;
        auto volQuote = boost::make_shared<SimpleQuote>(dummyQuote);
        Handle<Quote> vol = Handle<Quote>(volQuote);
        boost::shared_ptr<SwaptionHelper> helper;

        if (expiryDateBased && termDateBased) {
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryDb, termT) : 0.0;
            helper = boost::make_shared<SwaptionHelper>(expiryDb, termDb, vol, iborIndex, fixedLegTenor,
                                                        fixedDayCounter, floatDayCounter, yts, calibrationErrorType_,
                                                        strikeValue, 1.0, svts_->volatilityType(), shift);
            if (std::abs(helper->marketValue()) < minMarketValue &&
                (calibrationErrorType_ == BlackCalibrationHelper::RelativePriceError ||
                 calibrationErrorType_ == BlackCalibrationHelper::ImpliedVolError)) {
                helper = boost::make_shared<SwaptionHelper>(
                    expiryDb, termDb, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter, yts,
                    BlackCalibrationHelper::PriceError, strikeValue, 1.0, svts_->volatilityType(), shift);
            }
        }
        if (expiryDateBased && !termDateBased) {
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryDb, termPb) : 0.0;
            helper = boost::make_shared<SwaptionHelper>(expiryDb, termPb, vol, iborIndex, fixedLegTenor,
                                                        fixedDayCounter, floatDayCounter, yts, calibrationErrorType_,
                                                        strikeValue, 1.0, svts_->volatilityType(), shift);
            if (std::abs(helper->marketValue()) < minMarketValue &&
                (calibrationErrorType_ == BlackCalibrationHelper::RelativePriceError ||
                 calibrationErrorType_ == BlackCalibrationHelper::ImpliedVolError)) {
                helper = boost::make_shared<SwaptionHelper>(
                    expiryDb, termPb, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter, yts,
                    BlackCalibrationHelper::PriceError, strikeValue, 1.0, svts_->volatilityType(), shift);
            }
        }
        if (!expiryDateBased && termDateBased) {
            Date expiry = svts_->optionDateFromTenor(expiryPb);
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryPb, termT) : 0.0;
            helper = boost::make_shared<SwaptionHelper>(expiry, termDb, vol, iborIndex, fixedLegTenor, fixedDayCounter,
                                                        floatDayCounter, yts, calibrationErrorType_, strikeValue, 1.0,
                                                        svts_->volatilityType(), shift);
            if (std::abs(helper->marketValue()) < minMarketValue &&
                (calibrationErrorType_ == BlackCalibrationHelper::RelativePriceError ||
                 calibrationErrorType_ == BlackCalibrationHelper::ImpliedVolError)) {
                helper = boost::make_shared<SwaptionHelper>(
                    expiry, termDb, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter, yts,
                    BlackCalibrationHelper::PriceError, strikeValue, 1.0, svts_->volatilityType(), shift);
            }
        }
        if (!expiryDateBased && !termDateBased) {
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryPb, termPb) : 0.0;
            helper = boost::make_shared<SwaptionHelper>(expiryPb, termPb, vol, iborIndex, fixedLegTenor,
                                                        fixedDayCounter, floatDayCounter, yts, calibrationErrorType_,
                                                        strikeValue, 1.0, svts_->volatilityType(), shift);
            if (std::abs(helper->marketValue()) < minMarketValue &&
                (calibrationErrorType_ == BlackCalibrationHelper::RelativePriceError ||
                 calibrationErrorType_ == BlackCalibrationHelper::ImpliedVolError)) {
                helper = boost::make_shared<SwaptionHelper>(
                    expiryPb, termPb, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter, yts,
                    BlackCalibrationHelper::PriceError, strikeValue, 1.0, svts_->volatilityType(), shift);
            }
        }

        // check if we want to keep the helper when a reference calibration grid is given
        Date expiryDate = helper->swaption()->exercise()->date(0);
        auto refCalDate =
            std::lower_bound(referenceCalibrationDates.begin(), referenceCalibrationDates.end(), expiryDate);
        if (refCalDate == referenceCalibrationDates.end() || *refCalDate > lastRefCalDate) {
            swaptionActive_[j] = true;
            swaptionBasketVols_.push_back(volQuote);
            swaptionBasket_.push_back(helper);
            expiryTimes.push_back(yts->timeFromReference(expiryDate));
            maturityTimes.push_back(yts->timeFromReference(helper->underlyingSwap()->maturityDate()));
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

std::string LgmBuilder::getBasketDetails() const {
    std::ostringstream log;
    boost::shared_ptr<PricingEngine> tmpEngine;
    Handle<YieldTermStructure> yts = market_->discountCurve(data_->ccy(), configuration_);
    switch (svts_->volatilityType()) {
    case ShiftedLognormal:
        tmpEngine = ext::make_shared<BlackSwaptionEngine>(yts, svts_);
        break;
    case Normal:
        tmpEngine = ext::make_shared<BachelierSwaptionEngine>(yts, svts_);
        break;
    default:
        QL_FAIL("can not construct engine: " << svts_->volatilityType());
        break;
    }
    log << std::right << std::setw(3) << "#" << std::setw(16) << "expiry" << std::setw(16) << "swapLength"
        << std::setw(16) << "strike" << std::setw(16) << "atmForward" << std::setw(16) << "annuity" << std::setw(16)
        << "vega" << std::setw(16) << "vol\n";
    for (Size j = 0; j < swaptionBasket_.size(); ++j) {
        auto swp = boost::static_pointer_cast<SwaptionHelper>(swaptionBasket_[j])->swaption();
        swp->setPricingEngine(tmpEngine);
        Real t = svts_->timeFromReference(swp->exercise()->dates().back());
        log << std::right << std::setw(3) << j << std::setw(16) << t << std::setw(16) << swp->result<Real>("swapLength")
            << std::setw(16) << swp->result<Real>("strike") << std::setw(16) << swp->result<Real>("atmForward")
            << std::setw(16) << swp->result<Real>("annuity") << std::setw(16) << swp->result<Real>("vega")
            << std::setw(16) << std::setw(16) << swp->result<Real>("stdDev") / std::sqrt(t) << "\n";
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
