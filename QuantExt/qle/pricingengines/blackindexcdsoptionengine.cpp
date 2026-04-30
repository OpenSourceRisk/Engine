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
#include <ql/pricingengines/credit/isdacdsengine.hpp>
#include <qle/pricingengines/blackindexcdsoptionengine.hpp>
#include <qle/pricingengines/forwardriskyannuitystrike.hpp>
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
        priceStrikeCalculate();
}

void BlackIndexCdsOptionEngine::spreadStrikeCalculate(Real fep) const {

    const Date& exerciseDate = arguments_.exercise->dates().front();
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);
    const auto& cds = *arguments_.swap;
    const Real& strike = arguments_.strike;
    Real runningSpread = cds.runningSpread();
    DiscountFactor discTradeCollToExercise = discountTradeCollateral_->discount(exerciseDate);
    DiscountFactor discSwapCurrToExercise = discountSwapCurrency_->discount(exerciseDate);
    bool cashSettled = arguments_.settlementType == Settlement::Cash;

    // Calculate the risky annuity
    Real rpv01 = std::abs(cds.couponLegNPV() + cds.accrualRebateNPV()) / (cds.notional() * cds.runningSpread());
    QL_REQUIRE(cds.notional() > 0.0 || close_enough(cds.notional(), 0.0),
               "BlackIndexCdsOptionEngine: notional must not be negative (" << cds.notional() << ")");
    QL_REQUIRE(rpv01 > 0.0, "BlackIndexCdsOptionEngine: risky annuity must be positive (couponLegNPV="
                                << cds.couponLegNPV() << ", accrualRebateNPV=" << cds.accrualRebateNPV()
                                << ", notional=" << cds.notional() << ", runningSpread=" << cds.runningSpread() << ")");

    Real fairSpread = cds.fairSpreadClean();

    // FEP adjusted forward spread. F^{Adjusted} in O'Kane 2008, Section 11.7. F' in ICE paper (notation is poor).
    Real Fp = fairSpread + fep * (cashSettled ? discSwapCurrToExercise : discTradeCollToExercise) / rpv01 /
                               discTradeCollToExercise / cds.notional();

    // Adjusted strike spread. K' in O'Kane 2008, Section 11.7. K' in ICE paper (notation is poor).
    Real rpv01_K_fwd = QuantExt::forwardRiskyAnnuityStrike(discountSwapCurrency_, strike, exerciseDate, indexRecovery_, cds);

    Real Kp = close_enough(strike, 0.0)
                  ? 0.0
                  : runningSpread + arguments_.tradeDateNtl / cds.notional() * rpv01_K_fwd *
                                        (strike - runningSpread) *
                                        (cashSettled ? discSwapCurrToExercise : discTradeCollToExercise) / rpv01;

    // Read the volatility from the volatility surface
    Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm), strike,
                                              CreditVolCurve::Type::Spread);
    Real stdDev = volatility * std::sqrt(exerciseTime);

    // Option type
    Option::Type callPut = cds.side() == Protection::Buyer ? Option::Call : Option::Put;
    results_.additionalResults["callPut"] = callPut == Option::Call ? string("Call") : string("Put");

    // Check the forward before plugging it into the black formula
    QL_REQUIRE(Fp > 0.0 || close_enough(stdDev, 0.0),
               "BlackIndexCdsOptionEngine: FEP adjusted forward spread ("
                   << Fp << ") is not positive, can not calculate a reasonable option price");
    // The strike spread might get negative through the adjustment above, but economically the strike is
    // floored at 0.0, so we ensure this here. This lets us compute the black formula as well in all cases.
    Kp = std::max(Kp, 0.0);

    results_.value = discTradeCollToExercise / (cashSettled ? discSwapCurrToExercise : discTradeCollToExercise) *
                     rpv01 * cds.notional() * blackFormula(callPut, Kp, Fp, stdDev, 1.0);

    if (generateAdditionalResults_) {
        results_.additionalResults["Model"] = std::string("LognormalStrikeVolatility");
        results_.additionalResults["strikeSpread"] = strike;
        results_.additionalResults["riskyAnnuity"] = rpv01;
        results_.additionalResults["adjustedStrikeSpread"] = Kp;
        results_.additionalResults["runningSpread"] = runningSpread;
        results_.additionalResults["forwardSpread"] = fairSpread;
        results_.additionalResults["fepAdjustedForwardSpread"] = Fp;
        results_.additionalResults["discountToExerciseTradeCollateral"] = discTradeCollToExercise;
        results_.additionalResults["discountToExerciseSwapCurrency"] = discSwapCurrToExercise;
        results_.additionalResults["volatility"] = volatility;
        results_.additionalResults["standardDeviation"] = stdDev;
        results_.additionalResults["valuationDateNotional"] = cds.notional();
        results_.additionalResults["tradeDateNotional"] = arguments_.tradeDateNtl;
        results_.additionalResults["forwardRiskyAnnuityStrike"] = rpv01_K_fwd;
    }
}

