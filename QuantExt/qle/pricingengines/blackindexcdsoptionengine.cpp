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

#include <qle/pricingengines/blackindexcdsoptionengine.hpp>

#include <ql/exercise.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/credit/isdacdsengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <qle/utilities/time.hpp>

#include <numeric>

using namespace QuantLib;
using std::string;
using std::vector;

namespace QuantExt {

void BlackIndexCdsOptionEngine::doCalc() const {
    // Calculate option value depending on strike type.
    if (arguments_.strikeType == CdsOption::Spread)
        spreadStrikeCalculate(fep());
    else
        priceStrikeCalculate(fep());
}

void BlackIndexCdsOptionEngine::spreadStrikeCalculate(Real fep) const {

    const Date& exerciseDate = arguments_.exercise->dates().front();
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);
    const auto& cds = *arguments_.swap;
    const Real& strike = arguments_.strike;
    results_.additionalResults["strikeSpread"] = strike;
    Real runningSpread = cds.runningSpread();
    results_.additionalResults["runningSpread"] = runningSpread;

    DiscountFactor discTradeCollToExercise = discountTradeCollateral_->discount(exerciseDate);
    DiscountFactor discSwapCurrToExercise = discountSwapCurrency_->discount(exerciseDate);
    results_.additionalResults["discountToExerciseTradeCollateral"] = discTradeCollToExercise;
    results_.additionalResults["discountToExerciseSwapCurrency"] = discSwapCurrToExercise;

    // Calculate the risky annuity
    Real rpv01 = std::abs(cds.couponLegNPV() + cds.accrualRebateNPV()) / (cds.notional() * cds.runningSpread());
    results_.additionalResults["riskyAnnuity"] = rpv01;
    QL_REQUIRE(cds.notional() > 0.0 || close_enough(cds.notional(), 0.0),
               "BlackIndexCdsOptionEngine: notional must not be negative (" << cds.notional() << ")");
    QL_REQUIRE(rpv01 > 0.0, "BlackIndexCdsOptionEngine: risky annuity must be positive (couponLegNPV="
                                << cds.couponLegNPV() << ", accrualRebateNPV=" << cds.accrualRebateNPV()
                                << ", notional=" << cds.notional() << ", runningSpread=" << cds.runningSpread() << ")");

    Real fairSpread = cds.fairSpreadClean();
    results_.additionalResults["forwardSpread"] = fairSpread;

    // FEP adjusted forward spread. F^{Adjusted} in O'Kane 2008, Section 11.7. F' in ICE paper (notation is poor).
    Real Fp = fairSpread + fep * (Settlement::Cash ? discSwapCurrToExercise : discTradeCollToExercise) / rpv01 /
                               discTradeCollToExercise / cds.notional();
    results_.additionalResults["fepAdjustedForwardSpread"] = Fp;

    // Adjusted strike spread. K' in O'Kane 2008, Section 11.7. K' in ICE paper (notation is poor).
    Real Kp = close_enough(strike, 0.0)
                  ? 0.0
                  : runningSpread + arguments_.tradeDateNtl / cds.notional() * forwardRiskyAnnuityStrike() *
                                        (strike - runningSpread) *
                                        (Settlement::Cash ? discSwapCurrToExercise : discTradeCollToExercise) / rpv01;
    results_.additionalResults["adjustedStrikeSpread"] = Kp;

    // Read the volatility from the volatility surface
    Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm), strike,
                                              CreditVolCurve::Type::Spread);
    Real stdDev = volatility * std::sqrt(exerciseTime);
    results_.additionalResults["volatility"] = volatility;
    results_.additionalResults["standardDeviation"] = stdDev;

    // Option type
    Option::Type callPut = cds.side() == Protection::Buyer ? Option::Call : Option::Put;
    results_.additionalResults["callPut"] = callPut == Option::Call ? string("Call") : string("Put");

    // NPV. Add the relevant notionals to the additional results also.
    results_.additionalResults["valuationDateNotional"] = cds.notional();
    results_.additionalResults["tradeDateNotional"] = arguments_.tradeDateNtl;

    // Check the forward before plugging it into the black formula
    QL_REQUIRE(Fp > 0.0 || close_enough(stdDev, 0.0),
               "BlackIndexCdsOptionEngine: FEP adjusted forward spread ("
                   << Fp << ") is not positive, can not calculate a reasonable option price");
    // The strike spread might get negative through the adjustment above, but economically the strike is
    // floored at 0.0, so we ensure this here. This lets us compute the black formula as well in all cases.
    Kp = std::max(Kp, 0.0);

    results_.value = discTradeCollToExercise / (Settlement::Cash ? discSwapCurrToExercise : discTradeCollToExercise) *
                     rpv01 * cds.notional() * blackFormula(callPut, Kp, Fp, stdDev, 1.0);
}

