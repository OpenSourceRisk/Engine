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
    const Calendar& settlementCalendar, const Period& swapTenor,
    BusinessDayConvention rollConvention,
    const boost::shared_ptr<QuantLib::IborIndex>& fgnCcyIndex,
    const boost::shared_ptr<QuantLib::IborIndex>& domCcyIndex,
    const Handle<YieldTermStructure>& fgnCcyDiscountCurve,
    const Handle<YieldTermStructure>& domCcyDiscountCurve,
    const Handle<YieldTermStructure>& fgnCcyFxFwdRateCurve,
    const Handle<YieldTermStructure>& domCcyFxFwdRateCurve,
    bool eom,
    bool spreadOnFgnCcy) 
    : RelativeDateRateHelper(spreadQuote), spotFX_(spotFX), settlementDays_(settlementDays),
      settlementCalendar_(settlementCalendar), swapTenor_(swapTenor), rollConvention_(rollConvention), 
      fgnCcyIndex_(fgnCcyIndex), domCcyIndex_(domCcyIndex), fgnCcyDiscountCurve_(fgnCcyDiscountCurve),
      domCcyDiscountCurve_(domCcyDiscountCurve), fgnCcyFxFwdRateCurve_(fgnCcyFxFwdRateCurve),
      domCcyFxFwdRateCurve_(domCcyFxFwdRateCurve), eom_(eom), spreadOnFgnCcy_(spreadOnFgnCcy) {

    fgnCurrency_ = fgnCcyIndex_->currency();
    domCurrency_ = domCcyIndex_->currency();
    QL_REQUIRE(fgnCurrency_ != domCurrency_, "matching currencies not allowed on CrossCcyBasisMtMResetSwapHelper");

    bool fgnIndexHasCurve = !fgnCcyIndex_->forwardingTermStructure().empty();
    bool domIndexHasCurve = !domCcyIndex_->forwardingTermStructure().empty();
    bool haveFgnDiscountCurve = !fgnCcyDiscountCurve_.empty();
    bool haveDomDiscountCurve = !domCcyDiscountCurve_.empty();

    QL_REQUIRE(!(fgnIndexHasCurve && domIndexHasCurve && haveFgnDiscountCurve && haveDomDiscountCurve),
        "CrossCcyBasisMtMResetSwapHelper - Have all curves, nothing to solve for.");

    /* Link the curve being bootstrapped to the index if the index has
    no projection curve */
    if (fgnIndexHasCurve && haveFgnDiscountCurve) {
        if (!domIndexHasCurve) {
            domCcyIndex_ = domCcyIndex_->clone(termStructureHandle_);
            domCcyIndex_->unregisterWith(termStructureHandle_);
        }
        // if we have both index and discounting curve on foreign leg,
          // check fgnCcyFxFwdRateCurve and link it to fgn discount curve if empty
          // (we are bootstrapping on domestic leg in this instance, so fgn leg needs to be fully determined
        if (fgnCcyFxFwdRateCurve_.empty())
            fgnCcyFxFwdRateCurve_ = fgnCcyDiscountCurve_;
    }
    else if (domIndexHasCurve && haveDomDiscountCurve) {
        if (!fgnIndexHasCurve) {
            fgnCcyIndex_ = fgnCcyIndex_->clone(termStructureHandle_);
            fgnCcyIndex_->unregisterWith(termStructureHandle_);
        }
        // if we have both index and discounting curve on domestic leg,
        // check domCcyFxFwdRateCurve and link it to dom discount curve if empty
        // (we are bootstrapping on fgn leg in this instance, so dom leg needs to be fully determined
        if (domCcyFxFwdRateCurve_.empty())
            domCcyFxFwdRateCurve_ = domCcyDiscountCurve_;
    }
    else {
        QL_FAIL("Need one leg of the cross currency basis swap to "
            "have all of its curves.");
    }

    registerWith(spotFX_);
    registerWith(domCcyIndex_);
    registerWith(fgnCcyIndex_);
    registerWith(fgnCcyDiscountCurve_);
    registerWith(domCcyDiscountCurve_);
    registerWith(fgnCcyFxFwdRateCurve_);
    registerWith(domCcyFxFwdRateCurve_);

    initializeDates();

}

