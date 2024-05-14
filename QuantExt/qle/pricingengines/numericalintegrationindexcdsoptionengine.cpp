/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/numericalintegrationindexcdsoptionengine.hpp>

#include <qle/utilities/time.hpp>

#include <ql/exercise.hpp>
#include <ql/math/integrals/simpsonintegral.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/actual360.hpp>

#include <boost/math/constants/constants.hpp>

#include <numeric>

using namespace QuantLib;

namespace QuantExt {

void NumericalIntegrationIndexCdsOptionEngine::doCalc() const {

    // checks

    QL_REQUIRE(indexRecovery_ != Null<Real>(),
               "NumericalIntegrationIndexCdsOptionEngine::doCalc(): index recovery is not given.");

    // set some variables for later use

    Date exerciseDate = arguments_.exercise->dates().front();
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);
    Real omega = arguments_.swap->side() == Protection::Buyer ? 1.0 : -1.0;
    Real discTradeCollToExercise = discountTradeCollateral_->discount(exerciseDate);
    Real discSwapCurrToExercise = discountSwapCurrency_->discount(exerciseDate);
    Real maturityTime = volatility_->timeFromReference(arguments_.swap->maturity());
    Real underlyingNpv =
        arguments_.swap->side() == Protection::Buyer ? arguments_.swap->NPV() : -arguments_.swap->NPV();

    results_.additionalResults["runningSpread"] = arguments_.swap->runningSpread();
    results_.additionalResults["discountToExerciseTradeCollateral"] = discTradeCollToExercise;
    results_.additionalResults["discountToExerciseSwapCurrency"] = discSwapCurrToExercise;
    results_.additionalResults["upfront"] =
        underlyingNpv *
        (arguments_.settlementType == Settlement::Cash ? discTradeCollToExercise / discSwapCurrToExercise : 1.0);
    results_.additionalResults["valuationDateNotional"] = arguments_.swap->notional();
    results_.additionalResults["tradeDateNotional"] = arguments_.tradeDateNtl;
    results_.additionalResults["callPut"] =
        arguments_.swap->side() == Protection::Buyer ? std::string("Call") : std::string("Put");

    // The model that we use is driven by the vol type of the market cds vol surface, i.e. either spread vol or price
    // vol We handle both spread or price strikes in both models.

