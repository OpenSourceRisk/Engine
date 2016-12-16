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
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>

#include <qle/models/irlgm1fpiecewiseconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiselinearparametrization.hpp>
#include <qle/models/irlgm1fconstantparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ored/model/lgmbuilder.hpp>
#include <ored/model/utilities.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/strike.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
using namespace data;
namespace data {

LgmBuilder::LgmBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<LgmData>& data,
                       const std::string& configuration, Real bootstrapTolerance)
    : market_(market), configuration_(configuration), data_(data), bootstrapTolerance_(bootstrapTolerance),
      optimizationMethod_(boost::shared_ptr<OptimizationMethod>(new LevenbergMarquardt(1E-8, 1E-8, 1E-8))),
      endCriteria_(EndCriteria(1000, 500, 1E-8, 1E-8, 1E-8)),
      calibrationErrorType_(CalibrationHelper::RelativePriceError) {

    QuantLib::Currency ccy = parseCurrency(data_->ccy());
    LOG("LgmCalibration for ccy " << ccy << ", configuration is " << configuration_);

    discountCurve_ = RelinkableHandle<YieldTermStructure>(*market_->discountCurve(ccy.code(), configuration_));
    if (data_->calibrateA() || data_->calibrateH())
        buildSwaptionBasket();

    // convert vector<Real> to Array
    Array aTimes(data_->aTimes().begin(), data_->aTimes().end());
    Array hTimes(data_->hTimes().begin(), data_->hTimes().end());
    Array alpha(data_->aValues().begin(), data_->aValues().end());
    Array h(data_->hValues().begin(), data_->hValues().end());

    if (data_->aParamType() == ParamType::Constant) {
        QL_REQUIRE(data_->aTimes().size() == 0, "empty alpha times expected");
        QL_REQUIRE(data_->aValues().size() == 1, "initial alpha array should have size 1");
    } else if (data_->aParamType() == ParamType::Piecewise) {
        if (data_->calibrateA()) { // override
            if (data_->aTimes().size() > 0) {
                LOG("overriding alpha time grid with swaption expiries");
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
                LOG("overriding h time grid with swaption underlying maturities");
            }
            hTimes = swaptionMaturities_;
            h = Array(hTimes.size() + 1, data_->hValues()[0]); // constant array
        } else {                                               // use input time grid and input h array otherwise
            QL_REQUIRE(h.size() == hTimes.size() + 1, "H grids do not match");
        }
    } else
        QL_FAIL("H type case not covered");

    if (data_->aParamType() == ParamType::Piecewise) {
        if (data_->reversionType() == LgmData::ReversionType::HullWhite &&
            data_->volatilityType() == LgmData::VolatilityType::HullWhite) {
            LOG("IR parametrization for " << ccy << ": IrLgm1fPiecewiseConstantHullWhiteAdaptor");
            parametrization_ = boost::make_shared<QuantExt::IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
                ccy, discountCurve_, aTimes, alpha, hTimes, h);
        } else if (data_->reversionType() == LgmData::ReversionType::HullWhite) {
            LOG("IR parametrization for " << ccy << ": IrLgm1fPiecewiseConstant");
            parametrization_ = boost::make_shared<QuantExt::IrLgm1fPiecewiseConstantParametrization>(
                ccy, discountCurve_, aTimes, alpha, hTimes, h);
        } else {
            parametrization_ = boost::make_shared<QuantExt::IrLgm1fPiecewiseLinearParametrization>(
                ccy, discountCurve_, aTimes, alpha, hTimes, h);
            LOG("IR parametrization for " << ccy << ": IrLgm1fPiecewiseLinear");
        }
        LOG("alpha times size: " << aTimes.size());
        LOG("lambda times size: " << hTimes.size());
    } else {
        parametrization_ =
            boost::make_shared<QuantExt::IrLgm1fConstantParametrization>(ccy, discountCurve_, alpha[0], h[0]);
        LOG("IR parametrization for ccy " << ccy << ": IrLgm1fConstant");
        LOG("alpha times size: " << aTimes.size());
        LOG("lambda times size: " << hTimes.size());
    }

    model_ = boost::make_shared<QuantExt::LGM>(parametrization_);

    // attach pricing engines to helpers
    boost::shared_ptr<PricingEngine> swaptionEngine = boost::make_shared<QuantExt::AnalyticLgmSwaptionEngine>(model_);
    for (Size j = 0; j < swaptionBasket_.size(); j++)
        swaptionBasket_[j]->setPricingEngine(swaptionEngine);
}

