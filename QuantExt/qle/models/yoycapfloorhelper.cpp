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

#include <qle/models/yoycapfloorhelper.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/cashflows/yoyinflationcoupon.hpp>
#include <vector>

using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::DateGeneration;
using QuantLib::DayCounter;
using QuantLib::Days;
using QuantLib::Handle;
using QuantLib::Leg;
using QuantLib::Natural;
using QuantLib::Period;
using QuantLib::PricingEngine;
using QuantLib::Quote;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::Schedule;
using QuantLib::Settings;
using QuantLib::YoYInflationCapFloor;
using QuantLib::yoyInflationLeg;
using QuantLib::YoYInflationIndex;
using std::vector;

namespace QuantExt {

YoYCapFloorHelper::YoYCapFloorHelper(const Handle<Quote>& premium,
    YoYInflationCapFloor::Type type,
    Rate strike,
    Natural settlementDays,
    const Period& tenor,
    const QuantLib::ext::shared_ptr<YoYInflationIndex>& yoyIndex,
    const Period& observationLag,
    const Calendar& yoyCalendar,
    BusinessDayConvention yoyConvention,
    const DayCounter& yoyDayCount,
    const Calendar& paymentCalendar,
    BusinessDayConvention paymentConvention,
    const Period& yoyTenor)
    : premium_(premium),
      evaluationDate_(Settings::instance().evaluationDate()),
      type_(type),
      strike_(strike),
      settlementDays_(settlementDays),
      tenor_(tenor),
      yoyIndex_(yoyIndex),
      observationLag_(observationLag),
      yoyCalendar_(yoyCalendar),
      yoyConvention_(yoyConvention),
      yoyDayCount_(yoyDayCount),
      paymentCalendar_(paymentCalendar),
      paymentConvention_(paymentConvention),
      yoyTenor_(yoyTenor) {

    registerWith(premium_);
    registerWith(Settings::instance().evaluationDate());
    registerWith(yoyIndex_);

    createCapFloor();
}

Real YoYCapFloorHelper::calibrationError() {
    yoyCapFloor_->setPricingEngine(engine_);
    return premium_->value() - yoyCapFloor_->NPV();
}

void YoYCapFloorHelper::update() {
    if (evaluationDate_ != Settings::instance().evaluationDate()) {
        evaluationDate_ = Settings::instance().evaluationDate();
        createCapFloor();
    }
    notifyObservers();
}

QuantLib::ext::shared_ptr<YoYInflationCapFloor> YoYCapFloorHelper::yoyCapFloor() const {
    return yoyCapFloor_;
}

void YoYCapFloorHelper::setPricingEngine(const QuantLib::ext::shared_ptr<PricingEngine>& engine) {
    engine_ = engine;
}

QuantLib::Real YoYCapFloorHelper::marketValue() const {
    return premium_->value();
}

QuantLib::Real YoYCapFloorHelper::modelValue() const {
    yoyCapFloor_->setPricingEngine(engine_);
    return yoyCapFloor_->NPV();
}

void YoYCapFloorHelper::createCapFloor() {

    // YoY cap floor start date and end date.
    auto start = yoyCalendar_.advance(evaluationDate_, settlementDays_ * Days);
    auto end = start + tenor_;

    // YoY leg schedule.
    Schedule yoySchedule(start, end, yoyTenor_, yoyCalendar_, yoyConvention_,
        yoyConvention_, DateGeneration::Backward, false);

    // YoY leg.
    Leg yoyLeg = yoyInflationLeg(yoySchedule, paymentCalendar_, yoyIndex_, observationLag_)
        .withNotionals(1.0)
        .withPaymentDayCounter(yoyDayCount_)
        .withPaymentAdjustment(paymentConvention_);

    // YoY cap floor.
    vector<Rate> strikes{ strike_ };
    yoyCapFloor_ = QuantLib::ext::make_shared<YoYInflationCapFloor>(type_, yoyLeg, strikes);
}

}
