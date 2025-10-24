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

#include <ored/marketdata/market.hpp>
#include <ored/model/irmodelbuilder.hpp>
#include <ored/model/structuredmodelerror.hpp>
#include <ored/model/structuredmodelwarning.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

#include <qle/models/irmodel.hpp>
#include <qle/models/marketobserver.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace {

// Return swaption data
SwaptionData swaptionData(const QuantLib::ext::shared_ptr<SwaptionHelper>& h) {
    SwaptionData sd;
    h->blackPrice(h->volatility()->value());
    sd.timeToExpiry = h->timeToExpiry();
    sd.swapLength = h->swapLength();
    sd.strike = h->strike();
    sd.atmForward = h->atmForward();
    sd.annuity = h->annuity();
    sd.vega = h->vega();
    sd.stdDev = h->stdDev();
    return sd;
}

// Utility function to create swaption helper. Returns helper and (possibly updated) strike
template <typename E, typename T>
std::tuple<QuantLib::ext::shared_ptr<SwaptionHelper>, double, ore::data::IrModelBuilder::FallbackType>
createSwaptionHelper(const E& expiry, const T& term, const Handle<SwaptionVolatilityStructure>& svts,
                     const Handle<Quote>& vol, const QuantLib::ext::shared_ptr<IborIndex>& iborIndex,
                     const Period& fixedLegTenor, const DayCounter& fixedDayCounter, const DayCounter& floatDayCounter,
                     const Handle<YieldTermStructure>& yts, BlackCalibrationHelper::CalibrationErrorType errorType,
                     Real strike, Real shift, const Size settlementDays, const RateAveraging::Type averagingMethod) {

    DLOG("IrModelBuilder::createSwaptionHelper(" << expiry << ", " << term << ")");

    ore::data::IrModelBuilder::FallbackType fallbackType = ore::data::IrModelBuilder::FallbackType::NoFallback;

    auto vt = svts->volatilityType();
    auto helper = QuantLib::ext::make_shared<SwaptionHelper>(expiry, term, vol, iborIndex, fixedLegTenor,
                                                             fixedDayCounter, floatDayCounter, yts, errorType, strike,
                                                             1.0, vt, shift, settlementDays, averagingMethod);
    auto sd = swaptionData(helper);

    // ensure fallback rule 1

    Real atmStdDev = svts->volatility(sd.timeToExpiry, sd.swapLength, sd.atmForward) * std::sqrt(sd.timeToExpiry);
    if (vt == ShiftedLognormal) {
        atmStdDev *= sd.atmForward + shift;
    }
    if (strike != Null<Real>() &&
        std::abs(strike - sd.atmForward) > ore::data::IrModelBuilder::maxAtmStdDev * atmStdDev) {
        DLOG("Helper with expiry " << expiry << " and term " << term << " has a strike (" << strike
                                   << ") that is too far out of the money (atm = " << sd.atmForward
                                   << ", atmStdDev = " << atmStdDev << "). Adjusting the strike using maxAtmStdDev "
                                   << ore::data::IrModelBuilder::maxAtmStdDev);
        if (strike > sd.atmForward)
            strike = sd.atmForward + ore::data::IrModelBuilder::maxAtmStdDev * atmStdDev;
        else
            strike = sd.atmForward - ore::data::IrModelBuilder::maxAtmStdDev * atmStdDev;
        helper = QuantLib::ext::make_shared<SwaptionHelper>(expiry, term, vol, iborIndex, fixedLegTenor,
                                                            fixedDayCounter, floatDayCounter, yts, errorType, strike,
                                                            1.0, vt, shift, settlementDays, averagingMethod);
        fallbackType = ore::data::IrModelBuilder::FallbackType::FallbackRule1;
    }

    DLOG("Created swaption helper with expiry " << expiry << " and term " << term << ": vol=" << vol->value()
                                                << ", index=" << iborIndex->name() << ", strike=" << strike
                                                << ", shift=" << shift);

    return std::make_tuple(helper, strike, fallbackType);
}

} // namespace

