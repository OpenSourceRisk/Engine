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

#include <qle/models/representativeswaption.hpp>

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/math/optimization/costfunction.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actualactual.hpp>

namespace QuantExt {

RepresentativeSwaptionMatcher::RepresentativeSwaptionMatcher(
    const std::vector<Leg>& underlying, const std::vector<bool>& isPayer,
    const QuantLib::ext::shared_ptr<SwapIndex>& swapIndexBase, const bool useUnderlyingIborIndex,
    const Handle<YieldTermStructure>& discountCurve, const Real reversion, const Real volatility, const Real flatRate)
    : underlying_(underlying), isPayer_(isPayer), swapIndexBase_(swapIndexBase),
      useUnderlyingIborIndex_(useUnderlyingIborIndex), discountCurve_(discountCurve), reversion_(reversion),
      volatility_(volatility), flatRate_(flatRate) {

    // set up flat curve, if we want that
    Handle<YieldTermStructure> flatCurve;
    if (flatRate != Null<Real>()) {
        flatCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), flatRate_, ActualActual(ActualActual::ISDA)));
    }

    // determine last cashflow date of underlying
    Date maturityDate = Date::minDate();
    for (auto const& l : underlying)
        for (auto const& c : l)
            maturityDate = std::max(c->date(), maturityDate);
    QL_REQUIRE(maturityDate > discountCurve->referenceDate(), "underlying maturity ("
                                                                  << maturityDate << ") must be gt reference date ("
                                                                  << discountCurve_->referenceDate() << ")");

    // set up model
    model_ = QuantLib::ext::make_shared<LGM>(QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        swapIndexBase_->currency(), flatCurve.empty() ? discountCurve_ : flatCurve, Array(), Array(1, volatility_),
        Array(), Array(1, reversion_)));

    // build underlying leg with its ibor / ois coupons linked to model forward curves
    QuantLib::ext::shared_ptr<IborIndex> modelIborIndexToUse, iborIndexToUse;
    for (Size l = 0; l < underlying_.size(); ++l) {
        for (auto const& c : underlying_[l]) {
            if (auto i = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(c)) {
                // standard ibor coupon
                QL_REQUIRE(!i->isInArrears(), "RepresentativeSwaptionMatcher: can not handle in arrears fixing");
                QuantLib::ext::shared_ptr<LgmImpliedYtsFwdFwdCorrected> y;
                auto fc = modelForwardCurves_.find(i->iborIndex()->name());
                if (fc != modelForwardCurves_.end())
                    y = fc->second;
                else {
                    y = QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(
                        model_, flatCurve.empty() ? i->iborIndex()->forwardingTermStructure() : flatCurve);
                    modelForwardCurves_[i->iborIndex()->name()] = y;
                }
                auto iborIndexLinkedToModelCurve = i->iborIndex()->clone(Handle<YieldTermStructure>(y));
                auto tmp = QuantLib::ext::make_shared<IborCoupon>(
                    i->date(), i->nominal(), i->accrualStartDate(), i->accrualEndDate(), i->fixingDays(),
                    iborIndexLinkedToModelCurve, i->gearing(), i->spread(), i->referencePeriodStart(),
                    i->referencePeriodEnd(), i->dayCounter(), false);
                tmp->setPricer(QuantLib::ext::make_shared<BlackIborCouponPricer>());
                modelLinkedUnderlying_.push_back(tmp);
                if (modelIborIndexToUse == nullptr) {
                    modelIborIndexToUse = iborIndexLinkedToModelCurve;
                    iborIndexToUse = i->iborIndex();
                }
            } else if (auto o = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(c)) {
                auto onIndex = o->overnightIndex();
                QL_REQUIRE(onIndex, "internal error: could not cast o->index() to overnightIndex");
                QuantLib::ext::shared_ptr<LgmImpliedYtsFwdFwdCorrected> y;
                auto fc = modelForwardCurves_.find(onIndex->name());
                if (fc != modelForwardCurves_.end())
                    y = fc->second;
                else {
                    y = QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(
                        model_, flatCurve.empty() ? onIndex->forwardingTermStructure() : flatCurve);
                    modelForwardCurves_[onIndex->name()] = y;
                }
                auto onIndexLinkedToModelCurve =
                    QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(onIndex->clone(Handle<YieldTermStructure>(y)));
                QL_REQUIRE(onIndexLinkedToModelCurve,
                           "internal error: could not cast onIndex->clone() to OvernightIndex");
                auto tmp = QuantLib::ext::make_shared<QuantExt::OvernightIndexedCoupon>(
                    o->date(), o->nominal(), o->accrualStartDate(), o->accrualEndDate(), onIndexLinkedToModelCurve,
                    o->gearing(), o->spread(), o->referencePeriodStart(), o->referencePeriodEnd(), o->dayCounter(),
                    false, o->includeSpread(), o->lookback(), o->rateCutoff(), o->fixingDays(),
                    o->rateComputationStartDate(), o->rateComputationEndDate());
                tmp->setPricer(QuantLib::ext::make_shared<OvernightIndexedCouponPricer>());
                modelLinkedUnderlying_.push_back(tmp);
                if (modelIborIndexToUse == nullptr) {
                    modelIborIndexToUse = onIndexLinkedToModelCurve;
                    iborIndexToUse = o->overnightIndex();
                }
            } else if (auto o = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(c)) {
                auto onIndex = o->overnightIndex();
                QL_REQUIRE(onIndex, "internal error: could not cast o->index() to overnightIndex");
                QuantLib::ext::shared_ptr<LgmImpliedYtsFwdFwdCorrected> y;
                auto fc = modelForwardCurves_.find(onIndex->name());
                if (fc != modelForwardCurves_.end())
                    y = fc->second;
                else {
                    y = QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(
                        model_, flatCurve.empty() ? onIndex->forwardingTermStructure() : flatCurve);
                    modelForwardCurves_[onIndex->name()] = y;
                }
                auto onIndexLinkedToModelCurve =
                    QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(onIndex->clone(Handle<YieldTermStructure>(y)));
                QL_REQUIRE(onIndexLinkedToModelCurve,
                           "internal error: could not cast onIndex->clone() to OvernightIndex");
                auto tmp = QuantLib::ext::make_shared<QuantExt::AverageONIndexedCoupon>(
                    o->date(), o->nominal(), o->accrualStartDate(), o->accrualEndDate(), onIndexLinkedToModelCurve,
                    o->gearing(), o->spread(), o->rateCutoff(), o->dayCounter(), o->lookback(), o->fixingDays(),
                    o->rateComputationStartDate(), o->rateComputationEndDate());
                tmp->setPricer(QuantLib::ext::make_shared<AverageONIndexedCouponPricer>());
                modelLinkedUnderlying_.push_back(tmp);
                if (modelIborIndexToUse == nullptr) {
                    modelIborIndexToUse = onIndexLinkedToModelCurve;
                    iborIndexToUse = o->overnightIndex();
                }
            } else if (QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c) ||
                       QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(c)) {
                // fixed coupon or simple cashflow
                modelLinkedUnderlying_.push_back(c);
            } else {
                QL_FAIL("RepresentativeSwaptionMatcher: unsupported coupon type");
            }
            modelLinkedUnderlyingIsPayer_.push_back(isPayer[l]);
        }
    }

    // build model linked discounting curve
    modelDiscountCurve_ =
        QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(model_, flatCurve.empty() ? discountCurve_ : flatCurve);

    // identify the ibor index to use for the matching
    if (modelIborIndexToUse == nullptr || !useUnderlyingIborIndex_) {
        auto fc = modelForwardCurves_.find(swapIndexBase_->iborIndex()->name());
        if (fc != modelForwardCurves_.end())
            modelIborIndexToUse = swapIndexBase_->iborIndex()->clone(Handle<YieldTermStructure>(fc->second));
        else
            modelIborIndexToUse = swapIndexBase_->iborIndex()->clone(
                Handle<YieldTermStructure>(QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(
                    model_, flatCurve.empty() ? swapIndexBase_->iborIndex()->forwardingTermStructure() : flatCurve)));
        iborIndexToUse = swapIndexBase_->iborIndex();
    }

    // build model linked swap index base
    modelSwapIndexForwardCurve_ =
        QuantLib::ext::dynamic_pointer_cast<LgmImpliedYtsFwdFwdCorrected>(*modelIborIndexToUse->forwardingTermStructure());
    QL_REQUIRE(modelSwapIndexForwardCurve_,
               "internal error: could not cast modelIborIndexToUse->forwardingTermStructure() to "
               "LgmImpliedYtsFwdFwdCorrected");
    modelSwapIndexDiscountCurve_ = QuantLib::ext::make_shared<LgmImpliedYtsFwdFwdCorrected>(
        model_, flatCurve.empty() ? (swapIndexBase_->discountingTermStructure().empty()
                                         ? modelIborIndexToUse->forwardingTermStructure()
                                         : swapIndexBase_->discountingTermStructure())
                                  : flatCurve);

    // create the final swap index base to use, i.e. the one with replaced ibor index, if desired
    swapIndexBaseFinal_ = QuantLib::ext::make_shared<SwapIndex>(
        swapIndexBase_->familyName(), swapIndexBase_->tenor(), swapIndexBase_->fixingDays(), swapIndexBase_->currency(),
        swapIndexBase_->fixingCalendar(), swapIndexBase_->fixedLegTenor(), swapIndexBase_->fixedLegConvention(),
        swapIndexBase_->dayCounter(), iborIndexToUse, Handle<YieldTermStructure>(modelSwapIndexDiscountCurve_));

    // clone the swap index base using the model fwd and dsc curves and replacing the ibor tenor, if that applies
    modelSwapIndexBase_ = QuantLib::ext::make_shared<SwapIndex>(
        swapIndexBase_->familyName(), swapIndexBase_->tenor(), swapIndexBase_->fixingDays(), swapIndexBase_->currency(),
        swapIndexBase_->fixingCalendar(), swapIndexBase_->fixedLegTenor(), swapIndexBase_->fixedLegConvention(),
        swapIndexBase_->dayCounter(), modelIborIndexToUse, Handle<YieldTermStructure>(modelSwapIndexDiscountCurve_));
}