void BlackIndexCdsOptionEngine::priceStrikeCalculate(Real fep) const {

    // Underlying index CDS.
    const auto& cds = *arguments_.swap;

    // Add some additional entries to additional results.
    results_.additionalResults["strikePrice"] = arguments_.strike;

    const Real& tradeDateNtl = arguments_.tradeDateNtl;
    results_.additionalResults["valuationDateNotional"] = cds.notional();
    results_.additionalResults["tradeDateNotional"] = tradeDateNtl;

    // effective strike (strike is expressed w.r.t. trade date notional by market convention)
    Real effStrike = 1.0 - tradeDateNtl / cds.notional() * (1.0 - arguments_.strike);
    results_.additionalResults["strikePriceDefaultAdjusted"] = effStrike;

    // Discount factor to exercise
    const Date& exerciseDate = arguments_.exercise->dates().front();
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);
    DiscountFactor discTradeCollToExercise = discountTradeCollateral_->discount(exerciseDate);
    DiscountFactor discSwapCurrToExercise = discountSwapCurrency_->discount(exerciseDate);
    results_.additionalResults["discountToExerciseTradeCollateral"] = discTradeCollToExercise;
    results_.additionalResults["discountToExerciseSwapCurrency"] = discSwapCurrToExercise;

    // NPV from buyer's perspective gives upfront, as of valuation date, with correct sign.
    Real npv = cds.side() == Protection::Buyer ? cds.NPV() : -cds.NPV();
    results_.additionalResults["upfront"] =
        npv * (arguments_.settlementType == Settlement::Cash ? discTradeCollToExercise / discSwapCurrToExercise : 1.0);

    Real forwardPrice =
        1 - npv / cds.notional() / (Settlement::Cash ? discSwapCurrToExercise : discTradeCollToExercise);
    results_.additionalResults["forwardPrice"] = forwardPrice;

    // Front end protection adjusted forward price.
    Real Fp = forwardPrice - fep / cds.notional() / discTradeCollToExercise;
    results_.additionalResults["fepAdjustedForwardPrice"] = Fp;

    // Read the volatility from the volatility surface
    Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm), effStrike,
                                              CreditVolCurve::Type::Price);
    Real stdDev = volatility * std::sqrt(exerciseTime);
    results_.additionalResults["volatility"] = volatility;
    results_.additionalResults["standardDeviation"] = stdDev;

    // If protection buyer, put on price.
    Option::Type cp = cds.side() == Protection::Buyer ? Option::Put : Option::Call;
    results_.additionalResults["callPut"] = cp == Option::Put ? string("Put") : string("Call");

    // Check the inputs to the black formula before applying it
    QL_REQUIRE(Fp > 0.0 || close_enough(stdDev, 0.0),
               "BlackIndexCdsOptionEngine: FEP adjusted forward price ("
                   << Fp << ") is not positive, can not calculate a reasonable option price");
    QL_REQUIRE(effStrike > 0 || close_enough(effStrike, 0.0),
               "BlackIndexCdsOptionEngine: Effective Strike price ("
                   << effStrike << ") is not positive, can not calculate a reasonable option price");

    results_.value = cds.notional() * blackFormula(cp, effStrike, Fp, stdDev, discTradeCollToExercise);
}

Real BlackIndexCdsOptionEngine::forwardRiskyAnnuityStrike() const {

    // Underlying index CDS.
    const auto& cds = *arguments_.swap;

    // This method returns RPV01(0; t_e, T, K) / SP(t_e; K). This is the quantity in formula 11.9 of O'Kane 2008.
    // There is a slight modification in that we divide by the survival probability to t_E using the flat curve at
    // the strike spread that we create here.

    // Standard index CDS schedule.
    Schedule schedule = MakeSchedule()
                            .from(cds.protectionStartDate())
                            .to(cds.maturity())
                            .withCalendar(WeekendsOnly())
                            .withFrequency(Quarterly)
                            .withConvention(Following)
                            .withTerminationDateConvention(Unadjusted)
                            .withRule(DateGeneration::CDS2015);

    // Derive hazard rate curve from a single forward starting CDS matching the characteristics of underlying index
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
        0.0, Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.0, Actual365Fixed()))));

    Real hazardRate;
    try {
        hazardRate =
            strikeCds->impliedHazardRate(0.0, discountSwapCurrency_, Actual365Fixed(), indexRecovery_, accuracy);
    } catch (const std::exception& e) {
        QL_FAIL("can not imply fair hazard rate for CDS at option strike "
                << strike << ". Is the strike correct? Exception: " << e.what());
    }

    Handle<DefaultProbabilityTermStructure> dph(
        QuantLib::ext::make_shared<FlatHazardRate>(discountSwapCurrency_->referenceDate(), hazardRate, Actual365Fixed()));

    // Calculate the forward risky strike annuity.
    strikeCds->setPricingEngine(
        QuantLib::ext::make_shared<QuantExt::MidPointCdsEngine>(dph, indexRecovery_, discountSwapCurrency_));
    Real rpv01_K = std::abs(strikeCds->couponLegNPV() + strikeCds->accrualRebateNPV()) /
                   (strikeCds->notional() * strikeCds->runningSpread());
    results_.additionalResults["riskyAnnuityStrike"] = rpv01_K;
    QL_REQUIRE(rpv01_K > 0.0, "BlackIndexCdsOptionEngine: strike based risky annuity must be positive.");

    // Survival to exercise
    const Date& exerciseDate = arguments_.exercise->dates().front();
    Probability spToExercise = dph->survivalProbability(exerciseDate);
    Real discToExercise = discountSwapCurrency_->discount(exerciseDate);
    results_.additionalResults["strikeBasedSurvivalToExercise"] = spToExercise;

    // Forward risky annuity strike (divides out the survival probability and discount to exercise)
    Real rpv01_K_fwd = rpv01_K / spToExercise / discToExercise;
    results_.additionalResults["forwardRiskyAnnuityStrike"] = rpv01_K_fwd;

    return rpv01_K_fwd;
}

} // namespace QuantExt