namespace ore {
namespace data {

IrModelBuilder::IrModelBuilder(
    const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<IrModelData>& data,
    const std::vector<std::string>& optionExpiries, const std::vector<std::string>& optionTerms,
    const std::vector<std::string>& optionStrikes, const std::string& configuration, const Real bootstrapTolerance,
    const bool continueOnError, const std::string& referenceCalibrationGrid,
    BlackCalibrationHelper::CalibrationErrorType calibrationErrorType, const bool allowChangingFallbacksUnderScenarios,
    const bool allowModelFallbacks, const bool requiresCalibration, const bool dontCalibrate,
    const std::string& modelLabel, const std::string& id)
    : market_(market), configuration_(configuration), data_(data), optionExpiries_(optionExpiries),
      optionTerms_(optionTerms), optionStrikes_(optionStrikes), bootstrapTolerance_(bootstrapTolerance),
      continueOnError_(continueOnError), referenceCalibrationGrid_(referenceCalibrationGrid),
      calibrationErrorType_(calibrationErrorType),
      allowChangingFallbacksUnderScenarios_(allowChangingFallbacksUnderScenarios),
      allowModelFallbacks_(allowModelFallbacks), requiresCalibration_(requiresCalibration),
      dontCalibrate_(dontCalibrate), modelLabel_(modelLabel), id_(id),
      optimizationMethod_(QuantLib::ext::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8)) {

    LOG("IrModelBuilder called.");

    marketObserver_ = QuantLib::ext::make_shared<MarketObserver>();
    string qualifier = data_->qualifier();
    currency_ = qualifier;
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(qualifier, index)) {
        currency_ = index->currency().code();
    }
    LOG("IrModelBuilder: calibration for " << modelLabel_ << " and qualifier " << qualifier << " (ccy=" << currency_
                                           << "), configuration is " << configuration_);
    Currency ccy = parseCurrency(currency_);

    // try to get market objects, if sth fails, we fall back to a default and log a structured error

    Handle<YieldTermStructure> dummyYts(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));

    try {
        shortSwapIndex_ =
            market_->swapIndex(market_->shortSwapIndexBase(data_->qualifier(), configuration_), configuration_);
    } catch (const std::exception& e) {
        processException("short swap index", e);
        shortSwapIndex_ = Handle<SwapIndex>(QuantLib::ext::make_shared<SwapIndex>(
            "dummy", 30 * Years, 0, ccy, NullCalendar(), 1 * Years, Unadjusted, Actual365Fixed(),
            QuantLib::ext::make_shared<IborIndex>("dummy", 1 * Years, 0, ccy, NullCalendar(), Unadjusted, false,
                                                  Actual365Fixed(), dummyYts),
            dummyYts));
    }

    try {
        swapIndex_ = market_->swapIndex(market_->swapIndexBase(data_->qualifier(), configuration_), configuration_);
    } catch (const std::exception& e) {
        processException("swap index", e);
        swapIndex_ = Handle<SwapIndex>(QuantLib::ext::make_shared<SwapIndex>(
            "dummy", 30 * Years, 0, ccy, NullCalendar(), 1 * Years, Unadjusted, Actual365Fixed(),
            QuantLib::ext::make_shared<IborIndex>("dummy", 1 * Years, 0, ccy, NullCalendar(), Unadjusted, false,
                                                  Actual365Fixed(), dummyYts),
            dummyYts));
    }

    try {
        svts_ = market_->swaptionVol(data_->qualifier(), configuration_);
    } catch (const std::exception& e) {
        processException("swaption vol surface", e);
        svts_ = Handle<SwaptionVolatilityStructure>(QuantLib::ext::make_shared<ConstantSwaptionVolatility>(
            0, NullCalendar(), Unadjusted, 0.0010, Actual365Fixed(), Normal, 0.0));
    }

    // see the comment for dinscountCurve() in the interface
    modelDiscountCurve_ = RelinkableHandle<YieldTermStructure>(*swapIndex_->discountingTermStructure());
    calibrationDiscountCurve_ = Handle<YieldTermStructure>(*swapIndex_->discountingTermStructure());

    // check if weed calibration

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

    swaptionIndexInBasket_ = std::vector<Size>(optionExpiries_.size(), Null<Size>());

    if (requiresCalibration_) {
        buildSwaptionBasket(false);
    }
}

