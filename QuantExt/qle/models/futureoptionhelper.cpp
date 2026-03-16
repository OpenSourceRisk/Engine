/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/models/futureoptionhelper.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

FutureOptionHelper::FutureOptionHelper(
        const Period& maturity, const Calendar& calendar, const Real strike,
        const Handle<QuantExt::PriceTermStructure> priceCurve, const Handle<Quote> volatility, 
        BlackCalibrationHelper::CalibrationErrorType errorType)
    : BlackCalibrationHelper(volatility, errorType), priceCurve_(priceCurve), hasMaturity_(true),
      maturity_(maturity), calendar_(calendar), strike_(strike) {
    registerWith(priceCurve_);
}

FutureOptionHelper::FutureOptionHelper(
        const Date& exerciseDate, const Real strike,
        const Handle<QuantExt::PriceTermStructure> priceCurve, const Handle<Quote> volatility,
        BlackCalibrationHelper::CalibrationErrorType errorType)
    : BlackCalibrationHelper(volatility, errorType), priceCurve_(priceCurve), hasMaturity_(false),
      exerciseDate_(exerciseDate), strike_(strike) {
    registerWith(priceCurve_);
}

void FutureOptionHelper::performCalculations() const {
    if (hasMaturity_)
        exerciseDate_ = calendar_.advance(priceCurve_->referenceDate(), maturity_);
    tau_ = priceCurve_->timeFromReference(exerciseDate_);
    atm_ = priceCurve_->price(tau_);
    effStrike_ = strike_;
    if (effStrike_ == Null<Real>())
        effStrike_ = atm_;
    type_ = effStrike_ >= atm_ ? Option::Call : Option::Put;
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type_, effStrike_));
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate_);
    option_ = QuantLib::ext::shared_ptr<VanillaOption>(new VanillaOption(payoff, exercise));
    BlackCalibrationHelper::performCalculations();
}

Real FutureOptionHelper::modelValue() const {
    calculate();
    option_->setPricingEngine(engine_);
    return option_->NPV();
}

Real FutureOptionHelper::blackPrice(Real volatility) const {
    calculate();
    const Real stdDev = volatility * std::sqrt(tau_);
    return blackFormula(type_, effStrike_, atm_, stdDev, 1.0);
}

} // namespace QuantExt
