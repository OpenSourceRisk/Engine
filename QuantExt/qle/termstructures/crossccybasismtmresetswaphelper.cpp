/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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
#include <qle/pricingengines/crossccyswapengine.hpp>
#ifdef QL_USE_INDEXED_COUPON
#include <ql/cashflows/floatingratecoupon.hpp>
#endif

#include <qle/termstructures/crossccybasismtmresetswaphelper.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

namespace {
void no_deletion(YieldTermStructure*) {}
} // namespace

CrossCcyBasisMtMResetSwapHelper::CrossCcyBasisMtMResetSwapHelper(
    const Handle<Quote>& spreadQuote, const Handle<Quote>& spotFX, Natural settlementDays,
    const Calendar& settlementCalendar, const Period& swapTenor, BusinessDayConvention rollConvention,
    const boost::shared_ptr<QuantLib::IborIndex>& foreignCcyIndex,
    const boost::shared_ptr<QuantLib::IborIndex>& domesticCcyIndex,
    const Handle<YieldTermStructure>& foreignCcyDiscountCurve,
    const Handle<YieldTermStructure>& domesticCcyDiscountCurve,
    const Handle<YieldTermStructure>& foreignCcyFxFwdRateCurve,
    const Handle<YieldTermStructure>& domesticCcyFxFwdRateCurve, bool eom, bool spreadOnForeignCcy, bool invertFxIndex,
    boost::optional<Period> foreignTenor, boost::optional<Period> domesticTenor)
    : RelativeDateRateHelper(spreadQuote), spotFX_(spotFX), settlementDays_(settlementDays),
      settlementCalendar_(settlementCalendar), swapTenor_(swapTenor), rollConvention_(rollConvention),
      foreignCcyIndex_(foreignCcyIndex), domesticCcyIndex_(domesticCcyIndex),
      foreignCcyDiscountCurve_(foreignCcyDiscountCurve), domesticCcyDiscountCurve_(domesticCcyDiscountCurve),
      foreignCcyFxFwdRateCurve_(foreignCcyFxFwdRateCurve), domesticCcyFxFwdRateCurve_(domesticCcyFxFwdRateCurve),
      eom_(eom), spreadOnForeignCcy_(spreadOnForeignCcy), invertFxIndex_(invertFxIndex),
      foreignTenor_(foreignTenor ? *foreignTenor : foreignCcyIndex_->tenor()),
      domesticTenor_(domesticTenor ? *domesticTenor : domesticCcyIndex_->tenor()) {

    foreignCurrency_ = foreignCcyIndex_->currency();
    domesticCurrency_ = domesticCcyIndex_->currency();
    QL_REQUIRE(foreignCurrency_ != domesticCurrency_,
               "matching currencies not allowed on CrossCcyBasisMtMResetSwapHelper");

    bool foreignIndexHasCurve = !foreignCcyIndex_->forwardingTermStructure().empty();
    bool domesticIndexHasCurve = !domesticCcyIndex_->forwardingTermStructure().empty();
    bool haveForeignDiscountCurve = !foreignCcyDiscountCurve_.empty();
    bool haveDomesticDiscountCurve = !domesticCcyDiscountCurve_.empty();

    QL_REQUIRE(
        !(foreignIndexHasCurve && domesticIndexHasCurve && haveForeignDiscountCurve && haveDomesticDiscountCurve),
        "CrossCcyBasisMtMResetSwapHelper - Have all curves, nothing to solve for.");

    /* Link the curve being bootstrapped to the index if the index has
    no projection curve */
    if (foreignIndexHasCurve && haveForeignDiscountCurve) {
        if (!domesticIndexHasCurve) {
            domesticCcyIndex_ = domesticCcyIndex_->clone(termStructureHandle_);
            domesticCcyIndex_->unregisterWith(termStructureHandle_);
        }
        // if we have both index and discounting curve on foreign leg,
        // check foreignCcyFxFwdRateCurve and link it to foreign discount curve if empty
        // (we are bootstrapping on domestic leg in this instance, so foreign leg needs to be fully determined
        if (foreignCcyFxFwdRateCurve_.empty())
            foreignCcyFxFwdRateCurve_ = foreignCcyDiscountCurve_;
    } else if (domesticIndexHasCurve && haveDomesticDiscountCurve) {
        if (!foreignIndexHasCurve) {
            foreignCcyIndex_ = foreignCcyIndex_->clone(termStructureHandle_);
            foreignCcyIndex_->unregisterWith(termStructureHandle_);
        }
        // if we have both index and discounting curve on domestic leg,
        // check domesticCcyFxFwdRateCurve and link it to domestic discount curve if empty
        // (we are bootstrapping on foreign leg in this instance, so domestic leg needs to be fully determined
        if (domesticCcyFxFwdRateCurve_.empty())
            domesticCcyFxFwdRateCurve_ = domesticCcyDiscountCurve_;
    } else {
        QL_FAIL("Need one leg of the cross currency basis swap to "
                "have all of its curves.");
    }

    registerWith(spotFX_);
    registerWith(domesticCcyIndex_);
    registerWith(foreignCcyIndex_);
    registerWith(foreignCcyDiscountCurve_);
    registerWith(domesticCcyDiscountCurve_);
    registerWith(foreignCcyFxFwdRateCurve_);
    registerWith(domesticCcyFxFwdRateCurve_);

    initializeDates();
}