void CrossCcyBasisMtMResetSwapHelper::initializeDates() {

    Date refDate = evaluationDate_;
    // if the evaluation date is not a business day
    // then move to the next business day
    refDate = settlementCalendar_.adjust(refDate);

    Date settlementDate = settlementCalendar_.advance(refDate, settlementDays_, Days);
    Date maturityDate = settlementDate + swapTenor_;

    Period fgnLegTenor = fgnCcyIndex_->tenor();
    Schedule fgnLegSchedule = MakeSchedule()
                                   .from(settlementDate)
                                   .to(maturityDate)
                                   .withTenor(fgnLegTenor)
                                   .withCalendar(settlementCalendar_)
                                   .withConvention(rollConvention_)
                                   .endOfMonth(eom_);

    Period domLegTenor = domCcyIndex_->tenor();
    Schedule domLegSchedule = MakeSchedule()
                                     .from(settlementDate)
                                     .to(maturityDate)
                                     .withTenor(domLegTenor)
                                     .withCalendar(settlementCalendar_)
                                     .withConvention(rollConvention_)
                                     .endOfMonth(eom_);

    Real fgnNominal = 1.0;
    // build an FX index for forward rate projection (TODO - review settlement and calendar)
    boost::shared_ptr<FxIndex> fxIdx = 
        boost::make_shared<FxIndex>("dummy", 0, fgnCurrency_, domCurrency_, NullCalendar(), 
            fgnCcyFxFwdRateCurveRLH_, domCcyFxFwdRateCurveRLH_);

    swap_ = boost::shared_ptr<CrossCcyBasisMtMResetSwap>(
        new CrossCcyBasisMtMResetSwap(fgnNominal, fgnCurrency_, fgnLegSchedule, fgnCcyIndex_, 0.0,
            domCurrency_, domLegSchedule, domCcyIndex_, 0.0, fxIdx, false));

    boost::shared_ptr<PricingEngine> engine = 
        boost::make_shared<CrossCcySwapEngine>(domCurrency_, domDiscountRLH_, fgnCurrency_, fgnDiscountRLH_, spotFX_);
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();

/* May need to adjust latestDate_ if you are projecting libor based
   on tenor length rather than from accrual date to accrual date. */
#ifdef QL_USE_INDEXED_COUPON
    if (termStructureHandle_ == fgnCcyIndex_->forwardingTermStructure()) {
        Size numCashflows = swap_->leg(0).size();
        Date endDate = latestDate_;
        if (numCashflows > 0) {
            for (Size i = numCashflows - 1; i >= 0; i--) {
                boost::shared_ptr<FloatingRateCoupon> lastFloating =
                    boost::dynamic_pointer_cast<FloatingRateCoupon>(swap_->leg(0)[i]);
                if (!lastFloating)
                    continue;
                else {
                    Date fixingValueDate = fgnCcyIndex_->valueDate(lastFloating->fixingDate());
                    endDate = domCcyIndex_->maturityDate(fixingValueDate);
                    Date endValueDate = fgnCcyIndex_->maturityDate(fixingValueDate);
                    latestDate_ = std::max(latestDate_, endValueDate);
                    break;
                }
            }
        }
    }
    if (termStructureHandle_ == domCcyIndex_->forwardingTermStructure()) {
        Size numCashflows = swap_->leg(1).size();
        Date endDate = latestDate_;
        if (numCashflows > 0) {
            for (Size i = numCashflows - 1; i >= 0; i--) {
                boost::shared_ptr<FloatingRateCoupon> lastFloating =
                    boost::dynamic_pointer_cast<FloatingRateCoupon>(swap_->leg(1)[i]);
                if (!lastFloating)
                    continue;
                else {
                    Date fixingValueDate = domCcyIndex_->valueDate(lastFloating->fixingDate());
                    endDate = domCcyIndex_->maturityDate(fixingValueDate);
                    Date endValueDate = domCcyIndex_->maturityDate(fixingValueDate);
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

    if (fgnCcyDiscountCurve_.empty())
        fgnDiscountRLH_.linkTo(temp, observer);
    else
        fgnDiscountRLH_.linkTo(*fgnCcyDiscountCurve_, observer);

    if (domCcyDiscountCurve_.empty())
        domDiscountRLH_.linkTo(temp, observer);
    else
        domDiscountRLH_.linkTo(*domCcyDiscountCurve_, observer);

    // the below are the curves used for FX forward rate projection (for the resetting cashflows)
    if (fgnCcyFxFwdRateCurve_.empty())
        fgnCcyFxFwdRateCurveRLH_.linkTo(temp, observer);
    else
        fgnCcyFxFwdRateCurveRLH_.linkTo(*fgnCcyFxFwdRateCurve_, observer);

    if (domCcyFxFwdRateCurve_.empty())
        domCcyFxFwdRateCurveRLH_.linkTo(temp, observer);
    else
        domCcyFxFwdRateCurveRLH_.linkTo(*domCcyFxFwdRateCurve_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Real CrossCcyBasisMtMResetSwapHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "Term structure needs to be set");
    swap_->recalculate();
    if (spreadOnFgnCcy_)
        return swap_->fairFgnSpread();
    else
        return swap_->fairDomSpread();
}

void CrossCcyBasisMtMResetSwapHelper::accept(AcyclicVisitor& v) {
    Visitor<CrossCcyBasisMtMResetSwapHelper>* v1 = dynamic_cast<Visitor<CrossCcyBasisMtMResetSwapHelper>*>(&v);
    if (v1)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
