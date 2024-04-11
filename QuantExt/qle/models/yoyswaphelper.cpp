/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/models/yoyswaphelper.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/cashflows/inflationcouponpricer.hpp>

using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::DateGeneration;
using QuantLib::DayCounter;
using QuantLib::Days;
using QuantLib::Handle;
using QuantLib::JointCalendar;
using QuantLib::Natural;
using QuantLib::Period;
using QuantLib::PricingEngine;
using QuantLib::Quote;
using QuantLib::Real;
using QuantLib::Schedule;
using QuantLib::Settings;
using QuantLib::YearOnYearInflationSwap;
using QuantLib::YieldTermStructure;
using QuantLib::YoYInflationIndex;

namespace QuantExt {

YoYSwapHelper::YoYSwapHelper(const Handle<Quote>& rate,
    Natural settlementDays,
    const Period& tenor,
    const QuantLib::ext::shared_ptr<YoYInflationIndex>& yoyIndex,
    const Handle<YieldTermStructure>& rateCurve,
    const Period& observationLag,
    const Calendar& yoyCalendar,
    BusinessDayConvention yoyConvention,
    const DayCounter& yoyDayCount,
    const Calendar& fixedCalendar,
    BusinessDayConvention fixedConvention,
    const DayCounter& fixedDayCount,
    const Calendar& paymentCalendar,
    BusinessDayConvention paymentConvention,
    const Period& fixedTenor,
    const Period& yoyTenor)
    : rate_(rate),
      evaluationDate_(Settings::instance().evaluationDate()),
      settlementDays_(settlementDays),
      tenor_(tenor),
      yoyIndex_(yoyIndex),
      rateCurve_(rateCurve),
      observationLag_(observationLag),
      yoyCalendar_(yoyCalendar),
      yoyConvention_(yoyConvention),
      yoyDayCount_(yoyDayCount),
      fixedCalendar_(fixedCalendar),
      fixedConvention_(fixedConvention),
      fixedDayCount_(fixedDayCount),
      paymentCalendar_(paymentCalendar),
      paymentConvention_(paymentConvention),
      fixedTenor_(fixedTenor),
      yoyTenor_(yoyTenor) {

    registerWith(rate_);
    registerWith(Settings::instance().evaluationDate());
    registerWith(yoyIndex_);

    createSwap();
}

Real YoYSwapHelper::calibrationError() {
    yoySwap_->setPricingEngine(engine_);
    return rate_->value() - yoySwap_->fairRate();
}

void YoYSwapHelper::update() {
    if (evaluationDate_ != Settings::instance().evaluationDate()) {
        evaluationDate_ = Settings::instance().evaluationDate();
        createSwap();
    }
    notifyObservers();
}

QuantLib::ext::shared_ptr<YearOnYearInflationSwap> YoYSwapHelper::yoySwap() const {
    return yoySwap_;
}

void YoYSwapHelper::setPricingEngine(const QuantLib::ext::shared_ptr<PricingEngine>& engine) {
    engine_ = engine;
}

Real YoYSwapHelper::marketRate() const {
    return rate_->value();
}

Real YoYSwapHelper::modelRate() const {
    yoySwap_->setPricingEngine(engine_);
    return yoySwap_->fairRate();
}

void YoYSwapHelper::createSwap() {

    // YoY swap start date and end date.
    JointCalendar jc(yoyCalendar_, fixedCalendar_);
    auto start = jc.advance(evaluationDate_, settlementDays_ * Days);
    auto end = start + tenor_;

    // YoY fixed leg schedule.
    Schedule fixedSchedule(start, end, fixedTenor_, fixedCalendar_, fixedConvention_,
        fixedConvention_, DateGeneration::Backward, false);

    // YoY leg schedule.
    Schedule yoySchedule(start, end, yoyTenor_, yoyCalendar_, yoyConvention_,
        yoyConvention_, DateGeneration::Backward, false);

    // YoY swap
    yoySwap_ = QuantLib::ext::make_shared<YearOnYearInflationSwap>(YearOnYearInflationSwap::Payer, 1.0,
        fixedSchedule, 0.01, fixedDayCount_, yoySchedule, yoyIndex_, observationLag_, 0.0, yoyDayCount_,
        paymentCalendar_, paymentConvention_);

    // set coupon pricer
    auto pricer = QuantLib::ext::make_shared<QuantLib::YoYInflationCouponPricer>(rateCurve_);
    for (auto& c : yoySwap_->yoyLeg()) {
        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<QuantLib::YoYInflationCoupon>(c))
            cpn->setPricer(pricer);
    }
}

} // namespace QuantExt
