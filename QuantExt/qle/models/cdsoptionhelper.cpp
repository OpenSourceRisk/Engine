/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/models/cdsoptionhelper.hpp>
#include <qle/pricingengines/blackcdsoptionengine.hpp>
#include <qle/pricingengines/midpointcdsengine.hpp>

#include <ql/exercise.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/time/schedule.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

CdsOptionHelper::CdsOptionHelper(const Date& exerciseDate, const Handle<Quote>& volatility, const Protection::Side side,
                                 const Schedule& schedule, const BusinessDayConvention paymentConvention,
                                 const DayCounter& dayCounter,
                                 const Handle<DefaultProbabilityTermStructure>& probability, const Real recoveryRate,
                                 const Handle<YieldTermStructure>& termStructure, const Rate spread, const Rate upfront,
                                 const bool settlesAccrual, const bool paysAtDefaultTime, const Date protectionStart,
                                 const Date upfrontDate, const boost::shared_ptr<Claim>& claim,
                                 const CalibrationHelper::CalibrationErrorType errorType)
    : CalibrationHelper(volatility, termStructure, errorType), blackVol_(boost::make_shared<SimpleQuote>(0.0)) {

    boost::shared_ptr<PricingEngine> cdsEngine =
        boost::make_shared<QuantExt::MidPointCdsEngine>(probability, recoveryRate, termStructure);

    boost::shared_ptr<CreditDefaultSwap> tmp;
    if (upfront == Null<Real>())
        tmp = boost::shared_ptr<CreditDefaultSwap>(new CreditDefaultSwap(side, 1.0, 0.02, schedule, paymentConvention,
                                                                         dayCounter, settlesAccrual, paysAtDefaultTime,
                                                                         protectionStart, claim));
    else
        tmp = boost::shared_ptr<CreditDefaultSwap>(
            new CreditDefaultSwap(side, 1.0, upfront, 0.02, schedule, paymentConvention, dayCounter, settlesAccrual,
                                  paysAtDefaultTime, protectionStart, upfrontDate, claim));
    tmp->setPricingEngine(cdsEngine);

    Real strike = spread == Null<Real>() ? tmp->fairSpread() : spread;
    if (upfront == Null<Real>())
        cds_ = boost::shared_ptr<CreditDefaultSwap>(new CreditDefaultSwap(side, 1.0, strike, schedule,
                                                                          paymentConvention, dayCounter, settlesAccrual,
                                                                          paysAtDefaultTime, protectionStart, claim));
    else
        cds_ = boost::shared_ptr<CreditDefaultSwap>(
            new CreditDefaultSwap(side, 1.0, upfront, strike, schedule, paymentConvention, dayCounter, settlesAccrual,
                                  paysAtDefaultTime, protectionStart, upfrontDate, claim));

    cds_->setPricingEngine(cdsEngine);

    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(exerciseDate);

    option_ = boost::make_shared<CdsOption>(cds_, exercise, true);
    Handle<BlackVolTermStructure> h(
        boost::make_shared<BlackConstantVol>(0, NullCalendar(), Handle<Quote>(blackVol_), Actual365Fixed()));

    blackEngine_ = boost::make_shared<BlackCdsOptionEngine>(probability, recoveryRate, termStructure, h);
}

Real CdsOptionHelper::modelValue() const {
    calculate();
    option_->setPricingEngine(engine_);
    return option_->NPV();
}

Real CdsOptionHelper::blackPrice(Volatility sigma) const {
    calculate();
    blackVol_->setValue(sigma);
    option_->setPricingEngine(blackEngine_);
    Real value = option_->NPV();
    option_->setPricingEngine(engine_);
    return value;
}

} // namespace QuantExt