void IrModelBuilder::processException(const std::string& s, const std::exception& e) {
    std::string message = "Error while building IrModel " + modelLabel_ + " for qualifier '" + data_->qualifier() +
                          "', context '" + s + "'.";
    if (allowModelFallbacks_) {
        message += " Using a fallback, results depending on this object will be invalid.";
        StructuredModelErrorMessage(message, e.what(), id_).log();
    } else {
        message +=
            " Fallbacks are not allowed for this model builder (error: " + std::string(e.what()) + ", id: " + id_;
        QL_FAIL(message);
    }
}

Real IrModelBuilder::error() const {
    calculate();
    return error_;
}

QuantLib::ext::shared_ptr<QuantExt::IrModel> IrModelBuilder::model() const {
    calculate();
    return model_;
}

QuantLib::ext::shared_ptr<QuantExt::Parametrization> IrModelBuilder::parametrization() const {
    calculate();
    return parametrization_;
}

std::vector<QuantLib::ext::shared_ptr<BlackCalibrationHelper>> IrModelBuilder::swaptionBasket() const {
    calculate();
    return swaptionBasket_;
}

void IrModelBuilder::recalibrate() const {
    suspendCalibration_ = false;
    calculate();
}

void IrModelBuilder::newCalcWithoutRecalibration() const {
    suspendCalibration_ = true;
    calculate();
}

bool IrModelBuilder::requiresRecalibration() const {
    return requiresCalibration_ && !dontCalibrate_ &&
           (volSurfaceChanged(false) || marketObserver_->hasUpdated(false) || forceCalibration_) &&
           !suspendCalibration_;
}

void IrModelBuilder::performCalculations() const {

    DLOG("Recalibrate IR model " << modelLabel_ << " for qualifier " << data_->qualifier() << " currency "
                                 << currency_);

    initParametrization();

    if (!requiresRecalibration()) {
        DLOG("Skipping calibration as nothing has changed or calibration is not required.");
        return;
    }

    // reset lgm observer's updated flag
    marketObserver_->hasUpdated(true);

    // if reference date has changed we must rebuild the swaption basket, otherwise we can reuse the existing basket
    // except when a fallback rule in createSwaptionHelper() implies a change in the helper
    buildSwaptionBasket(swaptionBasketRefDate_ != calibrationDiscountCurve_->referenceDate());
    volSurfaceChanged(true);
    updateSwaptionBasketVols();

    for (Size j = 0; j < swaptionBasket_.size(); j++) {
        swaptionBasket_[j]->setPricingEngine(getPricingEngine());
        // necessary if notifications are disabled (observation mode = Disable)
        swaptionBasket_[j]->update();
    }

    // reset model parameters to ensure identical results on identical market data input
    model_->setParams(params_);

    // call into calibration routines
    calibrate();

} // performCalculations()