void BlackIndexCdsOptionEngine::priceStrikeCalculate() const {

    // Underlying index CDS.
    const auto& cds = *arguments_.swap;

    const Real& tradeDateNtl = arguments_.tradeDateNtl;
    bool cashSettled = arguments_.settlementType == Settlement::Cash;

    // Discount factor to exercise
    const Date& exerciseDate = arguments_.exercise->dates().front();
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);
    DiscountFactor discTradeCollToExercise = discountTradeCollateral_->discount(exerciseDate);
    DiscountFactor discSwapCurrToExercise = discountSwapCurrency_->discount(exerciseDate);

    // effective strike (strike is expressed w.r.t. trade date notional by market convention)
    Real rFep = realizedFep() / discTradeCollToExercise;
    Real effStrike = 1.0 - (tradeDateNtl * (1.0 - arguments_.strike) - rFep) / cds.notional();

    // NPV from buyer's perspective gives upfront, as of valuation date, with correct sign.
    Real npv = cds.side() == Protection::Buyer ? cds.NPV() : -cds.NPV();

    Real forwardPrice =
        1 - npv / cds.notional() / (cashSettled ? discSwapCurrToExercise : discTradeCollToExercise);

    // Front end protection adjusted forward price.
    Real urFep = unrealizedFep() / discTradeCollToExercise;
    Real Fp = forwardPrice - urFep / cds.notional();

    // Read the volatility from the volatility surface. Stripped surfaces and those from provider supply volatilities 
    // for old series versions on the original strike. So, use original strike here rather than effective strike. If we 
    // move to using the on-the-run series verion volatility surface to price older series versions, we should switch 
    // to using effective strike here as well.
    Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm),
        arguments_.strike, CreditVolCurve::Type::Price);
    Real stdDev = volatility * std::sqrt(exerciseTime);

    // If protection buyer, put on price.
    Option::Type cp = cds.side() == Protection::Buyer ? Option::Put : Option::Call;

    // Check the inputs to the black formula before applying it
    QL_REQUIRE(Fp > 0.0 || close_enough(stdDev, 0.0),
               "BlackIndexCdsOptionEngine: FEP adjusted forward price ("
                   << Fp << ") is not positive, can not calculate a reasonable option price");
    QL_REQUIRE(effStrike > 0 || close_enough(effStrike, 0.0),
               "BlackIndexCdsOptionEngine: Effective Strike price ("
                   << effStrike << ") is not positive, can not calculate a reasonable option price");

    results_.value = cds.notional() * blackFormula(cp, effStrike, Fp, stdDev, discTradeCollToExercise);

    if (generateAdditionalResults_) {
        results_.additionalResults["Model"] = std::string("LognormalPriceVolatility");
        results_.additionalResults["valuationDateNotional"] = cds.notional();
        results_.additionalResults["tradeDateNotional"] = tradeDateNtl;
        results_.additionalResults["strikePrice"] = arguments_.strike;
        results_.additionalResults["strikePriceDefaultAdjusted"] = effStrike;
        results_.additionalResults["discountToExerciseTradeCollateral"] = discTradeCollToExercise;
        results_.additionalResults["discountToExerciseSwapCurrency"] = discSwapCurrToExercise;
        results_.additionalResults["upfront"] =
            npv * (cashSettled ? discTradeCollToExercise / discSwapCurrToExercise : 1.0);
        results_.additionalResults["forwardPrice"] = forwardPrice;
        results_.additionalResults["fepAdjustedForwardPrice"] = Fp;
        results_.additionalResults["volatility"] = volatility;
        results_.additionalResults["standardDeviation"] = stdDev;
        results_.additionalResults["callPut"] = cp == Option::Put ? string("Put") : string("Call");
    }
}

} // namespace QuantExt
