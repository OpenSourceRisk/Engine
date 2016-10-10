/*
 Copyright (C) 2015 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program.
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

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

#include <qle/models/fxoptionhelper.hpp>
#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

FxOptionHelper::FxOptionHelper(const Period& maturity, const Calendar& calendar, const Real strike,
                               const Handle<Quote> fxSpot, const Handle<Quote> volatility,
                               const Handle<YieldTermStructure>& domesticYield,
                               const Handle<YieldTermStructure>& foreignYield,
                               CalibrationHelper::CalibrationErrorType errorType)
    : CalibrationHelper(volatility, domesticYield, errorType), hasMaturity_(true), maturity_(maturity),
      calendar_(calendar), strike_(strike), fxSpot_(fxSpot), foreignYield_(foreignYield) {
    registerWith(fxSpot_);
    registerWith(foreignYield_);
}

FxOptionHelper::FxOptionHelper(const Date& exerciseDate, const Real strike, const Handle<Quote> fxSpot,
                               const Handle<Quote> volatility, const Handle<YieldTermStructure>& domesticYield,
                               const Handle<YieldTermStructure>& foreignYield,
                               CalibrationHelper::CalibrationErrorType errorType)
    : CalibrationHelper(volatility, domesticYield, errorType), hasMaturity_(false), exerciseDate_(exerciseDate),
      strike_(strike), fxSpot_(fxSpot), foreignYield_(foreignYield) {
    registerWith(fxSpot_);
    registerWith(foreignYield_);
}

void FxOptionHelper::performCalculations() const {
    if (hasMaturity_)
        exerciseDate_ = calendar_.advance(termStructure_->referenceDate(), maturity_);
    tau_ = termStructure_->timeFromReference(exerciseDate_);
    atm_ = fxSpot_->value() * foreignYield_->discount(tau_) / termStructure_->discount(tau_);
    effStrike_ = strike_;
    if (effStrike_ == Null<Real>())
        effStrike_ = atm_;
    type_ = effStrike_ >= atm_ ? Option::Call : Option::Put;
    boost::shared_ptr<StrikedTypePayoff> payoff(new PlainVanillaPayoff(type_, effStrike_));
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(exerciseDate_);
    option_ = boost::shared_ptr<VanillaOption>(new VanillaOption(payoff, exercise));
    CalibrationHelper::performCalculations();
}

Real FxOptionHelper::modelValue() const {
    calculate();
    option_->setPricingEngine(engine_);
    return option_->NPV();
}

Real FxOptionHelper::blackPrice(Real volatility) const {
    calculate();
    const Real stdDev = volatility * std::sqrt(tau_);
    return blackFormula(type_, effStrike_, atm_, stdDev, termStructure_->discount(tau_));
}

} // namespace QuantExt
