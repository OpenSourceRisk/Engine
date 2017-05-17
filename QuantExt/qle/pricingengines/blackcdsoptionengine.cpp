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

/*
 Copyright (C) 2008 Roland Stamm
 Copyright (C) 2009 Jose Aparicio

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/pricingengines/blackcdsoptionengine.hpp>

#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

namespace QuantExt {

BlackCdsOptionEngine::BlackCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>& probability,
                                           Real recoveryRate, const Handle<YieldTermStructure>& termStructure,
                                           const Handle<BlackVolTermStructure>& volatility)
    : BlackCdsOptionEngineBase(termStructure, volatility), probability_(probability), recoveryRate_(recoveryRate) {
    registerWith(probability_);
    registerWith(termStructure_);
    registerWith(volatility_);
}

Real BlackCdsOptionEngine::recoveryRate() const { return recoveryRate_; }

Real BlackCdsOptionEngine::defaultProbability(const Date& d) const { return probability_->defaultProbability(d); }

void BlackCdsOptionEngine::calculate() const {
    BlackCdsOptionEngineBase::calculate(*arguments_.swap, arguments_.exercise->dates().front(), arguments_.knocksOut,
                                        results_);
}

BlackCdsOptionEngineBase::BlackCdsOptionEngineBase(const Handle<YieldTermStructure>& termStructure,
                                                   const Handle<BlackVolTermStructure>& vol)
    : termStructure_(termStructure), volatility_(vol) {}

void BlackCdsOptionEngineBase::calculate(const CreditDefaultSwap& swap, const Date& exerciseDate, const bool knocksOut,
                                         CdsOption::results& results) const {

    Date maturityDate = swap.coupons().front()->date();
    QL_REQUIRE(maturityDate > exerciseDate, "Underlying CDS should start after option maturity");
    Date settlement = termStructure_->referenceDate();

    Rate spotFwdSpread = swap.fairSpread();
    Rate swapSpread = swap.runningSpread();

    DayCounter tSDc = termStructure_->dayCounter();

    // The sense of the underlying/option has to be sent this way
    // to the Black formula, no sign.
    Real riskyAnnuity = std::fabs(swap.couponLegNPV() / swapSpread);
    results.riskyAnnuity = riskyAnnuity;

    // incorporate upfront amount
    swapSpread -=
        (swap.side() == Protection::Buyer ? -1.0 : 1.0) * (swap.upfrontNPV() - swap.accrualRebateNPV()) / riskyAnnuity;

    Time T = tSDc.yearFraction(settlement, exerciseDate);

    Real stdDev = volatility_->blackVol(exerciseDate, 1.0, true) * std::sqrt(T);
    Option::Type callPut = (swap.side() == Protection::Buyer) ? Option::Call : Option::Put;

    results.value = blackFormula(callPut, swapSpread, spotFwdSpread, stdDev, riskyAnnuity);

    // if a non knock-out payer option, add front end protection value
    if (swap.side() == Protection::Buyer && !knocksOut) {
        Real frontEndProtection = callPut * swap.notional() * (1. - recoveryRate()) * defaultProbability(exerciseDate) *
                                  termStructure_->discount(exerciseDate);
        results.value += frontEndProtection;
    }
}

Handle<YieldTermStructure> BlackCdsOptionEngineBase::termStructure() { return termStructure_; }

Handle<BlackVolTermStructure> BlackCdsOptionEngineBase::volatility() { return volatility_; }
} // namespace QuantExt
