/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/blackmultilegoptionengine.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/instruments/rebatedexercise.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <boost/algorithm/string/join.hpp>

namespace QuantExt {

BlackMultiLegOptionEngineBase::BlackMultiLegOptionEngineBase(const Handle<YieldTermStructure>& discountCurve,
                                                             const Handle<SwaptionVolatilityStructure>& volatility)
    : discountCurve_(discountCurve), volatility_(volatility) {}

bool BlackMultiLegOptionEngineBase::instrumentIsHandled(const MultiLegOption& m, std::vector<std::string>& messages) {
    return instrumentIsHandled(m.legs(), m.payer(), m.currency(), m.exercise(), m.settlementType(),
                               m.settlementMethod(), messages);
}

bool BlackMultiLegOptionEngineBase::instrumentIsHandled(const std::vector<Leg>& legs, const std::vector<bool>& payer,
                                                        const std::vector<Currency>& currency,
                                                        const QuantLib::ext::shared_ptr<Exercise>& exercise,
                                                        const Settlement::Type& settlementType,
                                                        const Settlement::Method& settlementMethod,
                                                        std::vector<std::string>& messages) {

    bool isHandled = true;

    // is there a unique pay currency and all interest rate indices are in this same currency?

    for (Size i = 1; i < currency.size(); ++i) {
        if (currency[0] != currency[i]) {
            messages.push_back("NumericLgmMultilegOptionEngine: can only handle single currency underlyings, got " +
                               currency[0].code() + " on leg #1 and " + currency[i].code() + " on leg #" +
                               std::to_string(i + 1));
            isHandled = false;
        }
    }

    for (Size i = 0; i < legs.size(); ++i) {
        for (Size j = 0; j < legs[i].size(); ++j) {
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(legs[i][j])) {
                if (cpn->index()->currency() != currency[0]) {
                    messages.push_back("NumericLgmMultilegOptionEngine: can only handle indices (" +
                                       cpn->index()->name() + ") with same currency as unqiue pay currency (" +
                                       currency[0].code());
                }
            }
        }
    }

    // check coupon types

    for (Size i = 0; i < legs.size(); ++i) {
        for (Size j = 0; j < legs[i].size(); ++j) {
            if (auto c = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs[i][j])) {
                if (!(QuantLib::ext::dynamic_pointer_cast<IborCoupon>(c) || QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantLib::AverageBMACoupon>(c) ||
                      QuantLib::ext::dynamic_pointer_cast<QuantExt::SubPeriodsCoupon1>(c))) {
                    messages.push_back(
                        "BlackMultilegOptionEngine: coupon type not handled, supported coupon types: Fix, "
                        "Ibor, ON comp, ON avg, BMA/SIFMA, subperiod. leg = " +
                        std::to_string(i) + " cf = " + std::to_string(j));
                    isHandled = false;
                }
            }
        }
    }

    // check exercise type

    isHandled = isHandled && (exercise == nullptr || exercise->type() == Exercise::European);

    return isHandled;
}

