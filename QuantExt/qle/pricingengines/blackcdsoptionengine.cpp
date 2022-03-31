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

#include <qle/pricingengines/blackcdsoptionengine.hpp>
#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <string>

using namespace QuantLib;
using std::string;

namespace QuantExt {

BlackCdsOptionEngine::BlackCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>& probability, Real recovery,
                                           const Handle<YieldTermStructure>& discount,
                                           const QuantLib::Handle<QuantExt::CreditVolCurve>& volatility)
    : probability_(probability), recovery_(recovery), discount_(discount), volatility_(volatility) {
    registerWith(probability_);
    registerWith(discount_);
    registerWith(volatility_);
}

const Handle<DefaultProbabilityTermStructure>& BlackCdsOptionEngine::probability() const {
    return probability_;
}

Real BlackCdsOptionEngine::recovery() const {
    return recovery_;
}

const Handle<YieldTermStructure> BlackCdsOptionEngine::discount() const {
    return discount_;
}

const Handle<CreditVolCurve> BlackCdsOptionEngine::volatility() const {
    return volatility_;
}


void BlackCdsOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.strikeType == CdsOption::Spread, "BlackCdsOptionEngine does not support valuation" <<
        " of single name options quoted in terms of strike price.");

    // Reference to the underlying forward starting CDS, from expiry date $t_E$ to maturity $T$.
    const auto& cds = *arguments_.swap;

    // Get the additional results of the underlying CDS (need to call NPV first for calculation).
    cds.NPV();
    results_.additionalResults = cds.additionalResults();

    // Add some entries to additional results.
    Real forward = cds.fairSpreadClean();
    results_.additionalResults["forwardSpread"] = forward;
    const Real& strike = arguments_.strike;
    results_.additionalResults["strikeSpread"] = strike;

    // Calculate risky PV01, as of the valuation date i.e. time 0, for the period from $t_E$ to underlying 
    // CDS maturity $T$. This risky PV01 does not include the non-risky accrual from the CDS premium leg coupon date 
    // immediately preceding the expiry date up to the expiry date.
    Real rpv01 = std::abs(cds.couponLegNPV() + cds.accrualRebateNPV()) / (cds.notional() * cds.runningSpread());
    results_.additionalResults["riskyAnnuity"] = rpv01;

    // Read the volatility from the volatility surface, assumed to have strike dimension in terms of spread.
    const Date& exerciseDate = arguments_.exercise->dates().front();
    Real underlyingLength = volatility_->dayCounter().yearFraction(exerciseDate, cds.maturity());
    Real vol = volatility_->volatility(exerciseDate, underlyingLength, strike, CreditVolCurve::Type::Spread);
    Real stdDev = vol * std::sqrt(volatility_->timeFromReference(exerciseDate));
    results_.additionalResults["volatility"] = vol;
    results_.additionalResults["standardDeviation"] = stdDev;

    // Option type
    Option::Type callPut = cds.side() == Protection::Buyer ? Option::Call : Option::Put;
    results_.additionalResults["callPut"] = callPut == Option::Call ? string("Call") : string("Put");

    // NPV, Section 9.3.7 O'Kane 2008.
    results_.value = rpv01 * cds.notional() * blackFormula(callPut, strike, forward, stdDev, 1.0);

    // If it is non-knockout and a payer, add the value of the default payout.
    // Section 2.2 of Richard J.Martin, 2019 or Section 9.3.7 O'Kane 2008.
    if (!arguments_.knocksOut && cds.side() == Protection::Buyer) {

        DiscountFactor disc = discount_->discount(exerciseDate);
        results_.additionalResults["discountToExercise"] = disc;

        Probability sp = probability_->survivalProbability(exerciseDate);
        results_.additionalResults["survivalProbabilityToExercise"] = sp;

        Real nonKoPv = disc * sp * (1 - recovery_);
        results_.additionalResults["nonKnockoutPv"] = nonKoPv;

        results_.value += nonKoPv;
    }
}

}