    if (volatility_->type() == CreditVolCurve::Type::Price) {

        // 1 price vol type model

        results_.additionalResults["Model"] = std::string("LognormalPriceVolatility");

        // convert spread to strike if necessary

        Real strikePrice;
        if (arguments_.strikeType == CdsOption::StrikeType::Price) {
            // strike is expressed w.r.t. trade date notional
            strikePrice = 1.0 - arguments_.tradeDateNtl / arguments_.swap->notional() * (1.0 - arguments_.strike);
        } else {
            results_.additionalResults["strikeSpread"] = arguments_.strike;
            strikePrice = 1.0 + arguments_.tradeDateNtl / arguments_.swap->notional() *
                                    forwardRiskyAnnuityStrike(arguments_.strike) *
                                    (arguments_.swap->runningSpread() - arguments_.strike);
        }

        results_.additionalResults["strikePrice"] = strikePrice;

        // get volatility

        Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm),
                                                  strikePrice, CreditVolCurve::Type::Price);
        Real stdDev = volatility * std::sqrt(exerciseTime);
        results_.additionalResults["volatility"] = volatility;
        results_.additionalResults["standardDeviation"] = stdDev;

        // calculate the default-adjusted forward price

        Real forwardPriceExclFep = 1.0 - underlyingNpv / arguments_.swap->notional() /
                                             (Settlement::Cash ? discSwapCurrToExercise : discTradeCollToExercise);
        Real forwardPrice = forwardPriceExclFep - fep() / arguments_.swap->notional() / discTradeCollToExercise;

        results_.additionalResults["fepAdjustedForwardPrice"] = forwardPrice;
        results_.additionalResults["forwardPrice"] = forwardPriceExclFep;

        // Check the inputs to the black formula before applying it
        QL_REQUIRE(forwardPrice > 0.0 || close_enough(forwardPrice, 0.0),
                   "NumericalIntegrationIndexCdsOptionEngine: FEP adjusted forward price ("
                       << forwardPrice << ") is not positive, can not calculate a reasonable option price");
        QL_REQUIRE(strikePrice > 0 || close_enough(strikePrice, 0.0),
                   "NumericalIntegrationIndexCdsOptionEngine: Effective Strike price ("
                       << strikePrice << ") is not positive, can not calculate a reasonable option price");

        results_.value = arguments_.swap->notional() *
                         blackFormula(arguments_.swap->side() == Protection::Buyer ? Option::Put : Option::Call,
                                      strikePrice, forwardPrice, stdDev, discTradeCollToExercise);

    } else {

        // 2 spread vol type model

        results_.additionalResults["Model"] = std::string("LognormalSpreadVolatility");

        // compute average interest rate for underlying swap time interval

        auto tmpDisc = arguments_.settlementType == Settlement::Cash ? discountSwapCurrency_ : discountTradeCollateral_;
        Real averageInterestRate =
            -std::log(tmpDisc->discount(arguments_.swap->maturity()) / tmpDisc->discount(exerciseDate)) /
            (maturityTime - exerciseTime);

        // compute the strike adjustment, notice that the strike adjustment is scaled by trade date notional

        Real strikeAdjustment;
        if (arguments_.strikeType == CdsOption::StrikeType::Spread) {
            strikeAdjustment = arguments_.tradeDateNtl / arguments_.swap->notional() *
                               forwardRiskyAnnuityStrike(arguments_.strike) *
                               (arguments_.swap->runningSpread() - arguments_.strike);
        } else if (arguments_.strikeType == CdsOption::StrikeType::Price) {
            strikeAdjustment = arguments_.tradeDateNtl / arguments_.swap->notional() * (arguments_.strike - 1.0);
        }
        results_.additionalResults["strikeAdjustment"] = strikeAdjustment;

        // back out spread strike from strike adjustment if necessary

        Real strikeSpread;
        if (arguments_.strikeType == CdsOption::StrikeType::Spread &&
            QuantLib::close_enough(arguments_.tradeDateNtl, arguments_.swap->notional())) {
            strikeSpread = arguments_.strike;
        } else {
            Brent brent;
            brent.setLowerBound(1.0E-8);
            auto strikeTarget = [this, strikeAdjustment](Real strikeSpread) {
                return forwardRiskyAnnuityStrike(strikeSpread) * (arguments_.swap->runningSpread() - strikeSpread) -
                       strikeAdjustment;
            };
            try {
                strikeSpread = brent.solve(strikeTarget, 1.0E-7, arguments_.swap->fairSpreadClean(), 0.0001);
                // eval function at solution to make sure, add results are set correctly
                forwardRiskyAnnuityStrike(strikeSpread);
            } catch (const std::exception& e) {
                QL_FAIL("NumericalIntegrationIndexCdsOptionEngine: can not compute strike spread: "
                        << e.what() << ", strikeAdjustment=" << strikeAdjustment << ", trade strike "
                        << arguments_.strike << ", trade strike type "
                        << (arguments_.strikeType == CdsOption::StrikeType::Spread ? "Spread" : "Price"));
            }
        }

        if (arguments_.strikeType == CdsOption::StrikeType::Price)
            results_.additionalResults["strikePrice"] = arguments_.strike;

        results_.additionalResults["strikeSpread"] = strikeSpread;

        // get volatility

        Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm),
                                                  strikeSpread, CreditVolCurve::Type::Spread);
        Real stdDev = volatility * std::sqrt(exerciseTime);
        results_.additionalResults["volatility"] = volatility;
        results_.additionalResults["standardDeviation"] = stdDev;

        // calculate the default-adjusted forward price

        Real forwardPriceExclFep = 1.0 -
                            underlyingNpv / arguments_.swap->notional() /
                                (Settlement::Cash ? discSwapCurrToExercise : discTradeCollToExercise);
        Real forwardPrice = forwardPriceExclFep -
                            fep() / arguments_.swap->notional() / discTradeCollToExercise;

        results_.additionalResults["fepAdjustedForwardPrice"] = forwardPrice;
        results_.additionalResults["forwardPrice"] = forwardPriceExclFep;

        // the default-adjusted index value Vc using a continuous annuity

        auto Vc = [](Real t, Real T, Real r, Real R, Real c, Real stdDev, Real m, Real x) {
            Real s = m * std::exp(-0.5 * stdDev * stdDev + stdDev * x);
            Real w = (s / (1.0 - R) + r) * (T - t);
            Real a = (T - t);
            if (std::abs(w) < 1.0E-6)
                a *= 1.0 - 0.5 * w + 1.0 / 6.0 * w * w - 1.0 / 24.0 * w * w * w;
            else
                a *= (1.0 - std::exp(-w)) / w;
            return (s - c) * a;
        };

        // calibrate the default-adjusted forward spread m to the forward price

        SimpsonIntegral simpson = SimpsonIntegral(1.0E-7, 100);

        auto target = [this, &Vc, &simpson, exerciseTime, maturityTime, averageInterestRate, stdDev,
                       forwardPrice](Real m) {
            return simpson(
                       [this, &Vc, exerciseTime, maturityTime, averageInterestRate, stdDev, m](Real x) {
                           return Vc(exerciseTime, maturityTime, averageInterestRate, indexRecovery_,
                                     arguments_.swap->runningSpread(), stdDev, m, x) *
                                  std::exp(-0.5 * x * x) / boost::math::constants::root_two_pi<Real>();
                       },
                       -10.0, 10.0) -
                   (1.0 - forwardPrice);
        };

        Real fepAdjustedForwardSpread;
        if (target(0.0) > 0.0) {
            // the target function might not have a zero, because of the continuous annuity approximation
            // in some extreme situations (e.g. survival prob = 1 everywhere)
            fepAdjustedForwardSpread = 0.0;
        } else {
            Brent brent;
            brent.setLowerBound(0.0);
            try {
                fepAdjustedForwardSpread = brent.solve(target, 1.0E-7, arguments_.swap->fairSpreadClean(), 0.0001);
            } catch (const std::exception& e) {
                QL_FAIL("NumericalIntegrationIndexCdsOptionEngine::doCalc(): failed to calibrate forward spread: "
                        << e.what());
            }
        }
        results_.additionalResults["fepAdjustedForwardSpread"] = fepAdjustedForwardSpread;
        results_.additionalResults["forwardSpread"] = arguments_.swap->fairSpreadClean();

        // find the exercise boundary

        auto payoff = [this, &Vc, exerciseTime, maturityTime, averageInterestRate, stdDev, fepAdjustedForwardSpread,
                       strikeAdjustment](Real x) {
            return (Vc(exerciseTime, maturityTime, averageInterestRate, indexRecovery_,
                       arguments_.swap->runningSpread(), stdDev, fepAdjustedForwardSpread, x) +
                    strikeAdjustment + arguments_.realisedFep / arguments_.swap->notional()) *
                   std::exp(-0.5 * x * x) / boost::math::constants::root_two_pi<Real>();
        };

        Real exerciseBoundary;
        Brent brent2;
        try {
            exerciseBoundary = brent2.solve(payoff, 1.0E-7, 0.0, 0.0001);
        } catch (const std::exception& e) {
            QL_FAIL(
                "NumericalIntegrationIndexCdsOptionEngine::doCalc(): failed to find exercise boundary: " << e.what());
        }
        results_.additionalResults["exerciseBoundary"] =
            fepAdjustedForwardSpread * std::exp(-0.5 * stdDev * stdDev + stdDev * exerciseBoundary);

        // compute the option value

        Real lowerIntegrationBound = -10.0, upperIntegrationBound = 10.0;
        if (arguments_.swap->side() == Protection::Buyer) {
            lowerIntegrationBound = exerciseBoundary;
        } else {
            upperIntegrationBound = exerciseBoundary;
        }

        try {
            results_.value = arguments_.swap->notional() * discTradeCollToExercise *
                             simpson(
                                 [exerciseTime, maturityTime, averageInterestRate, this, stdDev,
                                  fepAdjustedForwardSpread, omega, strikeAdjustment, &Vc](Real x) {
                                     return omega *
                                            (Vc(exerciseTime, maturityTime, averageInterestRate, indexRecovery_,
                                                arguments_.swap->runningSpread(), stdDev, fepAdjustedForwardSpread, x) +
                                             strikeAdjustment + arguments_.realisedFep / arguments_.swap->notional()) *
                                            std::exp(-0.5 * x * x) / boost::math::constants::root_two_pi<Real>();
                                 },
                                 lowerIntegrationBound, upperIntegrationBound);
        } catch (const std::exception& e) {
            QL_FAIL(
                "NumericalIntegrationIndexCdsOptionEngine::doCalc(): failed to compute option payoff: " << e.what());
        }

    } // handle 2 spread vol model type

} // doCalc();

Real NumericalIntegrationIndexCdsOptionEngine::forwardRiskyAnnuityStrike(const Real strike) const {

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
    Real accuracy = 1e-8;

    auto strikeCds = QuantLib::ext::make_shared<CreditDefaultSwap>(
        Protection::Buyer, 1 / accuracy, strike, schedule, Following, Actual360(), cds.settlesAccrual(),
        cds.protectionPaymentTime(), cds.protectionStartDate(), QuantLib::ext::shared_ptr<Claim>(), Actual360(true), true,
        cds.tradeDate(), cds.cashSettlementDays());
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

    // Forward risky annuity strike
    Real rpv01_K_fwd = rpv01_K / spToExercise / discToExercise;
    results_.additionalResults["forwardRiskyAnnuityStrike"] = rpv01_K_fwd;

    return rpv01_K_fwd;
}

} // namespace QuantExt