void BlackMultiLegOptionEngineBase::calculate() const {

    std::vector<std::string> messages;
    QL_REQUIRE(instrumentIsHandled(legs_, payer_, currency_, exercise_, settlementType_, settlementMethod_, messages),
               "BlackMultiLegOptionEngineBase::calculate(): instrument is not handled: "
                   << boost::algorithm::join(messages, ", "));

    // handle empty exercise

    if (exercise_ == nullptr) {
        npv_ = 0.0;
        for (Size i = 0; i < legs_.size(); ++i) {
            for (Size j = 0; j < legs_[i].size(); ++j) {
                npv_ += legs_[i][j]->amount() * discountCurve_->discount(legs_[i][j]->date());
            }
        }
        underlyingNpv_ = npv_;
        return;
    }

    // set exercise date and calculate exercise time on vol day counter

    QL_REQUIRE(exercise_->dates().size() == 1,
               "BlackMultiLegOptionEngineBase: expected 1 exercise dates, got " << exercise_->dates().size());
    Date exerciseDate = exercise_->date(0);
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);

    // filter on future coupons and store the necessary data,
    // also determine fixed day count (one of the fixed cpn), earliest and latest accrual dates

    struct CfInfo {
        Real fixedRate = Null<Real>();       // for fixed cpn
        Real floatingSpread = Null<Real>();  // for float cpn
        Real floatingGearing = Null<Real>(); // for float cpn
        Real nominal = Null<Real>();         // for all cpn
        Real accrual = Null<Real>();         // for all cpn
        Real amount = Null<Real>();          // for all cf
        Real discountFactor = Null<Real>();  // for all cf
        Date payDate = Null<Date>();         // for all cf
    };

    std::vector<CfInfo> cfInfo;

    DayCounter fixedDayCount;
    Date earliestAccrualStartDate = Date::maxDate();
    Date latestAccrualEndDate = Date::minDate();

    Size numFixed = 0, numFloat = 0;

    for (Size i = 0; i < legs_.size(); ++i) {
        for (Size j = 0; j < legs_[i].size(); ++j) {
            Real payer = payer_[i] ? -1.0 : 1.0;
            if (auto c = QuantLib::ext::dynamic_pointer_cast<Coupon>(legs_[i][j])) {
                auto fixedCoupon = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c);
                if (fixedCoupon)
                    fixedDayCount = c->dayCounter();
                if (c->accrualStartDate() >= exerciseDate) {
                    cfInfo.push_back(CfInfo());
                    earliestAccrualStartDate = std::min(earliestAccrualStartDate, c->accrualStartDate());
                    latestAccrualEndDate = std::max(latestAccrualEndDate, c->accrualEndDate());
                    if (fixedCoupon) {
                        cfInfo.back().fixedRate = fixedCoupon->rate();
                        cfInfo.back().nominal = c->nominal() * payer;
                        ++numFixed;
                    } else if (auto f = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(c)) {
                        cfInfo.back().floatingSpread = f->spread();
                        cfInfo.back().floatingGearing = f->gearing();
                        cfInfo.back().nominal = f->nominal() * payer;
                        ++numFloat;
                    }
                    cfInfo.back().accrual = c->accrualPeriod();
                    cfInfo.back().discountFactor = discountCurve_->discount(c->date());
                    cfInfo.back().amount = c->amount() * payer;
                    cfInfo.back().payDate = c->date();
                }
            } else if (legs_[i][j]->date() > exerciseDate) {
                cfInfo.push_back(CfInfo());
                cfInfo.back().discountFactor = discountCurve_->discount(legs_[i][j]->date());
                cfInfo.back().amount = legs_[i][j]->amount() * payer;
                cfInfo.back().payDate = legs_[i][j]->date();
            }
        }
    }

    // if there are future floating coupons, but no fixed, we add a dummy fixed with 0 fixed rate to 
    // ensure we can calculate the ATM rate
    if (numFloat > 0 && numFixed == 0) {
        for (auto const& c : cfInfo) {
            if (c.floatingSpread != Null<Real>()) {
                cfInfo.push_back(CfInfo());
                cfInfo.back().fixedRate = 0.0;
                cfInfo.back().nominal = c.nominal * -1; // assume opposite direction
                cfInfo.back().accrual = c.accrual;
                cfInfo.back().discountFactor = c.discountFactor;
                cfInfo.back().amount = 0.0;
                cfInfo.back().payDate = c.payDate;
            }
        }
    }

    QL_REQUIRE(!fixedDayCount.empty(), "BlackMultiLegOptionEngineBase::calculate(): could not determine fixed day "
                                       "counter - it appears no fixed coupons are present in the underlying.");
    QL_REQUIRE(earliestAccrualStartDate != Date::maxDate() && latestAccrualEndDate != Date::minDate(),
               "BlackMultiLegOptionEngineBase::calculate(): could not determine earliest / latest accrual start / end "
               "dates. No coupons seems to be present in the underlying.");

    // add the rebate (if any) as a fixed cashflow to the exercise-into underlying

    if (auto rebatedExercise = QuantLib::ext::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_)) {
        cfInfo.push_back(CfInfo());
        cfInfo.back().amount = rebatedExercise->rebate(0);
        cfInfo.back().discountFactor = discountCurve_->discount(rebatedExercise->rebatePaymentDate(0));
    }

    // determine the pv-weighted fixed rate, the fixed bps, the fixed npv

    Real weightedFixedRateNom = 0.0, weightedFixedRateDenom = 0.0;
    Real fixedBps = 0.0, fixedNpv = 0.0;
    for (auto const& c : cfInfo) {
        if (c.fixedRate != Null<Real>()) {
            Real w = c.nominal * c.accrual * c.discountFactor;
            weightedFixedRateNom += c.fixedRate * w;
            weightedFixedRateDenom += w;
            fixedBps += w;
            fixedNpv += c.amount * c.discountFactor;
        }
    }
    Real weightedFixedRate = weightedFixedRateNom / weightedFixedRateDenom;

    // determine the pv-weighted floating margin, the floating bps, the floating npv

    Real weightedFloatingSpreadNom = 0.0, weightedFloatingSpreadDenom = 0.0;
    Real floatingBps = 0.0, floatingNpv = 0.0;
    for (auto const& c : cfInfo) {
        if (c.floatingSpread != Null<Real>()) {
            Real w = c.nominal * c.accrual * c.discountFactor;
            weightedFloatingSpreadNom += c.floatingSpread * w;
            weightedFloatingSpreadDenom += w;
            floatingBps += w;
            floatingNpv += c.amount * c.discountFactor;
        }
    }
    Real weightedFloatingSpread = weightedFloatingSpreadNom / weightedFloatingSpreadDenom;

    // determine the simple cf npv

    Real simpleCfNpv = 0.0;
    for (auto const& c : cfInfo) {
        if (c.fixedRate == Null<Real>() && c.floatingSpread == Null<Real>()) {
            simpleCfNpv += c.amount * c.discountFactor;
        }
    }

    // determine the fair swap rate

    Real atmForward = -floatingNpv / fixedBps;

    // determine the spread correction

    Real spreadCorrection = weightedFloatingSpread * std::abs(floatingBps / fixedBps);

    Real effectiveAtmForward = atmForward - spreadCorrection;
    Real effectiveFixedRate = weightedFixedRate - spreadCorrection;

    // incorporate the simple cashflows (including the rebate)

    Real simpleCfCorrection = 0.0;
    for (auto const& c : cfInfo) {
        if (c.fixedRate == Null<Real>() && c.floatingSpread == Null<Real>()) {
            simpleCfCorrection += c.amount * c.discountFactor / fixedBps;
        }
    }

    effectiveFixedRate += simpleCfCorrection;

    // determine the annuity

    Real annuity = 0.0;
    if (settlementType_ == Settlement::Physical ||
        (settlementType_ == Settlement::Cash && settlementMethod_ == Settlement::CollateralizedCashPrice)) {
        annuity = std::abs(fixedBps);
    } else if (settlementType_ == Settlement::Cash && settlementMethod_ == Settlement::ParYieldCurve) {
        // we assume that the cash settlement date is equal to the swap start date
        InterestRate sr(effectiveAtmForward, fixedDayCount, Compounded, Annual);
        for (auto const& c : cfInfo) {
            if (c.fixedRate != Null<Real>()) {
                annuity += std::abs(c.nominal) * c.accrual *
                           sr.discountFactor(std::min(earliestAccrualStartDate, c.payDate), c.payDate);
            }
        }
        annuity *= discountCurve_->discount(earliestAccrualStartDate);
    } else {
        QL_FAIL("BlackMultiLegOptionEngine: invalid (settlementType, settlementMethod) pair");
    }

    // determine the swap length

    Real swapLength = volatility_->swapLength(earliestAccrualStartDate, latestAccrualEndDate);

    // swapLength is rounded to whole months. To ensure we can read a variance
    // and a shift from volatility_ we floor swapLength at 1/12 here therefore.
    swapLength = std::max(swapLength, 1.0 / 12.0);

    Real variance = volatility_->blackVariance(exerciseDate, swapLength, effectiveFixedRate);
    Real displacement =
        volatility_->volatilityType() == ShiftedLognormal ? volatility_->shift(exerciseDate, swapLength) : 0.0;

    Real stdDev = std::sqrt(variance);
    Real delta = 0.0, vega = 0.0;
    Option::Type optionType = fixedBps > 0 ? Option::Put : Option::Call;

    if (close_enough(annuity, 0.0)) {
        npv_ = 0.0;
    } else if (volatility_->volatilityType() == ShiftedLognormal) {
        npv_ = blackFormula(optionType, effectiveFixedRate, effectiveAtmForward, stdDev, annuity, displacement);
        vega = std::sqrt(exerciseTime) *
               blackFormulaStdDevDerivative(effectiveFixedRate, effectiveAtmForward, stdDev, annuity, displacement);
        delta = blackFormulaForwardDerivative(optionType, effectiveFixedRate, effectiveAtmForward, stdDev, annuity,
                                              displacement);
    } else {
        npv_ = bachelierBlackFormula(optionType, effectiveFixedRate, effectiveAtmForward, stdDev, annuity);
        vega = std::sqrt(exerciseTime) *
               bachelierBlackFormulaStdDevDerivative(effectiveFixedRate, effectiveAtmForward, stdDev, annuity);
        delta = bachelierBlackFormulaForwardDerivative(optionType, effectiveFixedRate, effectiveAtmForward, stdDev,
                                                       annuity);
    }

    underlyingNpv_ = fixedNpv + floatingNpv + simpleCfNpv;

    additionalResults_["spreadCorrection"] = spreadCorrection;
    additionalResults_["simpleCashflowCorrection"] = simpleCfCorrection;
    additionalResults_["strike"] = effectiveFixedRate;
    additionalResults_["atmForward"] = effectiveAtmForward;
    additionalResults_["underlyingNPV"] = underlyingNpv_;
    additionalResults_["annuity"] = annuity;
    additionalResults_["timeToExpiry"] = exerciseTime;
    additionalResults_["impliedVolatility"] = Real(stdDev / std::sqrt(exerciseTime));
    additionalResults_["swapLength"] = swapLength;
    additionalResults_["stdDev"] = stdDev;
    additionalResults_["delta"] = delta;
    additionalResults_["vega"] = vega;
    additionalResults_["fixedNpv"] = fixedNpv;
    additionalResults_["fixedBps"] = fixedBps;
    additionalResults_["weightedFixedRate"] = weightedFixedRate;
    additionalResults_["floatingNpv"] = floatingNpv;
    additionalResults_["floatingBps"] = floatingBps;
    additionalResults_["weightedFloatingSpread"] = weightedFloatingSpread;
    additionalResults_["simpleCfNpv"] = simpleCfNpv;
}