void LgmBuilder::update() {

    if (data_->calibrationType() != CalibrationType::None) {
        if (data_->calibrateA() && !data_->calibrateH()) {
            if (data_->aParamType() == ParamType::Piecewise && data_->calibrationType() == CalibrationType::Bootstrap) {
                LOG("call calibrateVolatilitiesIterative for alpha calibration");
                model_->calibrateVolatilitiesIterative(swaptionBasket_, *optimizationMethod_, endCriteria_);
            } else {
                LOG("call calibrateGlobal for alpha calibration");
                model_->calibrate(swaptionBasket_, *optimizationMethod_, endCriteria_);
            }
        } else {
            LOG("call calibrateGlobal");
            model_->calibrate(swaptionBasket_, *optimizationMethod_, endCriteria_);
        }
        LOG("LGM " << data_->ccy() << " calibration errors:");
        error_ = logCalibrationErrors(swaptionBasket_, parametrization_);
        if (data_->calibrationType() == CalibrationType::Bootstrap) {
            QL_REQUIRE(fabs(error_) < bootstrapTolerance_, "calibration error " << error_ << " exceeds tolerance "
                                                                                << bootstrapTolerance_);
        }
    }

    LOG("Apply shift horizon and scale (if not 0.0 and 1.0 respectively)");

    QL_REQUIRE(data_->shiftHorizon() >= 0.0, "shift horizon must be non negative");
    QL_REQUIRE(data_->scaling() > 0.0, "scaling must be positive");

    parametrization_->shift() = 0.0;
    parametrization_->scaling() = 1.0;

    if (data_->shiftHorizon() > 0.0) {
        Real value = -parametrization_->H(data_->shiftHorizon());
        LOG("Apply shift horizon " << data_->shiftHorizon() << " (C=" << value << ") to the " << data_->ccy()
                                   << " LGM model");
        parametrization_->shift() = value;
    }

    if (data_->scaling() != 1.0) {
        LOG("Apply scaling " << data_->scaling() << " to the " << data_->ccy() << " LGM model");
        parametrization_->scaling() = data_->scaling();
    }
}

