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
                                           const Handle<Quote>& volatility)
    : probability_(probability), recoveryRate_(recoveryRate), termStructure_(termStructure), volatility_(volatility) {

    registerWith(probability_);
    registerWith(termStructure_);
    registerWith(volatility_);
}

void BlackCdsOptionEngine::calculate() const {

    Date maturityDate = arguments_.swap->coupons().front()->date();
    Date exerciseDate = arguments_.exercise->date(0);
    QL_REQUIRE(maturityDate > exerciseDate, "Underlying CDS should start after option maturity");
    Date settlement = termStructure_->referenceDate();

    Rate spotFwdSpread = arguments_.swap->fairSpread();
    Rate swapSpread = arguments_.swap->runningSpread();

    DayCounter tSDc = termStructure_->dayCounter();

    // The sense of the underlying/option has to be sent this way
    // to the Black formula, no sign.
    Real riskyAnnuity = std::fabs(arguments_.swap->couponLegNPV() / swapSpread);
    results_.riskyAnnuity = riskyAnnuity;

    // incorporate upfront amount
    swapSpread -=
        (arguments_.swap->side() == Protection::Buyer ? -1.0 : 1.0) * arguments_.swap->upfrontNPV() / riskyAnnuity;

    Time T = tSDc.yearFraction(settlement, exerciseDate);

    Real stdDev = volatility_->value() * std::sqrt(T);
    Option::Type callPut = (arguments_.side == Protection::Buyer) ? Option::Call : Option::Put;

    results_.value = blackFormula(callPut, swapSpread, spotFwdSpread, stdDev, riskyAnnuity);

    // if a non knock-out payer option, add front end protection value
    if (arguments_.side == Protection::Buyer && !arguments_.knocksOut) {
        Real frontEndProtection = callPut * arguments_.swap->notional() * (1. - recoveryRate_) *
                                  probability_->defaultProbability(exerciseDate) *
                                  termStructure_->discount(exerciseDate);
        results_.value += frontEndProtection;
    }
}

Handle<YieldTermStructure> BlackCdsOptionEngine::termStructure() { return termStructure_; }

Handle<Quote> BlackCdsOptionEngine::volatility() { return volatility_; }
} // namespace QuantExt