void CrossCcyBasisMtMResetSwapHelper::initializeDates() {

    Date refDate = evaluationDate_;
    // if the evaluation date is not a business day
    // then move to the next business day
    refDate = settlementCalendar_.adjust(refDate);

    Date settlementDate = settlementCalendar_.advance(refDate, settlementDays_, Days);
    Date maturityDate = settlementDate + swapTenor_;

    Schedule foreignLegSchedule = MakeSchedule()
                                      .from(settlementDate)
                                      .to(maturityDate)
                                      .withTenor(foreignTenor_)
                                      .withCalendar(settlementCalendar_)
                                      .withConvention(rollConvention_)
                                      .endOfMonth(eom_);

    Schedule domesticLegSchedule = MakeSchedule()
                                       .from(settlementDate)
                                       .to(maturityDate)
                                       .withTenor(domesticTenor_)
                                       .withCalendar(settlementCalendar_)
                                       .withConvention(rollConvention_)
                                       .endOfMonth(eom_);

    Real foreignNominal = 1.0;
    // build an FX index for forward rate projection (TODO - review settlement and calendar)
    boost::shared_ptr<FxIndex> fxIdx = boost::make_shared<FxIndex>(
        "dummy", settlementDays_, foreignCurrency_, domesticCurrency_, settlementCalendar_, spotFX_,
        foreignCcyFxFwdRateCurveRLH_, domesticCcyFxFwdRateCurveRLH_, invertFxIndex_);

    swap_ = boost::shared_ptr<CrossCcyBasisMtMResetSwap>(
        new CrossCcyBasisMtMResetSwap(foreignNominal, foreignCurrency_, foreignLegSchedule, foreignCcyIndex_, 0.0,
                                      domesticCurrency_, domesticLegSchedule, domesticCcyIndex_, 0.0, fxIdx));

    boost::shared_ptr<PricingEngine> engine;
    if (invertFxIndex_) {
        engine.reset(new CrossCcySwapEngine(foreignCurrency_, foreignDiscountRLH_, domesticCurrency_,
                                            domesticDiscountRLH_, spotFX_));
    } else {
        engine.reset(new CrossCcySwapEngine(domesticCurrency_, domesticDiscountRLH_, foreignCurrency_,
                                            foreignDiscountRLH_, spotFX_));
    }
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();

/* May need to adjust latestDate_ if you are projecting libor based
   on tenor length rather than from accrual date to accrual date. */
#ifdef QL_USE_INDEXED_COUPON
    if (termStructureHandle_ == foreignCcyIndex_->forwardingTermStructure()) {
        Size numCashflows = swap_->leg(0).size();
        Date endDate = latestDate_;
        if (numCashflows > 0) {
            for (Size i = numCashflows - 1; i >= 0; i--) {
                boost::shared_ptr<FloatingRateCoupon> lastFloating =
                    boost::dynamic_pointer_cast<FloatingRateCoupon>(swap_->leg(0)[i]);
                if (!lastFloating)
                    continue;
                else {
                    Date fixingValueDate = foreignCcyIndex_->valueDate(lastFloating->fixingDate());
                    endDate = domesticCcyIndex_->maturityDate(fixingValueDate);
                    Date endValueDate = foreignCcyIndex_->maturityDate(fixingValueDate);
                    latestDate_ = std::max(latestDate_, endValueDate);
                    break;
                }
            }
        }
    }
    if (termStructureHandle_ == domesticCcyIndex_->forwardingTermStructure()) {
        Size numCashflows = swap_->leg(1).size();
        Date endDate = latestDate_;
        if (numCashflows > 0) {
            for (Size i = numCashflows - 1; i >= 0; i--) {
                boost::shared_ptr<FloatingRateCoupon> lastFloating =
                    boost::dynamic_pointer_cast<FloatingRateCoupon>(swap_->leg(1)[i]);
                if (!lastFloating)
                    continue;
                else {
                    Date fixingValueDate = domesticCcyIndex_->valueDate(lastFloating->fixingDate());
                    endDate = domesticCcyIndex_->maturityDate(fixingValueDate);
                    Date endValueDate = domesticCcyIndex_->maturityDate(fixingValueDate);
                    latestDate_ = std::max(latestDate_, endValueDate);
                    break;
                }
            }
        }
    }
#endif
}

void CrossCcyBasisMtMResetSwapHelper::setTermStructure(YieldTermStructure* t) {

    bool observer = false;
    boost::shared_ptr<YieldTermStructure> temp(t, no_deletion);

    termStructureHandle_.linkTo(temp, observer);

    if (foreignCcyDiscountCurve_.empty())
        foreignDiscountRLH_.linkTo(temp, observer);
    else
        foreignDiscountRLH_.linkTo(*foreignCcyDiscountCurve_, observer);

    if (domesticCcyDiscountCurve_.empty())
        domesticDiscountRLH_.linkTo(temp, observer);
    else
        domesticDiscountRLH_.linkTo(*domesticCcyDiscountCurve_, observer);

    // the below are the curves used for FX forward rate projection (for the resetting cashflows)
    if (foreignCcyFxFwdRateCurve_.empty())
        foreignCcyFxFwdRateCurveRLH_.linkTo(temp, observer);
    else
        foreignCcyFxFwdRateCurveRLH_.linkTo(*foreignCcyFxFwdRateCurve_, observer);

    if (domesticCcyFxFwdRateCurve_.empty())
        domesticCcyFxFwdRateCurveRLH_.linkTo(temp, observer);
    else
        domesticCcyFxFwdRateCurveRLH_.linkTo(*domesticCcyFxFwdRateCurve_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Real CrossCcyBasisMtMResetSwapHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "Term structure needs to be set");
    swap_->recalculate();
    if (spreadOnForeignCcy_)
        return swap_->fairForeignSpread();
    else
        return swap_->fairDomesticSpread();
}

void CrossCcyBasisMtMResetSwapHelper::accept(AcyclicVisitor& v) {
    Visitor<CrossCcyBasisMtMResetSwapHelper>* v1 = dynamic_cast<Visitor<CrossCcyBasisMtMResetSwapHelper>*>(&v);
    if (v1)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