BlackMultiLegOptionEngine::BlackMultiLegOptionEngine(const Handle<YieldTermStructure>& discountCurve,
                                                     const Handle<SwaptionVolatilityStructure>& volatility)
    : BlackMultiLegOptionEngineBase(discountCurve, volatility) {
    registerWith(discountCurve_);
    registerWith(volatility_);
}

void BlackMultiLegOptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_ = arguments_.payer;
    currency_ = arguments_.currency;
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    BlackMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.underlyingNpv = underlyingNpv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
}

BlackSwaptionFromMultilegOptionEngine::BlackSwaptionFromMultilegOptionEngine(
    const Handle<YieldTermStructure>& discountCurve, const Handle<SwaptionVolatilityStructure>& volatility)
    : BlackMultiLegOptionEngineBase(discountCurve, volatility) {
    registerWith(discountCurve_);
    registerWith(volatility_);
}

void BlackSwaptionFromMultilegOptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_.resize(arguments_.payer.size());
    for (Size i = 0; i < arguments_.payer.size(); ++i) {
        payer_[i] = QuantLib::close_enough(arguments_.payer[i], -1.0);
    }
    currency_ = std::vector<Currency>(legs_.size(), arguments_.swap->iborIndex()->currency());
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    BlackMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
}

BlackNonstandardSwaptionFromMultilegOptionEngine::BlackNonstandardSwaptionFromMultilegOptionEngine(
    const Handle<YieldTermStructure>& discountCurve, const Handle<SwaptionVolatilityStructure>& volatility)
    : BlackMultiLegOptionEngineBase(discountCurve, volatility) {
    registerWith(discountCurve_);
    registerWith(volatility_);
}

void BlackNonstandardSwaptionFromMultilegOptionEngine::calculate() const {
    legs_ = arguments_.legs;
    payer_.resize(arguments_.payer.size());
    for (Size i = 0; i < arguments_.payer.size(); ++i) {
        payer_[i] = QuantLib::close_enough(arguments_.payer[i], -1.0);
    }
    currency_ = std::vector<Currency>(legs_.size(), arguments_.swap->iborIndex()->currency());
    exercise_ = arguments_.exercise;
    settlementType_ = arguments_.settlementType;
    settlementMethod_ = arguments_.settlementMethod;

    BlackMultiLegOptionEngineBase::calculate();

    results_.value = npv_;
    results_.additionalResults = additionalResults_;
    results_.additionalResults["underlyingNpv"] = underlyingNpv_;
}

} // namespace QuantExt