namespace {
bool includeCashflow(const QuantLib::ext::shared_ptr<CashFlow>& f, const Date& exerciseDate,
                     const RepresentativeSwaptionMatcher::InclusionCriterion criterion) {
    switch (criterion) {
    case RepresentativeSwaptionMatcher::InclusionCriterion::AccrualStartGeqExercise:
        if (auto c = QuantLib::ext::dynamic_pointer_cast<Coupon>(f))
            return c->accrualStartDate() >= exerciseDate;
    case RepresentativeSwaptionMatcher::InclusionCriterion::PayDateGtExercise:
        return f->date() > exerciseDate;
    default:
        QL_FAIL("unknown inclusiong criterion");
    }
}
} // namespace

QuantLib::ext::shared_ptr<Swaption> RepresentativeSwaptionMatcher::representativeSwaption(Date exerciseDate,
                                                                                  const InclusionCriterion criterion) {

    QL_REQUIRE(exerciseDate > discountCurve_->referenceDate(),
               "exerciseDate (" << exerciseDate << ") must be greater than reference date ("
                                << discountCurve_->referenceDate() << ")");

    // shift size for derivative computation

    constexpr static Real h = 1.0E-4;

    // build leg containing all coupons with pay date > exerciseDate

    Date today = discountCurve_->referenceDate();
    Leg effectiveLeg;
    Real additionalDeterministicNpv = 0.0;
    std::vector<bool> effectiveIsPayer;
    for (Size c = 0; c < modelLinkedUnderlying_.size(); ++c) {
        QuantLib::ext::shared_ptr<CashFlow> cf = modelLinkedUnderlying_[c];
        if (!includeCashflow(cf, exerciseDate, criterion))
            continue;
        if (auto i = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(cf)) {
            if (today <= i->fixingDate() && i->fixingDate() < exerciseDate) {

                // an ibor coupon with today <= fixing date < exerciseDate is modified such that the fixing date is on
                // (or due to holiday adjustments shortly after) the exercise date; the nominal is adjusted such that
                // the effective accrual time remains the same
                // if on the other hand the fixing date is in the past, the historic fixing is required

                Calendar cal = i->iborIndex()->fixingCalendar();
                Date newAccrualStart = cal.advance(cal.adjust(exerciseDate), i->fixingDays() * Days);
                Date newAccrualEnd = std::max(i->accrualEndDate(), newAccrualStart + 1);
                Real newAccrualTime = i->dayCounter().yearFraction(newAccrualStart, newAccrualEnd);
                Real oldAccrualTime = i->dayCounter().yearFraction(i->accrualStartDate(), i->accrualEndDate());
                auto tmp = QuantLib::ext::make_shared<IborCoupon>(
                    i->date(), i->nominal() * oldAccrualTime / newAccrualTime, newAccrualStart, newAccrualEnd,
                    i->fixingDays(), i->iborIndex(), i->gearing(), i->spread(), i->referencePeriodStart(),
                    i->referencePeriodEnd(), i->dayCounter(), false);
                tmp->setPricer(QuantLib::ext::make_shared<BlackIborCouponPricer>());
                effectiveLeg.push_back(tmp);
                effectiveIsPayer.push_back(modelLinkedUnderlyingIsPayer_[c]);
            } else {
                // leave the ibor coupon as is
                effectiveLeg.push_back(cf);
                effectiveIsPayer.push_back(modelLinkedUnderlyingIsPayer_[c]);
            }
        } else if (auto o = QuantLib::ext::dynamic_pointer_cast<OvernightIndexedCoupon>(cf)) {

            if (o->fixingDates().empty())
                continue;

            // For an OIS coupon, we keep the original coupon, if the first fixing date >= exercise date

            if (o->fixingDates().front() >= exerciseDate) {
                o->setPricer(QuantLib::ext::make_shared<QuantExt::OvernightIndexedCouponPricer>());
                effectiveLeg.push_back(o);
                effectiveIsPayer.push_back(modelLinkedUnderlyingIsPayer_[c]);
            } else {

                // For an OIS coupon with first exercise date <= exercise date, represent
                // a) fixing dates < today via a fixed cashflow
                // b) fixing dates >= today via a float cashflow
                // For b) keep fixing dates >= exerciseDate only, but at least one fixing date and scale the result
                // to the full rate period associated to b). Furthermore, the accrual period will be the same as
                // the rate computation (value dates) period

                Real accrualToRatePeriodRatio = 1.0;
                if (o->rateComputationStartDate() != Null<Date>() && o->rateComputationEndDate() != Null<Date>()) {
                    accrualToRatePeriodRatio =
                        o->dayCounter().yearFraction(o->accrualStartDate(), o->accrualEndDate()) /
                        o->dayCounter().yearFraction(o->rateComputationStartDate(), o->rateComputationEndDate());
                }
                Date firstFixingDate = o->fixingDates().front();
                if (firstFixingDate < today) {
                    Date firstValueDate = valueDate(firstFixingDate, o);
                    Date lastFixingDateBeforeToday =
                        *std::next(std::lower_bound(o->fixingDates().begin(), o->fixingDates().end(), today), -1);
                    Date lastValueDateBeforeToday = valueDate(lastFixingDateBeforeToday, o);
                    if (lastValueDateBeforeToday > firstValueDate) {
                        auto tmp = QuantLib::ext::make_shared<OvernightIndexedCoupon>(
                            o->date(), o->nominal() * accrualToRatePeriodRatio, firstValueDate,
                            lastValueDateBeforeToday, o->overnightIndex(), o->gearing(), o->spread(),
                            o->referencePeriodStart(), o->referencePeriodEnd(), o->dayCounter(), false,
                            o->includeSpread(), 0 * Days, o->rateCutoff(), o->fixingDays());
                        tmp->setPricer(QuantLib::ext::make_shared<QuantExt::OvernightIndexedCouponPricer>());
                        additionalDeterministicNpv += discountCurve_->discount(tmp->date()) * tmp->amount();
                    }
                }
                if (o->fixingDates().back() >= today) {
                    Date firstFixingDateGeqToday =
                        *std::lower_bound(o->fixingDates().begin(), o->fixingDates().end(), today);
                    Date firstValueDateGeqToday = valueDate(firstFixingDateGeqToday, o);
                    Date lastValueDate =  valueDate(o->fixingDates().back(), o);
                    Date startDate = o->index()->fixingCalendar().adjust(exerciseDate);
                    Date endDate = std::max(lastValueDate, startDate + 1);
                    Real factor = o->dayCounter().yearFraction(firstValueDateGeqToday, lastValueDate) /
                                  o->dayCounter().yearFraction(startDate, endDate);
                    auto tmp = QuantLib::ext::make_shared<OvernightIndexedCoupon>(
                        o->date(), o->nominal() * accrualToRatePeriodRatio * factor, startDate, endDate,
                        o->overnightIndex(), o->gearing(), o->spread(), o->referencePeriodStart(),
                        o->referencePeriodEnd(), o->dayCounter(), false, o->includeSpread(), 0 * Days, o->rateCutoff(),
                        o->fixingDays());
                    tmp->setPricer(QuantLib::ext::make_shared<OvernightIndexedCouponPricer>());
                    effectiveLeg.push_back(tmp);
                    effectiveIsPayer.push_back(modelLinkedUnderlyingIsPayer_[c]);
                }
            }
        } else if (auto o = QuantLib::ext::dynamic_pointer_cast<AverageONIndexedCoupon>(cf)) {

            if (o->fixingDates().empty())
                continue;

            if (o->fixingDates().front() >= exerciseDate) {
                o->setPricer(QuantLib::ext::make_shared<QuantExt::AverageONIndexedCouponPricer>());
                effectiveLeg.push_back(o);
                effectiveIsPayer.push_back(modelLinkedUnderlyingIsPayer_[c]);
            } else {

                Real accrualToRatePeriodRatio = 1.0;
                if (o->rateComputationStartDate() != Null<Date>() && o->rateComputationEndDate() != Null<Date>()) {
                    accrualToRatePeriodRatio =
                        o->dayCounter().yearFraction(o->accrualStartDate(), o->accrualEndDate()) /
                        o->dayCounter().yearFraction(o->rateComputationStartDate(), o->rateComputationEndDate());
                }
                Date firstFixingDate = o->fixingDates().front();
                if (firstFixingDate < today) {
                    Date firstValueDate = valueDate(firstFixingDate, o);
                    Date lastFixingDateBeforeToday =
                        *std::next(std::lower_bound(o->fixingDates().begin(), o->fixingDates().end(), today), -1);
                    Date lastValueDateBeforeToday = valueDate(lastFixingDateBeforeToday, o);
                    if (lastValueDateBeforeToday > firstValueDate) {
                        auto tmp = QuantLib::ext::make_shared<AverageONIndexedCoupon>(
                            o->date(), o->nominal() * accrualToRatePeriodRatio, firstValueDate,
                            lastValueDateBeforeToday, o->overnightIndex(), o->gearing(), o->spread(), o->rateCutoff(),
                            o->dayCounter(), 0 * Days, o->fixingDays());
                        tmp->setPricer(QuantLib::ext::make_shared<QuantExt::AverageONIndexedCouponPricer>());
                        additionalDeterministicNpv += discountCurve_->discount(tmp->date()) * tmp->amount();
                    }
                }
                if (o->fixingDates().back() >= today) {
                    Date firstFixingDateGeqToday =
                        *std::lower_bound(o->fixingDates().begin(), o->fixingDates().end(), today);
                    Date firstValueDateGeqToday = valueDate(firstFixingDateGeqToday, o);
                    Date lastValueDate = valueDate(o->fixingDates().back(), o);
                    Date startDate = o->index()->fixingCalendar().adjust(exerciseDate);
                    Date endDate = std::max(lastValueDate, startDate + 1);
                    Real factor = o->dayCounter().yearFraction(firstValueDateGeqToday, lastValueDate) /
                                  o->dayCounter().yearFraction(startDate, endDate);
                    auto tmp = QuantLib::ext::make_shared<AverageONIndexedCoupon>(
                        o->date(), o->nominal() * accrualToRatePeriodRatio * factor, startDate, endDate,
                        o->overnightIndex(), o->gearing(), o->spread(), o->rateCutoff(), o->dayCounter(), 0 * Days,
                        o->fixingDays());
                    tmp->setPricer(QuantLib::ext::make_shared<AverageONIndexedCouponPricer>());
                    effectiveLeg.push_back(tmp);
                    effectiveIsPayer.push_back(modelLinkedUnderlyingIsPayer_[c]);
                }
            }
        } else if (QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(cf) != nullptr ||
                   QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(cf) != nullptr) {
            effectiveLeg.push_back(cf);
            effectiveIsPayer.push_back(modelLinkedUnderlyingIsPayer_[c]);
        } else {
            QL_FAIL("internal error: coupon type in modelLinkedUnderlying_ not supported in representativeSwaption()");
        }
    }

    if (effectiveLeg.empty())
        return QuantLib::ext::shared_ptr<Swaption>();

    // adjust exercise date to a valid fixing date, otherwise MakeVanillaSwap below may fail
    exerciseDate = swapIndexBase_->fixingCalendar().adjust(exerciseDate);

    // compute exercise time (the dc of the discount curve defines the date => time mapping by convention)
    Real t_ex = discountCurve_->timeFromReference(exerciseDate);

    // use T = t_ex - forward measure for all calculations instead of original LGM measure
    model_->parametrization()->shift() = -model_->parametrization()->H(t_ex);

    // initial guess for strike = nominal weighted fixed rate of coupons
    Real nominalSum = 0.0, nominalSumAbs = 0.0, strikeGuess = 0.0;
    Size nCpns = 0;
    for (Size c = 0; c < modelLinkedUnderlying_.size(); ++c) {
        if (auto f = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(modelLinkedUnderlying_[c])) {
            strikeGuess += f->rate() * std::abs(f->nominal());
            nominalSum += f->nominal() * (modelLinkedUnderlyingIsPayer_[c] ? -1.0 : 1.0);
            nominalSumAbs += std::abs(f->nominal());
            nCpns++;
        }
    }
    Real nominalGuess = nominalSum / static_cast<Real>(nCpns);
    if (close_enough(nominalSumAbs, 0.0))
        strikeGuess = 0.01; // default guess
    else
        strikeGuess /= nominalSumAbs;

    // initial guess for maturity = maturity of last cashflow
    Real maturityGuess = ActualActual(ActualActual::ISDA).yearFraction(exerciseDate, CashFlows::maturityDate(modelLinkedUnderlying_));

    // target function, input components are nominal, strike, maturity, output rel. error in npv, delta, gamma
    struct Matcher : public CostFunction {
        QuantLib::ext::shared_ptr<VanillaSwap> underlyingSwap(const QuantLib::ext::shared_ptr<SwapIndex> swapIndexBase,
                                                      const Period& maturity) const {
            // same as in SwapIndex::underlyingSwap() to make sure we are consistent
            return MakeVanillaSwap(maturity, swapIndexBase->iborIndex(), 0.0)
                .withEffectiveDate(swapIndexBase->valueDate(exerciseDate))
                .withFixedLegCalendar(swapIndexBase->fixingCalendar())
                .withFixedLegDayCount(swapIndexBase->dayCounter())
                .withFixedLegTenor(swapIndexBase->fixedLegTenor())
                .withFixedLegConvention(swapIndexBase->fixedLegConvention())
                .withFixedLegTerminationDateConvention(swapIndexBase->fixedLegConvention())
                .receiveFixed(true)
                .withNominal(1.0);
        }
        void setState(const Real state) const {
            for (auto const& c : modelCurves)
                c->state(state);
        }
        std::tuple<Size, Real> periodFromTime(Real t) const {
            t *= 12.0;
            Size months = static_cast<Size>(std::floor(t));
            Real alpha = t - static_cast<Real>(months);
            return std::make_tuple(months, alpha);
        }
        Array values(const Array& x) const override {
            Real maturityTime = std::min(x[2] * x[2], maxMaturityTime);
            // bracket continuous maturity between two discrete tenors rounded to whole months
            // this is essential to provide a smooth target function
            Size months;
            Real alpha;
            std::tie(months, alpha) = periodFromTime(maturityTime);
            RawResult rawResult;
            // do we have a cached result?
            auto res = cachedRawResults.find(months);
            if (res != cachedRawResults.end()) {
                rawResult = res->second;
            } else {
                Period lowerMaturity = months * Months;
                Period upperMaturity = lowerMaturity + 1 * Months;
                // generate candidate underlying and compute npv for states 0,+h,-h with chosen stepsize
                QuantLib::ext::shared_ptr<VanillaSwap> underlyingLower, underlyingUpper;
                if (lowerMaturity > 0 * Months)
                    underlyingLower = underlyingSwap(modelSwapIndexBase, lowerMaturity);
                underlyingUpper = underlyingSwap(modelSwapIndexBase, upperMaturity);
                if (underlyingLower)
                    underlyingLower->setPricingEngine(engine);
                underlyingUpper->setPricingEngine(engine);
                setState(0.0);
                rawResult.npv0_l = underlyingLower ? underlyingLower->NPV() : 0.0;
                rawResult.bps0_l = underlyingLower ? underlyingLower->fixedLegBPS() * 1.0E4 : 0.0;
                rawResult.npv0_u = underlyingUpper->NPV();
                rawResult.bps0_u = underlyingUpper->fixedLegBPS() * 1.0E4;
                setState(h);
                rawResult.npvu_l = underlyingLower ? underlyingLower->NPV() : 0.0;
                rawResult.bpsu_l = underlyingLower ? underlyingLower->fixedLegBPS() * 1.0E4 : 0.0;
                rawResult.npvu_u = underlyingUpper->NPV();
                rawResult.bpsu_u = underlyingUpper->fixedLegBPS() * 1.0E4;
                setState(-h);
                rawResult.npvd_l = underlyingLower ? underlyingLower->NPV() : 0.0;
                rawResult.bpsd_l = underlyingLower ? underlyingLower->fixedLegBPS() * 1.0E4 : 0.0;
                rawResult.npvd_u = underlyingUpper->NPV();
                rawResult.bpsd_u = underlyingUpper->fixedLegBPS() * 1.0E4;
                cachedRawResults[months] = rawResult;
            }
            // compute npv of lower and upper underlying
            Real v0_l = x[0] * (rawResult.npv0_l + rawResult.bps0_l * x[1]);
            Real v0_u = x[0] * (rawResult.npv0_u + rawResult.bps0_u * x[1]);
            Real vu_l = x[0] * (rawResult.npvu_l + rawResult.bpsu_l * x[1]);
            Real vu_u = x[0] * (rawResult.npvu_u + rawResult.bpsu_u * x[1]);
            Real vd_l = x[0] * (rawResult.npvd_l + rawResult.bpsd_l * x[1]);
            Real vd_u = x[0] * (rawResult.npvd_u + rawResult.bpsd_u * x[1]);
            // compute interpolated npv
            Real v0 = v0_l * (1.0 - alpha) + v0_u * alpha;
            Real vu = vu_l * (1.0 - alpha) + vu_u * alpha;
            Real vd = vd_l * (1.0 - alpha) + vd_u * alpha;
            // compute delta and gamma w.r.t. model state
            Real delta = (vu - vd) / (2.0 * h);
            Real gamma = (vu + vd) / (h * h);
            // return target function
            Array target{(v0 - npv_target) / delta_target, (delta - delta_target) / delta_target,
                         (gamma - gamma_target) / gamma_target};
            return target;
        }
        // to be provided
        Real h;
        Date exerciseDate;
        Real maxMaturityTime;
        QuantLib::ext::shared_ptr<SwapIndex> modelSwapIndexBase;
        QuantLib::ext::shared_ptr<PricingEngine> engine;
        std::set<QuantLib::ext::shared_ptr<LgmImpliedYtsFwdFwdCorrected>> modelCurves;
        Real npv_target, delta_target, gamma_target;
        // internal
        struct RawResult {
            double npv0_l, npvu_l, npvd_l, bps0_l, bpsu_l, bpsd_l;
            double npv0_u, npvu_u, npvd_u, bps0_u, bpsu_u, bpsd_u;
        };
        mutable std::map<Size, RawResult> cachedRawResults;
    };

    // init target function
    Matcher matcher;
    matcher.h = h;
    // limit max maturity time such that we are safe anyway
    matcher.maxMaturityTime = discountCurve_->dayCounter().yearFraction(exerciseDate, Date::maxDate() - 365);
    matcher.exerciseDate = exerciseDate;
    matcher.modelSwapIndexBase = modelSwapIndexBase_;
    matcher.engine = QuantLib::ext::make_shared<DiscountingSwapEngine>(Handle<YieldTermStructure>(modelDiscountCurve_), false,
                                                               exerciseDate, exerciseDate);
    for (auto const& c : modelForwardCurves_)
        matcher.modelCurves.insert(c.second);
    matcher.modelCurves.insert(modelDiscountCurve_);
    matcher.modelCurves.insert(modelSwapIndexForwardCurve_);
    matcher.modelCurves.insert(modelSwapIndexDiscountCurve_);

    // set reference date in model curves
    for (auto const& c : matcher.modelCurves)
        c->referenceDate(exerciseDate);

    // compute exotic underlying npv, delta, gamma and set as target
    Leg rec, pay;
    for (Size c = 0; c < effectiveLeg.size(); ++c) {
        if (effectiveIsPayer[c])
            pay.push_back(effectiveLeg[c]);
        else
            rec.push_back(effectiveLeg[c]);
    }
    Swap exotic(pay, rec);
    exotic.setPricingEngine(matcher.engine);
    Real v0 = exotic.NPV();
    matcher.setState(h);
    Real vu = exotic.NPV();
    matcher.setState(-h);
    Real vd = exotic.NPV();
    matcher.npv_target = v0 + additionalDeterministicNpv;
    matcher.delta_target = (vu - vd) / (2.0 * h);
    matcher.gamma_target = (vu + vd) / (h * h);

    // set up optimizer and run it
    NoConstraint constraint;
    Array guess{nominalGuess, strikeGuess, std::sqrt(maturityGuess)};
    Problem problem(matcher, constraint, guess);
    LevenbergMarquardt opt;
    EndCriteria ec(1000, 20, 1E-8, 1E-8, 1E-8);
    opt.minimize(problem, ec);

    // extract result and return it
    Array x = problem.currentValue();
    Real nominal = x[0];
    Real strike = x[1];
    Size months;
    Real alpha;
    std::tie(months, alpha) = matcher.periodFromTime(std::min(x[2] * x[2], matcher.maxMaturityTime));
    Size nMonths = std::max<Size>(1, (months + static_cast<Size>(std::round(alpha))));
    Period maturity = nMonths * Months;
    // rescale notional to adjust for the difference between the calibrated maturity and the actual maturity we set
    nominal *= x[2] * x[2] * 12.0 / static_cast<Real>(nMonths);
    QuantLib::ext::shared_ptr<VanillaSwap> underlying =
        MakeVanillaSwap(maturity, swapIndexBaseFinal_->iborIndex(), strike)
            .withEffectiveDate(swapIndexBaseFinal_->valueDate(exerciseDate))
            .withFixedLegCalendar(swapIndexBaseFinal_->fixingCalendar())
            .withFixedLegDayCount(swapIndexBaseFinal_->dayCounter())
            .withFixedLegTenor(swapIndexBaseFinal_->fixedLegTenor())
            .withFixedLegConvention(swapIndexBaseFinal_->fixedLegConvention())
            .withFixedLegTerminationDateConvention(swapIndexBaseFinal_->fixedLegConvention())
            .receiveFixed(nominal > 0.0)
            .withNominal(std::abs(nominal));
    underlying->setPricingEngine(QuantLib::ext::make_shared<DiscountingSwapEngine>(discountCurve_));
    return QuantLib::ext::make_shared<Swaption>(underlying, QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate));
}

QuantLib::Date
RepresentativeSwaptionMatcher::valueDate(const QuantLib::Date& fixingDate,
                                         const QuantLib::ext::shared_ptr<QuantLib::FloatingRateCoupon>& cpn) const {
    return cpn->index()->fixingCalendar().advance(fixingDate, cpn->fixingDays(), Days, Following);
}
} // namespace QuantExt
