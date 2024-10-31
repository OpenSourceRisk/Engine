/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/riskparticipationagreement.hpp>

#include <ored/scripting/engines/analyticblackriskparticipationagreementengine.hpp>
#include <ored/scripting/engines/analyticxccyblackriskparticipationagreementengine.hpp>
#include <ored/scripting/engines/numericlgmriskparticipationagreementengine.hpp>
#include <ored/scripting/engines/numericlgmriskparticipationagreementengine_tlock.hpp>

#include <ored/model/lgmbuilder.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

namespace ore {
namespace data {

std::map<std::string, Handle<YieldTermStructure>>
RiskParticipationAgreementEngineBuilderBase::getDiscountCurves(RiskParticipationAgreement* rpa) {
    std::map<std::string, Handle<YieldTermStructure>> curves;
    for (auto const& ccy : rpa->legCurrencies()) {
        curves[ccy] = market_->discountCurve(ccy, configuration(MarketContext::pricing));
    }
    return curves;
}

std::map<std::string, Handle<Quote>>
RiskParticipationAgreementEngineBuilderBase::getFxSpots(RiskParticipationAgreement* rpa) {
    std::map<std::string, Handle<Quote>> fxSpots;
    for (auto const& ccy : rpa->legCurrencies()) {
        fxSpots[ccy] = market_->fxRate(ccy + rpa->npvCurrency(), configuration(MarketContext::pricing));
    }
    return fxSpots;
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
RiskParticipationAgreementBlackEngineBuilder::engineImpl(const std::string& id, RiskParticipationAgreement* rpa) {
    Size maxDiscretisationPoints = parseInteger(engineParameter("MaxDiscretisationPoints"));
    if (maxDiscretisationPoints == 0)
        maxDiscretisationPoints = Null<Size>();
    string config = configuration(MarketContext::pricing);
    // the first ibor / ois index found
    QuantLib::ext::shared_ptr<IborIndex> index;
    auto qlInstr = QuantLib::ext::dynamic_pointer_cast<QuantExt::RiskParticipationAgreement>(rpa->instrument()->qlInstrument());
    QL_REQUIRE(qlInstr != nullptr, "RiskParticipationAgreementBlackEngineBuilder: internal error, could not "
                                   "cast to RiskParticipationAgreement");
    for (auto const& l : qlInstr->underlying()) {
        for (auto const& c : l) {
            if (auto floatingCpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(c)) {
                if (index == nullptr)
                    index = QuantLib::ext::dynamic_pointer_cast<IborIndex>(floatingCpn->index());
            }
        }
    }

    auto key = index == nullptr ? rpa->npvCurrency() : IndexNameTranslator::instance().oreName(index->name());
    return QuantLib::ext::make_shared<AnalyticBlackRiskParticipationAgreementEngine>(
        rpa->npvCurrency(), getDiscountCurves(rpa), getFxSpots(rpa),
        market_->defaultCurve(rpa->creditCurveId(), config)->curve(),
        market_->recoveryRate(rpa->creditCurveId(), config), market_->swaptionVol(key, config),
        *market_->swapIndex(market_->swapIndexBase(key, config)),
        parseBool(modelParameter("MatchUnderlyingTenor", {}, false, "false")),
        parseReal(
            modelParameter("Reversion", {IndexNameTranslator::instance().oreName(index->name()), rpa->npvCurrency()})),
        parseBool(engineParameter("AlwaysRecomputeOptionRepresentation")), parseInteger(engineParameter("MaxGapDays")),
        maxDiscretisationPoints);
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
RiskParticipationAgreementXCcyBlackEngineBuilder::engineImpl(const std::string& id, RiskParticipationAgreement* rpa) {
    Size maxDiscretisationPoints = parseInteger(engineParameter("MaxDiscretisationPoints"));
    if (maxDiscretisationPoints == 0)
        maxDiscretisationPoints = Null<Size>();
    string config = configuration(MarketContext::pricing);
    string ccyPair;
    for (auto const& c : rpa->legCurrencies()) {
        // the engine will check that there are exactly two underlying leg currencies, so here we can just look
        // for the first leg currency != npvCurrency and still be sure that the correct FX Vol will be applied
        if (c != rpa->npvCurrency()) {
            ccyPair = c + rpa->npvCurrency();
            break;
        }
    }
    QL_REQUIRE(!ccyPair.empty(),
               "RiskParticipationAgreementXCcyBlackEngineBuilder: no foreign currency found, this is unexpected");
    return QuantLib::ext::make_shared<AnalyticXCcyBlackRiskParticipationAgreementEngine>(
        rpa->npvCurrency(), getDiscountCurves(rpa), getFxSpots(rpa),
        market_->defaultCurve(rpa->creditCurveId(), config)->curve(),
        market_->recoveryRate(rpa->creditCurveId(), config), market_->fxVol(ccyPair, config),
        parseBool(engineParameter("AlwaysRecomputeOptionRepresentation")), parseInteger(engineParameter("MaxGapDays")),
        maxDiscretisationPoints);
}

QuantLib::ext::shared_ptr<QuantExt::LGM>
RiskParticipationAgreementLGMGridEngineBuilder::model(const string& id, const string& key,
                                                      const std::vector<Date>& expiries, const Date& maturity,
                                                      const std::vector<Real>& strikes) {

    // TODO this is the same as in LGMBermudanSwaptionEngineBuilder::model(), factor the model building out

    DLOG("Get model data");
    auto calibration = parseCalibrationType(modelParameter("Calibration"));
    auto calibrationStrategy = parseCalibrationStrategy(modelParameter("CalibrationStrategy"));
    std::string referenceCalibrationGrid = modelParameter("ReferenceCalibrationGrid", {}, false, "");
    Real lambda = parseReal(modelParameter("Reversion"));
    vector<Real> sigma = parseListOfValues<Real>(modelParameter("Volatility"), &parseReal);
    vector<Real> sigmaTimes = parseListOfValues<Real>(modelParameter("VolatilityTimes", {}, false), &parseReal);
    QL_REQUIRE(sigma.size() == sigmaTimes.size() + 1, "there must be n+1 volatilities (" << sigma.size()
                                                                                         << ") for n volatility times ("
                                                                                         << sigmaTimes.size() << ")");
    Real tolerance = parseReal(modelParameter("Tolerance"));
    auto reversionType = parseReversionType(modelParameter("ReversionType"));
    auto volatilityType = parseVolatilityType(modelParameter("VolatilityType"));
    bool continueOnCalibrationError = globalParameters_.count("ContinueOnCalibrationError") > 0 &&
                                      parseBool(globalParameters_.at("ContinueOnCalibrationError"));

    auto data = QuantLib::ext::make_shared<IrLgmData>();

    // check for allowed calibration / bermudan strategy settings
    std::vector<std::pair<CalibrationType, CalibrationStrategy>> validCalPairs = {
        {CalibrationType::None, CalibrationStrategy::None},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalATM},
        {CalibrationType::Bootstrap, CalibrationStrategy::CoterminalDealStrike},
        {CalibrationType::BestFit, CalibrationStrategy::CoterminalATM},
        {CalibrationType::BestFit, CalibrationStrategy::CoterminalDealStrike}};

    QL_REQUIRE(std::find(validCalPairs.begin(), validCalPairs.end(),
                         std::make_pair(calibration, calibrationStrategy)) != validCalPairs.end(),
               "Calibration (" << calibration << ") and CalibrationStrategy (" << calibrationStrategy
                               << ") are not allowed in this combination");

    // compute horizon shift
    Real shiftHorizon = parseReal(modelParameter("ShiftHorizon", {}, false, "0.5"));
    Date today = Settings::instance().evaluationDate();
    shiftHorizon = ActualActual(ActualActual::ISDA).yearFraction(today, maturity) * shiftHorizon;

    // Default: no calibration, constant lambda and sigma from engine configuration
    data->reset();
    data->qualifier() = key;
    data->calibrateH() = false;
    data->hParamType() = ParamType::Constant;
    data->hValues() = {lambda};
    data->reversionType() = reversionType;
    data->calibrateA() = false;
    data->aParamType() = ParamType::Piecewise;
    data->aValues() = sigma;
    data->aTimes() = sigmaTimes;
    data->volatilityType() = volatilityType;
    data->calibrationType() = calibration;
    data->shiftHorizon() = shiftHorizon;

    // calibration expiries might be empty, in this case do not calibrate
    if (!expiries.empty() && (calibrationStrategy == CalibrationStrategy::CoterminalATM ||
                              calibrationStrategy == CalibrationStrategy::CoterminalDealStrike)) {
        DLOG("Build LgmData for co-terminal specification");
        vector<string> expiryDates, termDates;
        for (Size i = 0; i < expiries.size(); ++i) {
            expiryDates.push_back(to_string(expiries[i]));
            termDates.push_back(to_string(maturity));
        }
        data->optionExpiries() = expiryDates;
        data->optionTerms() = termDates;
        data->optionStrikes().resize(expiryDates.size(), "ATM");
        if (calibrationStrategy == CalibrationStrategy::CoterminalDealStrike) {
            for (Size i = 0; i < expiryDates.size(); ++i) {
                if (strikes[i] != Null<Real>())
                    data->optionStrikes()[i] = std::to_string(strikes[i]);
            }
        }
        if (calibration == CalibrationType::Bootstrap) {
            DLOG("Calibrate piecewise alpha");
            data->calibrationType() = CalibrationType::Bootstrap;
            data->calibrateH() = false;
            data->hParamType() = ParamType::Constant;
            data->hValues() = {lambda};
            data->calibrateA() = true;
            data->aParamType() = ParamType::Piecewise;
            data->aValues() = {sigma};
        } else if (calibration == CalibrationType::BestFit) {
            DLOG("Calibrate constant sigma");
            data->calibrationType() = CalibrationType::BestFit;
            data->calibrateH() = false;
            data->hParamType() = ParamType::Constant;
            data->hValues() = {lambda};
            data->calibrateA() = true;
            data->aParamType() = ParamType::Constant;
            data->aValues() = {sigma};
        } else
            QL_FAIL("choice of calibration type invalid");
    }

    bool generateAdditionalResults = false;
    auto p = globalParameters_.find("GenerateAdditionalResults");
    if (p != globalParameters_.end()) {
        generateAdditionalResults = parseBool(p->second);
    }

    // Build model
    DLOG("Build LGM model");
    QuantLib::ext::shared_ptr<LgmBuilder> calib = QuantLib::ext::make_shared<LgmBuilder>(
        market_, data, configuration(MarketContext::irCalibration), tolerance, continueOnCalibrationError,
        referenceCalibrationGrid, generateAdditionalResults, id);

    // In some cases, we do not want to calibrate the model
    QuantLib::ext::shared_ptr<QuantExt::LGM> model;
    if (globalParameters_.count("Calibrate") == 0 || parseBool(globalParameters_.at("Calibrate"))) {
        DLOG("Calibrate model (configuration " << configuration(MarketContext::irCalibration) << ")");
        model = calib->model();
    } else {
        DLOG("Skip calibration of model based on global parameters");
        calib->freeze();
        model = calib->model();
        calib->unfreeze();
    }
    modelBuilders_.insert(std::make_pair(id, calib));

    return model;
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
RiskParticipationAgreementSwapLGMGridEngineBuilder::engineImpl(const std::string& id, RiskParticipationAgreement* rpa) {

    DLOG("Get engine data");
    Real sy = parseReal(engineParameter("sy"));
    Size ny = parseInteger(engineParameter("ny"));
    Real sx = parseReal(engineParameter("sx"));
    Size nx = parseInteger(engineParameter("nx"));
    Size maxGapDays = parseInteger(engineParameter("MaxGapDays"));
    Size maxDiscretisationPoints = parseInteger(engineParameter("MaxDiscretisationPoints"));
    if (maxDiscretisationPoints == 0)
        maxDiscretisationPoints = Null<Size>();

    // determine expiries and strikes for calibration basket (simple approach, a la summit)

    auto qlInstr = QuantLib::ext::dynamic_pointer_cast<QuantExt::RiskParticipationAgreement>(rpa->instrument()->qlInstrument());
    QL_REQUIRE(qlInstr != nullptr, "RiskParticipationAgreementSwapLGMGridEngineBuilder: internal error, could not "
                                   "cast to RiskParticipationAgreement");

    std::vector<Date> expiries;
    std::vector<Real> strikes;

    Date today = Settings::instance().evaluationDate();
    Date calibrationMaturity = std::max(qlInstr->underlyingMaturity(), today);

    // the first ibor / ois index found
    QuantLib::ext::shared_ptr<IborIndex> index;

    // if protection end <= today there is no model dependent part to value (just fees, possibly), so
    // we just pass a dummy calibration instruments
    if (rpa->protectionEnd() > today) {
        std::vector<Date> gridDates = RiskParticipationAgreementBaseEngine::buildDiscretisationGrid(
            today, rpa->protectionStart(), rpa->protectionEnd(), qlInstr->underlying(), maxGapDays,
            maxDiscretisationPoints);
        for (Size i = 0; i < gridDates.size() - 1; ++i) {
            Date mid = gridDates[i] + (gridDates[i + 1] - gridDates[i]) / 2;
            // mid might be = reference date degenerate cases where the first two discretisation points
            // are only one day apart from each other
            if (mid > today && ((calibrationMaturity - mid) >= 90 || expiries.empty()))
                expiries.push_back(mid);
        }

        std::vector<QuantLib::ext::shared_ptr<QuantLib::FixedRateCoupon>> fixedCpns;
        std::vector<QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon>> floatingCpns;
        for (auto const& l : qlInstr->underlying()) {
            for (auto const& c : l) {
                if (auto fixedCpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::FixedRateCoupon>(c))
                    fixedCpns.push_back(fixedCpn);
                if (auto floatingCpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::FloatingRateCoupon>(c)) {
                    floatingCpns.push_back(floatingCpn);
                    if (index == nullptr)
                        index = QuantLib::ext::dynamic_pointer_cast<IborIndex>(floatingCpn->index());
		}
            }
        }
        auto cpnLt = [](const QuantLib::ext::shared_ptr<Coupon>& x, const QuantLib::ext::shared_ptr<Coupon>& y) {
            return x->accrualStartDate() < y->accrualStartDate();
        };
        std::sort(fixedCpns.begin(), fixedCpns.end(), cpnLt);
        std::sort(floatingCpns.begin(), floatingCpns.end(), cpnLt);

        auto accLt = [](const QuantLib::ext::shared_ptr<Coupon>& x, const Date& e) { return x->accrualStartDate() < e; };
        for (auto const& expiry : expiries) {
            // look for the first fixed and float coupon with accrual start >= expiry
            auto firstFix = std::lower_bound(fixedCpns.begin(), fixedCpns.end(), expiry, accLt);
            auto firstFloat = std::lower_bound(floatingCpns.begin(), floatingCpns.end(), expiry, accLt);
            // if we find both coupons, we take the fixed rate minus the floating spread as the calibration strike
            // otherwise we set the strike to null meaning we request an ATM strike for the calibration
            if (firstFix != fixedCpns.end() && firstFloat != floatingCpns.end())
                strikes.push_back((*firstFix)->rate() - (*firstFloat)->spread());
            else
                strikes.push_back(Null<Real>());
        }
    }

    // build model + engine
    DLOG("Building LGM Grid RPA engine for trade " << id);
    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm =
        model(id, index == nullptr ? rpa->npvCurrency() : IndexNameTranslator::instance().oreName(index->name()),
              expiries, calibrationMaturity, strikes);
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    Handle<DefaultProbabilityTermStructure> creditCurve =
        market_->defaultCurve(rpa->creditCurveId(), configuration(MarketContext::pricing))->curve();
    Handle<Quote> recoveryRate = market_->recoveryRate(rpa->creditCurveId(), configuration(MarketContext::pricing));
    return QuantLib::ext::make_shared<NumericLgmRiskParticipationAgreementEngine>(
        rpa->npvCurrency(), getDiscountCurves(rpa), getFxSpots(rpa), lgm, sy, ny, sx, nx, creditCurve, recoveryRate,
        maxGapDays, maxDiscretisationPoints);
}

QuantLib::ext::shared_ptr<QuantLib::PricingEngine>
RiskParticipationAgreementTLockLGMGridEngineBuilder::engineImpl(const std::string& id,
                                                                RiskParticipationAgreement* rpa) {

    DLOG("Get engine data");

    Real sy = parseReal(engineParameter("sy"));
    Size ny = parseInteger(engineParameter("ny"));
    Real sx = parseReal(engineParameter("sx"));
    Size nx = parseInteger(engineParameter("nx"));

    Size timeStepsPerYear = parseInteger(engineParameter("TimeStepsPerYear"));
    Period spacing = parsePeriod(modelParameter("CalibrationInstrumentSpacing"));

    QL_REQUIRE(spacing != 0 * Days, "RiskParticipationAgreementTLockLGMGridEngineBuilder: CalibrationInstrumentSpacing "
                                    "is 0D, this is not allowed.");

    // determine expiries and strikes for calibration basket (coterminal ATM until termination date, spacing as
    // specified in config)

    auto qlInstr =
        QuantLib::ext::dynamic_pointer_cast<QuantExt::RiskParticipationAgreementTLock>(rpa->instrument()->qlInstrument());
    QL_REQUIRE(qlInstr != nullptr, "RiskParticipationAgreementTLockLGMGridEngineBuilder: internal error, could not "
                                   "cast to RiskParticipationAgreementTLock");

    QL_REQUIRE(qlInstr->bond() != nullptr,
               "RiskParticipationAgreementTLockLGMGridEngineBuilder: internal error, bond is null");

    std::vector<Date> expiries;
    std::vector<Real> strikes;

    Date today = Settings::instance().evaluationDate();
    Date calibrationMaturity = std::max(qlInstr->bond()->maturityDate(), today);

    // do not need calibration instruments, if the instrument price is not sensitive to the model

    if (rpa->protectionEnd() > today && qlInstr->terminationDate() > today) {
        Date calibrationDate = today + spacing;
        while (calibrationDate < qlInstr->terminationDate()) {
            if (expiries.empty() || (calibrationMaturity - calibrationDate) >= 90)
                expiries.push_back(calibrationDate);
            calibrationDate += spacing;
        }
        expiries.push_back(qlInstr->terminationDate());
        strikes.resize(expiries.size(), Null<Real>());
    }

    // build model + engine

    DLOG("Building LGM Grid RPA engine (tlock) for trade " << id);
    QuantLib::ext::shared_ptr<QuantExt::LGM> lgm = model(id, rpa->npvCurrency(), expiries, calibrationMaturity, strikes);
    DLOG("Build engine (configuration " << configuration(MarketContext::pricing) << ")");
    Handle<DefaultProbabilityTermStructure> creditCurve =
        market_->defaultCurve(rpa->creditCurveId(), configuration(MarketContext::pricing))->curve();
    Handle<Quote> recoveryRate = market_->recoveryRate(rpa->creditCurveId(), configuration(MarketContext::pricing));
    Handle<YieldTermStructure> treasuryCurve =
        market_->yieldCurve(rpa->tlockData().bondData().referenceCurveId(), configuration(MarketContext::pricing));
    try {
        // spread is optional, add it to the treasury curve here, if given
        auto spread =
            market_->securitySpread(rpa->tlockData().bondData().securityId(), configuration(MarketContext::pricing));
        treasuryCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(treasuryCurve, spread));
    } catch (...) {
    }

    return QuantLib::ext::make_shared<NumericLgmRiskParticipationAgreementEngineTLock>(
        rpa->npvCurrency(), getDiscountCurves(rpa), getFxSpots(rpa), lgm, sy, ny, sx, nx, treasuryCurve, creditCurve,
        recoveryRate, timeStepsPerYear);
}

} // namespace data
} // namespace ore
