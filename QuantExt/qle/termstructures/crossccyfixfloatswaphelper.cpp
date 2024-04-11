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

#include <boost/make_shared.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/math/comparison.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/termstructures/crossccyfixfloatswaphelper.hpp>

using QuantExt::CrossCcySwapEngine;
using QuantLib::close;
using QuantLib::FixedRateCoupon;
using QuantLib::YieldTermStructure;

namespace QuantExt {

CrossCcyFixFloatSwapHelper::CrossCcyFixFloatSwapHelper(
    const Handle<Quote>& rate, const Handle<Quote>& spotFx, Natural settlementDays, const Calendar& paymentCalendar,
    BusinessDayConvention paymentConvention, const Period& tenor, const Currency& fixedCurrency,
    Frequency fixedFrequency, BusinessDayConvention fixedConvention, const DayCounter& fixedDayCount,
    const QuantLib::ext::shared_ptr<IborIndex>& index, const Handle<YieldTermStructure>& floatDiscount,
    const Handle<Quote>& spread, bool endOfMonth)
    : RelativeDateRateHelper(rate), spotFx_(spotFx), settlementDays_(settlementDays), paymentCalendar_(paymentCalendar),
      paymentConvention_(paymentConvention), tenor_(tenor), fixedCurrency_(fixedCurrency),
      fixedFrequency_(fixedFrequency), fixedConvention_(fixedConvention), fixedDayCount_(fixedDayCount), index_(index),
      floatDiscount_(floatDiscount), spread_(spread), endOfMonth_(endOfMonth) {

    QL_REQUIRE(!spotFx_.empty(), "Spot FX quote cannot be empty.");
    QL_REQUIRE(fixedCurrency_ != index_->currency(), "Fixed currency should not equal float leg currency.");

    registerWith(spotFx_);
    registerWith(index_);
    registerWith(floatDiscount_);
    registerWith(spread_);

    initializeDates();
}

void CrossCcyFixFloatSwapHelper::update() {
    // Maybe FX spot quote or spread quote changed
    if (!close(spotFx_->value(), swap_->fixedNominal()) ||
        (!spread_.empty() && !close(spread_->value(), swap_->floatSpread()))) {
        initializeDates();
    }

    // Maybe evaluation date changed. RelativeDateRateHelper will take care of this
    // Note: if initializeDates() was called in above if statement, it will not be called
    // again in RelativeDateRateHelper::update() because evaluationDate_ is set in
    // initializeDates(). So, redundant instrument builds are avoided.
    RelativeDateRateHelper::update();
}

Real CrossCcyFixFloatSwapHelper::impliedQuote() const {
    QL_REQUIRE(termStructure_, "Term structure needs to be set");
    swap_->deepUpdate();
    return swap_->fairFixedRate();
}

void CrossCcyFixFloatSwapHelper::setTermStructure(YieldTermStructure* yts) {
    QuantLib::ext::shared_ptr<YieldTermStructure> temp(yts, null_deleter());
    termStructureHandle_.linkTo(temp, false);
    RelativeDateRateHelper::setTermStructure(yts);
}

void CrossCcyFixFloatSwapHelper::accept(AcyclicVisitor& v) {
    if (Visitor<CrossCcyFixFloatSwapHelper>* v1 = dynamic_cast<Visitor<CrossCcyFixFloatSwapHelper>*>(&v))
        v1->visit(*this);
    else
        RateHelper::accept(v);
}

void CrossCcyFixFloatSwapHelper::initializeDates() {

    // Swap start and end
    Date referenceDate = evaluationDate_ = Settings::instance().evaluationDate();
    referenceDate = paymentCalendar_.adjust(referenceDate);
    Date start = paymentCalendar_.advance(referenceDate, settlementDays_ * Days);
    Date end = start + tenor_;

    // Nominals
    Real floatNominal = 1.0;
    Real fixedNominal = spotFx_->value();

    // Fixed schedule
    Schedule fixedSchedule(start, end, Period(fixedFrequency_), paymentCalendar_, fixedConvention_, fixedConvention_,
                           DateGeneration::Backward, endOfMonth_);

    // Float schedule
    Schedule floatSchedule(start, end, index_->tenor(), paymentCalendar_, paymentConvention_, paymentConvention_,
                           DateGeneration::Backward, endOfMonth_);

    // Create the swap
    Natural paymentLag = 0;
    Spread floatSpread = spread_.empty() ? 0.0 : spread_->value();
    swap_.reset(new CrossCcyFixFloatSwap(CrossCcyFixFloatSwap::Payer, fixedNominal, fixedCurrency_, fixedSchedule, 0.0,
                                         fixedDayCount_, paymentConvention_, paymentLag, paymentCalendar_, floatNominal,
                                         index_->currency(), floatSchedule, index_, floatSpread, paymentConvention_,
                                         paymentLag, paymentCalendar_));

    earliestDate_ = swap_->startDate();
    maturityDate_ = swap_->maturityDate();

    // Swap is Payer => first leg is fixed leg
    latestRelevantDate_ = earliestDate_;
    for (Size i = 0; i < swap_->leg(0).size(); ++i) {
        latestRelevantDate_ = std::max(latestRelevantDate_, swap_->leg(0)[i]->date());
    }
    pillarDate_ = latestDate_ = latestRelevantDate_;

    // Attach engine
    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<CrossCcySwapEngine>(
        fixedCurrency_, termStructureHandle_, index_->currency(), floatDiscount_, spotFx_);
    swap_->setPricingEngine(engine);
}

} // namespace QuantExt
