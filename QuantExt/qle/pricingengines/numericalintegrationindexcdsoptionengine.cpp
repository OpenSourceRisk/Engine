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

    // The model that we use is driven by the vol type of the market cds vol surface:
    // 1) volType = Spread:
    //
    // 2) volType = Price:
    //

    QL_REQUIRE(volatility_->type() == CreditVolCurve::Type::Spread,
               "NumericalIntegrationIndexCdsOptionEngine::doCalc(): implemented for spread vols only at the moment!");

    // checks

    QL_REQUIRE(indexRecovery_ != Null<Real>(),
               "NumericalIntegrationIndexCdsOptionEngine::doCalc(): index recovery is not given.");

    // set some variables for later use

    Date exerciseDate = arguments_.exercise->dates().front();
    Real exerciseTime = volatility_->timeFromReference(exerciseDate);
    Real volatility = volatility_->volatility(exerciseDate, QuantExt::periodToTime(arguments_.indexTerm),
                                              arguments_.strike, CreditVolCurve::Type::Spread);
    Real stdDev = volatility * std::sqrt(exerciseTime);
    Real omega = arguments_.swap->side() == Protection::Buyer ? 1.0 : -1.0;
    Real discToExercise = discount_->discount(exerciseDate);
    Real maturityTime = volatility_->timeFromReference(arguments_.swap->maturity());
    Real averageInterestRate =
        -std::log(discount_->discount(arguments_.swap->maturity()) / discount_->discount(exerciseDate)) /
        (maturityTime - exerciseTime);
    Real underlyingNpv =
        arguments_.swap->side() == Protection::Buyer ? arguments_.swap->NPV() : -arguments_.swap->NPV();

    results_.additionalResults["runningSpread"] = arguments_.swap->runningSpread();
    results_.additionalResults["discountToExercise"] = discToExercise;
    results_.additionalResults["volatility"] = volatility;
    results_.additionalResults["standardDeviation"] = stdDev;
    results_.additionalResults["upfront"] = underlyingNpv;
    results_.additionalResults["valuationDateNotional"] = arguments_.swap->notional();
    results_.additionalResults["tradeDateNotional"] = arguments_.tradeDateNtl;
    results_.additionalResults["strikeSpread"] = arguments_.strike;
    results_.additionalResults["callPut"] =
        arguments_.swap->side() == Protection::Buyer ? std::string("Call") : std::string("Put");

    // calculate the default-adjusted forward price

    Real forwardPrice = 1.0 - (underlyingNpv + fep()) / arguments_.swap->notional() / discToExercise;

    results_.additionalResults["fepAdjustedForwardPrice"] = forwardPrice;
    results_.additionalResults["forwardPrice"] = 1.0 - underlyingNpv / arguments_.swap->notional() / discToExercise;

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

    auto target = [this, &Vc, &simpson, exerciseTime, maturityTime, averageInterestRate, stdDev, forwardPrice](Real m) {
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
    Brent brent;
    brent.setLowerBound(0.0);
    try {
        fepAdjustedForwardSpread = brent.solve(target, 1.0E-7, arguments_.swap->fairSpreadClean(), 0.01);
    } catch (const std::exception e) {
        QL_FAIL("NumericalIntegrationIndexCdsOptionEngine::doCalc(): failed to calibrate forward spread: " << e.what());
    }
    results_.additionalResults["fepAdjustedForwardSpread"] = fepAdjustedForwardSpread;
    results_.additionalResults["forwardSpread"] = arguments_.swap->fairSpreadClean();

    // compute the strike adjustment, notice that the strike adjustment is scaled by trade date notional

    Real strikeAdjustment = arguments_.tradeDateNtl / arguments_.swap->notional() * forwardRiskyAnnuityStrike() *
                            (arguments_.swap->runningSpread() - arguments_.strike);

    results_.additionalResults["strikeAdjustment"] = strikeAdjustment;

    // find the exercise boundary

    auto payoff = [this, &Vc, exerciseTime, maturityTime, averageInterestRate, stdDev, fepAdjustedForwardSpread,
                   strikeAdjustment](Real x) {
        return (Vc(exerciseTime, maturityTime, averageInterestRate, indexRecovery_, arguments_.swap->runningSpread(),
                   stdDev, fepAdjustedForwardSpread, x) +
                strikeAdjustment + arguments_.realisedFep / arguments_.swap->notional()) *
               std::exp(-0.5 * x * x) / boost::math::constants::root_two_pi<Real>();
    };

    Real exerciseBoundary;
    Brent brent2;
    try {
        exerciseBoundary = brent2.solve(payoff, 1.0E-7, 0.0, 0.01);
    } catch (const std::exception e) {
        QL_FAIL("NumericalIntegrationIndexCdsOptionEngine::doCalc(): failed to find exercise boundary: " << e.what());
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
        results_.value = arguments_.swap->notional() * discToExercise *
                         simpson(
                             [exerciseTime, maturityTime, averageInterestRate, this, stdDev, fepAdjustedForwardSpread,
                              omega, strikeAdjustment, &Vc](Real x) {
                                 return omega *
                                        (Vc(exerciseTime, maturityTime, averageInterestRate, indexRecovery_,
                                            arguments_.swap->runningSpread(), stdDev, fepAdjustedForwardSpread, x) +
                                         strikeAdjustment + arguments_.realisedFep / arguments_.swap->notional()) *
                                        std::exp(-0.5 * x * x) / boost::math::constants::root_two_pi<Real>();
                             },
                             lowerIntegrationBound, upperIntegrationBound);
    } catch (const std::exception e) {
        QL_FAIL("NumericalIntegrationIndexCdsOptionEngine::doCalc(): failed to compute option payoff: " << e.what());
    }

} // doCalc();

} // namespace QuantExt
