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
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>

#include <qle/termstructures/crossccybasisswaphelper.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

CrossCcyBasisSwapHelper::CrossCcyBasisSwapHelper(
    const Handle<Quote>& spreadQuote, const Handle<Quote>& spotFX, Natural settlementDays,
    const Calendar& settlementCalendar, const Period& swapTenor, BusinessDayConvention rollConvention,
    const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& flatIndex, const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& spreadIndex,
    const Handle<YieldTermStructure>& flatDiscountCurve, const Handle<YieldTermStructure>& spreadDiscountCurve,
    bool eom, bool flatIsDomestic, boost::optional<Period> flatTenor, boost::optional<Period> spreadTenor,
    Real spreadOnFlatLeg, Real flatGearing, Real spreadGearing, const Calendar& flatCalendar,
    const Calendar& spreadCalendar, const std::vector<Natural>& spotFXSettleDaysVec,
    const std::vector<Calendar>& spotFXSettleCalendarVec, Size paymentLag, Size flatPaymentLag,
    boost::optional<bool> includeSpread, boost::optional<Period> lookback, boost::optional<Size> fixingDays,
    boost::optional<Size> rateCutoff, boost::optional<bool> isAveraged, boost::optional<bool> flatIncludeSpread,
    boost::optional<Period> flatLookback, boost::optional<Size> flatFixingDays, boost::optional<Size> flatRateCutoff,
    boost::optional<bool> flatIsAveraged, const bool telescopicValueDates)
    : RelativeDateRateHelper(spreadQuote), spotFX_(spotFX), settlementDays_(settlementDays),
      settlementCalendar_(settlementCalendar), swapTenor_(swapTenor), rollConvention_(rollConvention),
      flatIndex_(flatIndex), spreadIndex_(spreadIndex), flatDiscountCurve_(flatDiscountCurve),
      spreadDiscountCurve_(spreadDiscountCurve), eom_(eom), flatIsDomestic_(flatIsDomestic),
      flatTenor_(flatTenor ? *flatTenor : flatIndex_->tenor()),
      spreadTenor_(spreadTenor ? *spreadTenor : spreadIndex_->tenor()), spreadOnFlatLeg_(spreadOnFlatLeg),
      flatGearing_(flatGearing), spreadGearing_(spreadGearing), flatCalendar_(flatCalendar),
      spreadCalendar_(spreadCalendar), spotFXSettleDaysVec_(spotFXSettleDaysVec),
      spotFXSettleCalendarVec_(spotFXSettleCalendarVec), paymentLag_(paymentLag), flatPaymentLag_(flatPaymentLag),
      includeSpread_(includeSpread), lookback_(lookback), fixingDays_(fixingDays), rateCutoff_(rateCutoff),
      isAveraged_(isAveraged), flatIncludeSpread_(flatIncludeSpread), flatLookback_(flatLookback),
      flatFixingDays_(flatFixingDays), flatRateCutoff_(flatRateCutoff), flatIsAveraged_(flatIsAveraged),
      telescopicValueDates_(telescopicValueDates) {

    flatLegCurrency_ = flatIndex_->currency();
    spreadLegCurrency_ = spreadIndex_->currency();

    bool flatIndexHasCurve = !flatIndex_->forwardingTermStructure().empty();
    bool spreadIndexHasCurve = !spreadIndex_->forwardingTermStructure().empty();
    bool haveFlatDiscountCurve = !flatDiscountCurve_.empty();
    bool haveSpreadDiscountCurve = !spreadDiscountCurve_.empty();

    QL_REQUIRE(!(flatIndexHasCurve && spreadIndexHasCurve && haveFlatDiscountCurve && haveSpreadDiscountCurve),
               "Have all curves, "
               "nothing to solve for.");

    if (flatCalendar_.empty())
        flatCalendar_ = settlementCalendar;
    if (spreadCalendar_.empty())
        spreadCalendar_ = settlementCalendar;

    // check spotFXSettleDaysVec_ and spotFXSettleCalendarVec_
    Size numSpotFXSettleDays = spotFXSettleDaysVec_.size();
    QL_REQUIRE(numSpotFXSettleDays == spotFXSettleCalendarVec_.size(),
               "Array size of spot fx settlement days must equal that of spot fx settlement calendars");
    if (numSpotFXSettleDays == 0) {
        spotFXSettleDaysVec_.resize(1, 0);
        spotFXSettleCalendarVec_.resize(1, settlementCalendar);
    }

    /* Link the curve being bootstrapped to the index if the index has
       no projection curve */
    if (flatIndexHasCurve && haveFlatDiscountCurve) {
        if (!spreadIndexHasCurve) {
            spreadIndex_ = spreadIndex_->clone(termStructureHandle_);
            spreadIndex_->unregisterWith(termStructureHandle_);
        }
    } else if (spreadIndexHasCurve && haveSpreadDiscountCurve) {
        if (!flatIndexHasCurve) {
            flatIndex_ = flatIndex_->clone(termStructureHandle_);
            flatIndex_->unregisterWith(termStructureHandle_);
        }
    } else {
        QL_FAIL("Need one leg of the cross currency basis swap to "
                "have all of its curves.");
    }

    registerWith(spotFX_);
    registerWith(flatIndex_);
    registerWith(spreadIndex_);
    registerWith(flatDiscountCurve_);
    registerWith(spreadDiscountCurve_);

    initializeDates();
}

