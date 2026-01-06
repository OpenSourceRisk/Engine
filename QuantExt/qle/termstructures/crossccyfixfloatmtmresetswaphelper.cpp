/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/utilities/ratehelpers.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/termstructures/crossccyfixfloatmtmresetswaphelper.hpp>

#include <ql/cashflows/iborcoupon.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <ql/quotes/derivedquote.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

CrossCcyFixFloatMtMResetSwapHelper::CrossCcyFixFloatMtMResetSwapHelper(
    const Handle<Quote>& rate, const Handle<Quote>& spotFx, Natural settlementDays, const Calendar& paymentCalendar,
    BusinessDayConvention paymentConvention, const Period& tenor, const Currency& fixedCurrency,
    Frequency fixedFrequency, BusinessDayConvention fixedConvention, const DayCounter& fixedDayCount,
    const QuantLib::ext::shared_ptr<IborIndex>& index, const Handle<YieldTermStructure>& floatDiscount,
    const Handle<Quote>& spread, bool endOfMonth, bool resetsOnFloatLeg, bool telescopicValueDates,
    const QuantLib::Pillar::Choice pillarChoice, const std::vector<Natural>& spotFXSettleDaysVec,
    const std::vector<Calendar>& spotFXSettleCalendarVec, QuantLib::ext::optional<bool> includeSpread,
    QuantLib::ext::optional<Period> lookback, QuantLib::ext::optional<Size> fixingDays,
    QuantLib::ext::optional<Size> rateCutoff, QuantLib::ext::optional<bool> isAveraged)
    : RelativeDateRateHelper(rate), spotFx_(spotFx), settlementDays_(settlementDays), paymentCalendar_(paymentCalendar),
      paymentConvention_(paymentConvention), tenor_(tenor), fixedCurrency_(fixedCurrency),
      fixedFrequency_(fixedFrequency), fixedConvention_(fixedConvention), fixedDayCount_(fixedDayCount), index_(index),
      floatDiscount_(floatDiscount), spread_(spread), endOfMonth_(endOfMonth), resetsOnFloatLeg_(resetsOnFloatLeg),
      telescopicValueDates_(telescopicValueDates), pillarChoice_(pillarChoice), includeSpread_(includeSpread),
      lookback_(lookback), fixingDays_(fixingDays), rateCutoff_(rateCutoff), isAveraged_(isAveraged) {

    QL_REQUIRE(!spotFx_.empty(), "Spot FX quote cannot be empty.");
    QL_REQUIRE(fixedCurrency_ != index_->currency(), "Fixed currency should not equal float leg currency.");

    registerWith(spotFx_);
    registerWith(index_);
    registerWith(floatDiscount_);
    registerWith(spread_);

    initializeDates();
}