void IrModelBuilder::getExpiryAndTerm(const Size j, Period& expiryPb, Period& termPb, Date& expiryDb, Date& termDb,
                                      Real& termT, bool& expiryDateBased, bool& termDateBased) const {
    std::string expiryString = optionExpiries_[j];
    std::string termString = optionTerms_[j];
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

Real IrModelBuilder::getStrike(const Size j) const {
    DLOG("IrModelBuilder::getStrike(" << j << "): '" << optionStrikes_[j] << "'");
    Strike strike = parseStrike(optionStrikes_[j]);
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

bool IrModelBuilder::volSurfaceChanged(const bool updateCache) const {
    bool hasUpdated = false;

    // create cache if not equal to required size
    if (swaptionVolCache_.size() != swaptionBasket_.size())
        swaptionVolCache_ = vector<Real>(swaptionBasket_.size(), Null<Real>());

    for (Size j = 0; j < optionExpiries_.size(); j++) {
        if (swaptionIndexInBasket_[j] == Null<Size>())
            continue;

        Real volCache = swaptionVolCache_.at(swaptionIndexInBasket_[j]);

        bool expiryDateBased, termDateBased;
        Period expiryPb, termPb;
        Date expiryDb, termDb;
        Real termT;

        getExpiryAndTerm(j, expiryPb, termPb, expiryDb, termDb, termT, expiryDateBased, termDateBased);
        Real strikeValue = swaptionStrike_.at(swaptionIndexInBasket_[j]);

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
                swaptionVolCache_[swaptionIndexInBasket_[j]] = vol;
            hasUpdated = true;
        }
    }
    return hasUpdated;
}

void IrModelBuilder::updateSwaptionBasketVols() const {
    for (Size j = 0; j < swaptionBasketVols_.size(); ++j)
        swaptionBasketVols_.at(j)->setValue(swaptionVolCache_.at(j));
}

void IrModelBuilder::buildSwaptionBasket(const bool enforceFullRebuild) const {

    bool fullRebuild = enforceFullRebuild || swaptionBasket_.empty();

    DLOG("build swaption basket (enforce full rebuild = " << std::boolalpha << enforceFullRebuild
                                                          << ", effective full rebuild = " << fullRebuild << ")")

    Date lastRefCalDate = Date::minDate();
    std::vector<Date> referenceCalibrationDates;

    if (fullRebuild) {
        QL_REQUIRE(optionExpiries_.size() == optionTerms_.size(), "swaption vector size mismatch");
        QL_REQUIRE(optionExpiries_.size() == optionStrikes_.size(), "swaption vector size mismatch");
        swaptionBasket_.clear();
        swaptionBasketVols_.clear();
        swaptionStrike_.clear();
        swaptionExpiries_.clear();
        swaptionFallbackType_.clear();
        swaptionMaturities_.clear();
        swaptionVolCache_.clear();
        DLOG("build reference date grid '" << referenceCalibrationGrid_ << "'");
        if (!referenceCalibrationGrid_.empty())
            referenceCalibrationDates = DateGrid(referenceCalibrationGrid_).dates();
    }

    for (Size j = 0; j < optionExpiries_.size(); j++) {

        if (!fullRebuild && swaptionIndexInBasket_[j] == Null<Size>())
            continue;

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

        QuantLib::ext::shared_ptr<SimpleQuote> volQuote =
            fullRebuild ? QuantLib::ext::make_shared<SimpleQuote>(0) : swaptionBasketVols_[swaptionIndexInBasket_[j]];
        Handle<Quote> vol = Handle<Quote>(volQuote);

        QuantLib::ext::shared_ptr<SwaptionHelper> helper;
        Real updatedStrike;
        FallbackType fallbackType;

        if (expiryDateBased && termDateBased) {
            double v = svts_->volatility(expiryDb, termT, strikeValue);
            volQuote->setValue(v);
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryDb, termT) : 0.0;
            std::tie(helper, updatedStrike, fallbackType) = createSwaptionHelper(
                expiryDb, termDb, svts_, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter,
                calibrationDiscountCurve_, calibrationErrorType_, strikeValue, shift, settlementDays, averagingMethod);
        }
        if (expiryDateBased && !termDateBased) {
            double v = svts_->volatility(expiryDb, termPb, strikeValue);
            volQuote->setValue(v);
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryDb, termPb) : 0.0;
            std::tie(helper, updatedStrike, fallbackType) = createSwaptionHelper(
                expiryDb, termPb, svts_, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter,
                calibrationDiscountCurve_, calibrationErrorType_, strikeValue, shift, settlementDays, averagingMethod);
        }
        if (!expiryDateBased && termDateBased) {
            double v = svts_->volatility(expiryPb, termT, strikeValue);
            volQuote->setValue(v);
            Date expiry = svts_->optionDateFromTenor(expiryPb);
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryPb, termT) : 0.0;
            std::tie(helper, updatedStrike, fallbackType) = createSwaptionHelper(
                expiry, termDb, svts_, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter,
                calibrationDiscountCurve_, calibrationErrorType_, strikeValue, shift, settlementDays, averagingMethod);
        }
        if (!expiryDateBased && !termDateBased) {
            double v = svts_->volatility(expiryPb, termPb, strikeValue);
            volQuote->setValue(v);
            Real shift = svts_->volatilityType() == ShiftedLognormal ? svts_->shift(expiryPb, termPb) : 0.0;
            std::tie(helper, updatedStrike, fallbackType) = createSwaptionHelper(
                expiryPb, termPb, svts_, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter,
                calibrationDiscountCurve_, calibrationErrorType_, strikeValue, shift, settlementDays, averagingMethod);
        }

        if (!fullRebuild && allowChangingFallbacksUnderScenarios_) {
            if (fallbackType == swaptionFallbackType_[swaptionIndexInBasket_[j]] &&
                QuantLib::close_enough(updatedStrike, swaptionStrike_[swaptionIndexInBasket_[j]])) {
                continue;
            }
            swaptionBasket_[swaptionIndexInBasket_[j]] = helper;
            swaptionFallbackType_[swaptionIndexInBasket_[j]] = fallbackType;
            swaptionStrike_[swaptionIndexInBasket_[j]] = updatedStrike;
            continue;
        }

        if (fullRebuild) {
            // check if we want to keep the helper when a reference calibration grid is given
            Date expiryDate = helper->swaption()->exercise()->date(0);
            auto refCalDate =
                std::lower_bound(referenceCalibrationDates.begin(), referenceCalibrationDates.end(), expiryDate);
            if (refCalDate == referenceCalibrationDates.end() || *refCalDate > lastRefCalDate) {
                swaptionIndexInBasket_[j] = swaptionBasket_.size();
                swaptionBasketVols_.push_back(volQuote);
                swaptionBasket_.push_back(helper);
                swaptionStrike_.push_back(updatedStrike);
                swaptionFallbackType_.push_back(fallbackType);
                swaptionExpiries_.insert(calibrationDiscountCurve_->timeFromReference(expiryDate));
                Date matDate = helper->underlying()->maturityDate();
                swaptionMaturities_.insert(calibrationDiscountCurve_->timeFromReference(matDate));
                if (refCalDate != referenceCalibrationDates.end())
                    lastRefCalDate = *refCalDate;
            }
        }
    }

    swaptionBasketRefDate_ = calibrationDiscountCurve_->referenceDate();
}