void CrossCcyBasisSwapHelper::initializeDates() {

    Date refDate = evaluationDate_;
    // if the evaluation date is not a business day
    // then move to the next business day
    refDate = settlementCalendar_.adjust(refDate);

    Date settlementDate = settlementCalendar_.advance(refDate, settlementDays_, Days);
    Date maturityDate = settlementDate + swapTenor_;

    // calc spotFXSettleDate
    Date spotFXSettleDate = refDate;
    Size numSpotFXSettleDays = spotFXSettleDaysVec_.size(); // guaranteed to be at least 1
    for (Size i = 0; i < numSpotFXSettleDays; i++) {
        // Guaranteed here that spotFXSettleDaysVec_ and spotFXSettleCalendarVec_ have the same size
        spotFXSettleDate = spotFXSettleCalendarVec_[i].advance(spotFXSettleDate, spotFXSettleDaysVec_[i], Days);
    }

    Schedule flatLegSchedule = MakeSchedule()
                                   .from(settlementDate)
                                   .to(maturityDate)
                                   .withTenor(flatTenor_)
                                   .withCalendar(flatCalendar_)
                                   .withConvention(rollConvention_)
                                   .endOfMonth(eom_);

    Schedule spreadLegSchedule = MakeSchedule()
                                     .from(settlementDate)
                                     .to(maturityDate)
                                     .withTenor(spreadTenor_)
                                     .withCalendar(spreadCalendar_)
                                     .withConvention(rollConvention_)
                                     .endOfMonth(eom_);

    Real flatLegNominal = 1.0;
    Real spreadLegNominal = 1.0;
    if (flatIsDomestic_) {
        flatLegNominal = spotFX_->value();
    } else {
        spreadLegNominal = spotFX_->value();
    }

    /* Arbitrarily set the spread leg as the pay leg */
    swap_ = QuantLib::ext::make_shared<CrossCcyBasisSwap>(
        spreadLegNominal, spreadLegCurrency_, spreadLegSchedule, spreadIndex_, 0.0, spreadGearing_, flatLegNominal,
        flatLegCurrency_, flatLegSchedule, flatIndex_, spreadOnFlatLeg_, flatGearing_, paymentLag_, flatPaymentLag_,
        includeSpread_, lookback_, fixingDays_, rateCutoff_, isAveraged_, flatIncludeSpread_, flatLookback_,
        flatFixingDays_, flatRateCutoff_, flatIsAveraged_, telescopicValueDates_);

    QuantLib::ext::shared_ptr<PricingEngine> engine;
    if (flatIsDomestic_) {
        engine = QuantLib::ext::make_shared<CrossCcySwapEngine>(flatLegCurrency_, flatDiscountRLH_, spreadLegCurrency_,
                                                        spreadDiscountRLH_, spotFX_, boost::none, Date(), Date(),
                                                        spotFXSettleDate);
    } else {
        engine = QuantLib::ext::make_shared<CrossCcySwapEngine>(spreadLegCurrency_, spreadDiscountRLH_, flatLegCurrency_,
                                                        flatDiscountRLH_, spotFX_, boost::none, Date(), Date(),
                                                        spotFXSettleDate);
    }
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    latestDate_ = swap_->maturityDate();

/* May need to adjust latestDate_ if you are projecting libor based
   on tenor length rather than from accrual date to accrual date. */
    if (!IborCoupon::Settings::instance().usingAtParCoupons()) {
        if (termStructureHandle_ == spreadIndex_->forwardingTermStructure()) {
            Size numCashflows = swap_->leg(0).size();
            if (numCashflows > 2) {
                QuantLib::ext::shared_ptr<FloatingRateCoupon> lastFloating =
                    QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(swap_->leg(0)[numCashflows - 2]);
                Date fixingValueDate = spreadIndex_->valueDate(lastFloating->fixingDate());
                Date endValueDate = spreadIndex_->maturityDate(fixingValueDate);
                latestDate_ = std::max(latestDate_, endValueDate);
            }
        }
        if (termStructureHandle_ == flatIndex_->forwardingTermStructure()) {
            Size numCashflows = swap_->leg(1).size();
            if (numCashflows > 2) {
                QuantLib::ext::shared_ptr<FloatingRateCoupon> lastFloating =
                    QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(swap_->leg(1)[numCashflows - 2]);
                Date fixingValueDate = flatIndex_->valueDate(lastFloating->fixingDate());
                Date endValueDate = flatIndex_->maturityDate(fixingValueDate);
                latestDate_ = std::max(latestDate_, endValueDate);
            }
        }
    }
}

void CrossCcyBasisSwapHelper::setTermStructure(YieldTermStructure* t) {

    bool observer = false;
    QuantLib::ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());

    termStructureHandle_.linkTo(temp, observer);

    if (flatDiscountCurve_.empty())
        flatDiscountRLH_.linkTo(temp, observer);
    else
        flatDiscountRLH_.linkTo(*flatDiscountCurve_, observer);

    if (spreadDiscountCurve_.empty())
        spreadDiscountRLH_.linkTo(temp, observer);
    else
        spreadDiscountRLH_.linkTo(*spreadDiscountCurve_, observer);

    RelativeDateRateHelper::setTermStructure(t);
}

Real CrossCcyBasisSwapHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "Term structure needs to be set");
    swap_->deepUpdate();
    return swap_->fairPaySpread();
}

void CrossCcyBasisSwapHelper::accept(AcyclicVisitor& v) {
    Visitor<CrossCcyBasisSwapHelper>* v1 = dynamic_cast<Visitor<CrossCcyBasisSwapHelper>*>(&v);
    if (v1)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