void CrossCcyFixFloatMtMResetSwapHelper::initializeDates() {

    // Swap start and end
    Date referenceDate = evaluationDate_ = Settings::instance().evaluationDate();
    referenceDate = paymentCalendar_.adjust(referenceDate);
    Date start = paymentCalendar_.advance(referenceDate, settlementDays_ * Days);
    Date end = start + tenor_;

    // calc spotFXSettleDate
    Date spotFXSettleDate = referenceDate;
    Size numSpotFXSettleDays = spotFXSettleDaysVec_.size(); // guaranteed to be at least 1
    for (Size i = 0; i < numSpotFXSettleDays; i++) {
        // Guaranteed here that spotFXSettleDaysVec_ and spotFXSettleCalendarVec_ have the same size
        spotFXSettleDate = spotFXSettleCalendarVec_[i].advance(spotFXSettleDate, spotFXSettleDaysVec_[i], Days);
    }

    // Fixed schedule
    Schedule fixedSchedule(start, end, Period(fixedFrequency_), paymentCalendar_, fixedConvention_, fixedConvention_,
                           DateGeneration::Backward, endOfMonth_);

    // Float schedule
    Schedule floatSchedule(start, end, index_->tenor(), paymentCalendar_, paymentConvention_, paymentConvention_,
                           DateGeneration::Backward, endOfMonth_);

    Real nominal = 1.0;

    // build an FX index for forward rate projection (TODO - review settlement and calendar)
    Natural paymentLag = 0;
    Spread floatSpread = spread_.empty() ? 0.0 : spread_->value();
    QuantLib::ext::shared_ptr<FxIndex> fxIdx;
    if (resetsOnFloatLeg_) {
        auto m = [](Real x) { return 1.0 / x; };
        Handle<Quote> fxSpotInv(QuantLib::ext::make_shared<DerivedQuote<decltype(m)>>(spotFx_, m));
        fxIdx = QuantLib::ext::make_shared<FxIndex>("dummy", settlementDays_, fixedCurrency_, index_->currency(),
                                                    paymentCalendar_, fxSpotInv, termStructureHandle_, floatDiscount_);
    } else {
        fxIdx = QuantLib::ext::make_shared<FxIndex>("dummy", settlementDays_, index_->currency(), fixedCurrency_,
                                                    paymentCalendar_, spotFx_, floatDiscount_, termStructureHandle_);
    }

    swap_ = QuantLib::ext::make_shared<CrossCcyFixFloatMtMResetSwap>(
        nominal, fixedCurrency_, fixedSchedule, quote().empty() || !quote()->isValid() ? 0.0 : quote()->value(),
        fixedDayCount_, paymentConvention_, paymentLag, paymentCalendar_, index_->currency(), floatSchedule, index_,
        floatSpread, paymentConvention_, paymentLag, paymentCalendar_, fxIdx, resetsOnFloatLeg_, true, includeSpread_,
        lookback_, fixingDays_, rateCutoff_, isAveraged_);

    // Attach engine
    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<CrossCcySwapEngine>(
        fixedCurrency_, termStructureHandle_, index_->currency(), floatDiscount_, spotFx_, QuantLib::ext::nullopt,
        Date(), Date(), spotFXSettleDate);
    swap_->setPricingEngine(engine);

    earliestDate_ = swap_->startDate();
    maturityDate_ = swap_->maturityDate();

    latestRelevantDate_ = determineLatestRelevantDate(swap_->legs(), {false, false});
    latestDate_ = pillarDate_ = determinePillarDate(pillarChoice_, maturityDate_, latestRelevantDate_);
}

void CrossCcyFixFloatMtMResetSwapHelper::setTermStructure(YieldTermStructure* t) {
    QuantLib::ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
    termStructureHandle_.linkTo(temp, false);
    RelativeDateRateHelper::setTermStructure(t);
}
void CrossCcyFixFloatMtMResetSwapHelper::update() {
    // Maybe FX spot quote or spread quote changed
    if (!close(spotFx_->value(), swap_->nominal()) ||
        (!spread_.empty() && !close(spread_->value(), swap_->floatSpread()))) {
        initializeDates();
    }

    // Maybe evaluation date changed. RelativeDateRateHelper will take care of this
    // Note: if initializeDates() was called in above if statement, it will not be called
    // again in RelativeDateRateHelper::update() because evaluationDate_ is set in
    // initializeDates(). So, redundant instrument builds are avoided.
    RelativeDateRateHelper::update();
}

Real CrossCcyFixFloatMtMResetSwapHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "Term structure needs to be set");
    swap_->deepUpdate();
    return swap_->fairFixedRate();
}

void CrossCcyFixFloatMtMResetSwapHelper::accept(AcyclicVisitor& v) {
    Visitor<CrossCcyFixFloatMtMResetSwapHelper>* v1 = dynamic_cast<Visitor<CrossCcyFixFloatMtMResetSwapHelper>*>(&v);
    if (v1)
        v1->visit(*this);
    else
        RateHelper::accept(v);
}
} // namespace QuantExt
