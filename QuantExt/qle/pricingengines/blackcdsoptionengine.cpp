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

#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <qle/pricingengines/blackcdsoptionengine.hpp>

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

    // Adjusted strike spread. K' in O'Kane 2008, Section 11.7.
    Real runningSpread = cds.runningSpread();
    Real Kp = close_enough(strike, 0.0)
                  ? 0.0
                  : runningSpread + forwardRiskyAnnuityStrike() * (strike - runningSpread) / rpv01;

    // NPV, Section 9.3.7 O'Kane 2008.
    results_.value = rpv01 * cds.notional() * blackFormula(callPut, Kp, forward, stdDev, 1.0);

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

Real BlackCdsOptionEngine::forwardRiskyAnnuityStrike() const {

    // Underlying  CDS.
    const auto& cds = *arguments_.swap;

    // This method returns RPV01(0; t_e, T, K) / SP(t_e; K). This is the quantity in formula 11.9 of O'Kane 2008.
    // There is a slight modification in that we divide by the survival probability to t_E using the flat curve at
    // the strike spread that we create here.

    // Standard  CDS schedule.
    Schedule schedule = MakeSchedule()
                            .from(cds.protectionStartDate())
                            .to(cds.maturity())
                            .withCalendar(WeekendsOnly())
                            .withFrequency(Quarterly)
                            .withConvention(Following)
                            .withTerminationDateConvention(Unadjusted)
                            .withRule(DateGeneration::CDS2015);

    // Derive hazard rate curve from a single forward starting CDS matching the characteristics of underlying
    // CDS with a running spread equal to the strike.
    const Real& strike = arguments_.strike;
    Real accuracy = 1e-8;

    auto strikeCds = QuantLib::ext::make_shared<CreditDefaultSwap>(
        Protection::Buyer, 1 / accuracy, strike, schedule, Following, Actual360(), cds.settlesAccrual(),
        cds.protectionPaymentTime(), cds.protectionStartDate(), QuantLib::ext::shared_ptr<Claim>(), Actual360(true),
        true, cds.tradeDate(), cds.cashSettlementDays());
    // dummy engine
    strikeCds->setPricingEngine(QuantLib::ext::make_shared<MidPointCdsEngine>(
        Handle<DefaultProbabilityTermStructure>(
            QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0, Actual365Fixed())),
        0.0,
        Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed()))));

    Real hazardRate;
    try {
        hazardRate = strikeCds->impliedHazardRate(0.0, discount_, Actual365Fixed(), recovery_, accuracy);
    } catch (const std::exception& e) {
        QL_FAIL("can not imply fair hazard rate for CDS at option strike "
                << strike << ". Is the strike correct? Exception: " << e.what());
    }

    Handle<DefaultProbabilityTermStructure> dph(
        QuantLib::ext::make_shared<FlatHazardRate>(discount_->referenceDate(), hazardRate, Actual365Fixed()));

    // Calculate the forward risky strike annuity.
    strikeCds->setPricingEngine(QuantLib::ext::make_shared<QuantExt::MidPointCdsEngine>(dph, recovery_, discount_));
    Real rpv01_K = std::abs(strikeCds->couponLegNPV() + strikeCds->accrualRebateNPV()) /
                   (strikeCds->notional() * strikeCds->runningSpread());
    QL_REQUIRE(rpv01_K > 0.0, "BlackCdsOptionEngine: strike based risky annuity must be positive.");

    // Survival to exercise
    const Date& exerciseDate = arguments_.exercise->dates().front();
    Probability spToExercise = dph->survivalProbability(exerciseDate);
    // Real discToExercise = discount_->discount(exerciseDate);

    // Forward risky annuity strike (divides out the survival probability and discount to exercise)
    Real rpv01_K_fwd = rpv01_K / spToExercise; // / discToExercise;

    // if (generateAdditionalResults_) {
    //     results_.additionalResults["riskyAnnuityStrike"] = rpv01_K;
    //     results_.additionalResults["strikeBasedSurvivalToExercise"] = spToExercise;
    //     results_.additionalResults["forwardRiskyAnnuityStrike"] = rpv01_K_fwd;
    // }

    return rpv01_K_fwd;
}
}