std::string IrModelBuilder::getBasketDetails(std::vector<SwaptionData>& swaptionData) const {
    std::ostringstream log;
    log << std::right << std::setw(3) << "#" << std::setw(16) << "expiry" << std::setw(16) << "swapLength"
        << std::setw(16) << "strike" << std::setw(16) << "atmForward" << std::setw(16) << "annuity" << std::setw(16)
        << "vega" << std::setw(16) << "vol\n";
    swaptionData.clear();
    for (Size j = 0; j < swaptionBasket_.size(); ++j) {
        auto swp = QuantLib::ext::static_pointer_cast<SwaptionHelper>(swaptionBasket_[j]);
        auto sd = ::swaptionData(swp);
        log << std::right << std::setw(3) << j << std::setw(16) << sd.timeToExpiry << std::setw(16) << sd.swapLength
            << std::setw(16) << sd.strike << std::setw(16) << sd.atmForward << std::setw(16) << sd.annuity
            << std::setw(16) << sd.vega << std::setw(16) << std::setw(16) << sd.stdDev / std::sqrt(sd.timeToExpiry)
            << "\n";
        swaptionData.push_back(sd);
    }
    return log.str();
}

void IrModelBuilder::forceRecalculate() {
    forceCalibration_ = true;
    ModelBuilder::forceRecalculate();
    forceCalibration_ = false;
}

} // namespace data
} // namespace ore