void LgmBuilder::buildSwaptionBasket() {
    QL_REQUIRE(data_->swaptionExpiries().size() == data_->swaptionTerms().size(), "swaption vector size mismatch");
    QL_REQUIRE(data_->swaptionExpiries().size() == data_->swaptionStrikes().size(), "swaption vector size mismatch");

    Handle<QuantLib::SwaptionVolatilityStructure> svts = market_->swaptionVol(data_->ccy(), configuration_);

    std::string swapIndexName = market_->swapIndexBase(data_->ccy(), configuration_);
    Handle<SwapIndex> swapIndex = market_->swapIndex(swapIndexName, configuration_);
    std::string shortSwapIndexName = market_->shortSwapIndexBase(data_->ccy(), configuration_);
    Handle<SwapIndex> shortSwapIndex = market_->swapIndex(shortSwapIndexName, configuration_);
    std::vector<Time> expiryTimes(data_->swaptionExpiries().size());
    std::vector<Time> maturityTimes(data_->swaptionTerms().size());
    swaptionBasket_.clear();
    for (Size j = 0; j < data_->swaptionExpiries().size(); j++) {
        std::string expiryString = data_->swaptionExpiries()[j];
        std::string termString = data_->swaptionTerms()[j];
        bool expiryDateBased, termDateBased;
        Period expiryPb, termPb;
        Date expiryDb, termDb;
        parseDateOrPeriod(expiryString, expiryDb, expiryPb, expiryDateBased);
        parseDateOrPeriod(termString, termDb, termPb, termDateBased);
        Strike strike = parseStrike(data_->swaptionStrikes()[j]);
        Real strikeValue;
        // TODO: Extend strike type coverage
        if (strike.type == Strike::Type::ATM)
            strikeValue = Null<Real>();
        else if (strike.type == Strike::Type::Absolute)
            strikeValue = strike.value;
        else
            QL_FAIL("strike type ATM or Absolute expected");

        Handle<Quote> vol;
        Handle<YieldTermStructure> yts = market_->discountCurve(data_->ccy(), configuration_);
        boost::shared_ptr<SwaptionHelper> helper;
        Real termT = Null<Real>();
        Period termTmp;
        if (termDateBased) {
            termT = svts->timeFromReference(termDb);
            // rounded to whole years, only used to distinguish between short and long
            // swap tenors, which in practice always are multiples of whole years
            termTmp = static_cast<Size>(termT + 0.5) * Years;
        }
        auto iborIndex = termTmp > shortSwapIndex->tenor() ? swapIndex->iborIndex() : shortSwapIndex->iborIndex();
        auto fixedLegTenor =
            termTmp > shortSwapIndex->tenor() ? swapIndex->fixedLegTenor() : shortSwapIndex->fixedLegTenor();
        auto fixedDayCounter =
            termTmp > shortSwapIndex->tenor() ? swapIndex->dayCounter() : shortSwapIndex->dayCounter();
        auto floatDayCounter = termTmp > shortSwapIndex->tenor() ? swapIndex->iborIndex()->dayCounter()
                                                                 : shortSwapIndex->iborIndex()->dayCounter();
        if (expiryDateBased && termDateBased) {
            vol = Handle<Quote>(boost::make_shared<SimpleQuote>(svts->volatility(expiryDb, termT, strikeValue)));
            Real shift = svts->volatilityType() == ShiftedLognormal ? svts->shift(expiryDb, termT) : 0.0;
            helper = boost::make_shared<SwaptionHelper>(
                expiryDb, termDb, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter, yts,
                calibrationErrorType_, strikeValue, 1.0, svts->volatilityType(), shift);
            LOG("Added Date / Date based SwaptionHelper " << data_->ccy() << " " << expiryDb << ", " << termDb << ", "
                                                          << strike << " : " << vol->value() << " "
                                                          << svts->volatilityType());
        }
        if (expiryDateBased && !termDateBased) {
            vol = Handle<Quote>(boost::make_shared<SimpleQuote>(svts->volatility(expiryDb, termPb, strikeValue)));
            Real shift = svts->volatilityType() == ShiftedLognormal ? svts->shift(expiryDb, termPb) : 0.0;
            helper = boost::make_shared<SwaptionHelper>(
                expiryDb, termPb, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter, yts,
                calibrationErrorType_, strikeValue, 1.0, svts->volatilityType(), shift);
            LOG("Added Date / Period based SwaptionHelper " << data_->ccy() << " " << expiryDb << ", " << termPb << ", "
                                                            << strike << " : " << vol->value() << " "
                                                            << svts->volatilityType());
        }
        if (!expiryDateBased && termDateBased) {
            Date expiry = svts->optionDateFromTenor(expiryPb);
            Real shift = svts->volatilityType() == ShiftedLognormal ? svts->shift(expiryPb, termT) : 0.0;
            vol = Handle<Quote>(boost::make_shared<SimpleQuote>(svts->volatility(expiryPb, termT, strikeValue)));
            helper = boost::make_shared<SwaptionHelper>(expiry, termDb, vol, iborIndex, fixedLegTenor, fixedDayCounter,
                                                        floatDayCounter, yts, calibrationErrorType_, strikeValue, 1.0,
                                                        svts->volatilityType(), shift);
            LOG("Added Period / Date based SwaptionHelper " << data_->ccy() << " " << expiryPb << ", " << termDb << ", "
                                                            << strike << " : " << vol->value() << " "
                                                            << svts->volatilityType());
        }
        if (!expiryDateBased && !termDateBased) {
            vol = Handle<Quote>(boost::make_shared<SimpleQuote>(svts->volatility(expiryPb, termPb, strikeValue)));
            Real shift = svts->volatilityType() == ShiftedLognormal ? svts->shift(expiryPb, termPb) : 0.0;
            helper = boost::make_shared<SwaptionHelper>(
                expiryPb, termPb, vol, iborIndex, fixedLegTenor, fixedDayCounter, floatDayCounter, yts,
                calibrationErrorType_, strikeValue, 1.0, svts->volatilityType(), shift);
            LOG("Added Period / Period based SwaptionHelper " << data_->ccy() << " " << expiryPb << ", " << termPb
                                                              << ", " << strike << " : " << vol->value() << " "
                                                              << svts->volatilityType());
        }
        swaptionBasket_.push_back(helper);
        expiryTimes[j] = yts->timeFromReference(helper->swaption()->exercise()->date(0));
        maturityTimes[j] = yts->timeFromReference(helper->underlyingSwap()->maturityDate());
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
}
}
}
