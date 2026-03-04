/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 Copyright (C) 2026 AcadiaSoft, Inc.
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
#include <qle/pricingengines/blackcdsoptionengine.hpp>
#include <qle/pricingengines/forwardriskyannuitystrike.hpp>

#include <string>

using namespace QuantLib;
using std::string;

namespace QuantExt {

BlackCdsOptionEngine::BlackCdsOptionEngine(const Handle<DefaultProbabilityTermStructure>& probability, Real recovery,
                                           const Handle<YieldTermStructure>& discountSwapCurrency,
                                           const Handle<YieldTermStructure>& discountTradeCollateral,
                                           const Handle<QuantExt::CreditVolCurve>& volatility)
    : probability_(probability), recovery_(recovery), discountSwapCurrency_(discountSwapCurrency),
      discountTradeCollateral_(discountTradeCollateral), volatility_(volatility) {
    registerWith(probability_);
    registerWith(discountSwapCurrency_);
    registerWith(discountTradeCollateral_);
    registerWith(volatility_);
}

const Handle<DefaultProbabilityTermStructure>& BlackCdsOptionEngine::probability() const {
    return probability_;
}

Real BlackCdsOptionEngine::recovery() const {
    return recovery_;
}

const Handle<YieldTermStructure> BlackCdsOptionEngine::discountSwapCurrency() const {
    return discountSwapCurrency_;
}

const Handle<YieldTermStructure> BlackCdsOptionEngine::discountTradeCollateral() const {
    return discountTradeCollateral_;
}

const Handle<CreditVolCurve> BlackCdsOptionEngine::volatility() const {
    return volatility_;
}


void BlackCdsOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.strikeType == CdsOption::Spread, "BlackCdsOptionEngine does not support valuation" <<
        " of single name options quoted in terms of strike price.");

    // Reference to the underlying forward starting CDS, from expiry date $t_E$ to maturity $T$.
    const auto& cds = *arguments_.swap;
    const Date& exerciseDate = arguments_.exercise->dates().front();
    DiscountFactor discTradeCollToExercise = discountTradeCollateral_->discount(exerciseDate);
    DiscountFactor discSwapCurrToExercise = discountSwapCurrency_->discount(exerciseDate);
    bool cashSettled = arguments_.settlementType == Settlement::Cash;

    // Get the additional results of the underlying CDS (need to call NPV first for calculation).
    cds.NPV();
    results_.additionalResults = cds.additionalResults();

    // Add some entries to additional results.
    Real forward = cds.fairSpreadClean();
    results_.additionalResults["forwardSpread"] = forward;
    const Real& strike = arguments_.strike;
    results_.additionalResults["strikeSpread"] = strike;

    // Calculate the risky annuity
    Real rpv01 = std::abs(cds.couponLegNPV() + cds.accrualRebateNPV()) / (cds.notional() * cds.runningSpread());
    QL_REQUIRE(cds.notional() > 0.0 || close_enough(cds.notional(), 0.0),
               "BlackCdsOptionEngine: notional must not be negative (" << cds.notional() << ")");
    QL_REQUIRE(rpv01 > 0.0, "BlackCdsOptionEngine: risky annuity must be positive (couponLegNPV="
                                << cds.couponLegNPV() << ", accrualRebateNPV=" << cds.accrualRebateNPV()
                                << ", notional=" << cds.notional() << ", runningSpread=" << cds.runningSpread() << ")");

    results_.additionalResults["riskyAnnuity"] = rpv01;

    // Read the volatility from the volatility surface, assumed to have strike dimension in terms of spread.
    Real underlyingLength = volatility_->dayCounter().yearFraction(exerciseDate, cds.maturity());
    Real vol = volatility_->volatility(exerciseDate, underlyingLength, strike, CreditVolCurve::Type::Spread);
    Real stdDev = vol * std::sqrt(volatility_->timeFromReference(exerciseDate));
    results_.additionalResults["volatility"] = vol;
    results_.additionalResults["standardDeviation"] = stdDev;

    // Option type
    Option::Type callPut = cds.side() == Protection::Buyer ? Option::Call : Option::Put;
    results_.additionalResults["callPut"] = callPut == Option::Call ? string("Call") : string("Put");

    // Adjusted strike spread, akin to BlackIndexCdsOptionEngine::spreadStrikeCalculate
    Real runningSpread = cds.runningSpread();
    Real rpv01_K_fwd = QuantExt::forwardRiskyAnnuityStrike(discountSwapCurrency_, strike, exerciseDate, recovery_, cds);
    Real Kp = close_enough(strike, 0.0)
                  ? 0.0
                  : runningSpread + rpv01_K_fwd * (strike - runningSpread) *
                                        (cashSettled ? discSwapCurrToExercise : discTradeCollToExercise) / rpv01;

    // The strike spread might get negative through the adjustment above, but economically the strike is
    // floored at 0.0, so we ensure this here. This lets us compute the black formula as well in all cases.
    Kp = std::max(Kp, 0.0);

    results_.value = discTradeCollToExercise / (cashSettled ? discSwapCurrToExercise : discTradeCollToExercise) *
                     rpv01 * cds.notional() * blackFormula(callPut, Kp, forward, stdDev, 1.0);

    // If it is non-knockout and a payer, add the value of the default payout.
    // Section 2.2 of Richard J.Martin, 2019 or Section 9.3.8 O'Kane 2008.
    if (!arguments_.knocksOut && cds.side() == Protection::Buyer) {

        Probability sp = probability_->survivalProbability(exerciseDate);
        results_.additionalResults["survivalProbabilityToExercise"] = sp;

        Real nonKoPv = discTradeCollToExercise * (1.0 - sp) * (1.0 - recovery_) * cds.notional();
        results_.additionalResults["nonKnockoutPv"] = nonKoPv;

        results_.value += nonKoPv;
    }
}

}
