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

    // calculate the default-adjusted forward price

    Real forwardPrice = 1.0 - (underlyingNpv - fep()) / arguments_.swap->notional() / discToExercise;

    results_.additionalResults["fepAdjustedForwardPrice"] = forwardPrice;
    results_.additionalResults["forwardPrice"] = 1.0 - underlyingNpv / arguments_.swap->notional() / discToExercise;

    // the default-adjusted index value Vc using a continuous annuity

    std::function<Real(Real, Real, Real, Real, Real, Real, Real, Real)> Vc = [](Real t, Real T, Real r, Real R, Real c,
                                                                                Real stdDev, Real m, Real x) {
        Real s = m * std::exp(-0.5 * stdDev * stdDev + stdDev * x);
        Real w = (s / (1.0 - R) + r) * (T - t);
        Real a = (T - t);
        if (std::abs(w) < 1.0E-6)
            a *= 1.0 - 0.5 * w + 1.0 / 6.0 * w * w - 1.0 / 24.0 * w * w * w;
        else
            a *= (1.0 - std::exp(w)) / w;
        return (s - c) * a;
    };

    // calibrate the default-adjusted forward spread m to the forward price

    Brent brent;

    struct target_function_m {
        Real t, T, r, recovery, runningSpread, stdDev, forwardPrice;
        std::function<Real(Real, Real, Real, Real, Real, Real, Real, Real)>& Vc;
        SimpsonIntegral simpson = SimpsonIntegral(1.0E-7, 100);
        Real operator()(Real m) const {
            return simpson([this, m](Real x) { return Vc(t, T, r, recovery, runningSpread, stdDev, m, x); }, -10.0,
                           10.0) -
                   forwardPrice;
        }
    };

    target_function_m target{exerciseTime,
                             maturityTime,
                             averageInterestRate,
                             indexRecovery_,
                             arguments_.swap->runningSpread(),
                             stdDev,
                             forwardPrice,
                             Vc};

    Real m;
    try {
        m = brent.solve(target, 1.0E-7, arguments_.swap->fairSpreadClean(), 0.01);
    } catch (const std::exception e) {
        QL_FAIL("NumericalIntegrationIndexCdsOptionEngine::doCalc(): failed to calibrate forward spread: " << e.what());
    }
    results_.additionalResults["fepAdjustedForwardSpread"] = m;
    results_.additionalResults["forwardSpread"] = arguments_.swap->fairSpreadClean();

    // compute the strike adjustment

    Real H = forwardRiskyAnnuityStrike() * (arguments_.swap->runningSpread() - arguments_.strike);

    // price the option

    SimpsonIntegral simpson(1.0E-7, 100);
    try {
        results_.value =
            arguments_.swap->notional() * discToExercise *
            simpson(
                [exerciseTime, maturityTime, averageInterestRate, this, stdDev, m, omega, H, &Vc](Real x) {
                    return std::max(0.0, omega * Vc(exerciseTime, maturityTime, averageInterestRate, indexRecovery_,
                                                    arguments_.swap->runningSpread(), stdDev, m, x) +
                                             H + arguments_.realisedFep);
                },
                -10.0, 10.0);
    } catch (const std::exception e) {
        QL_FAIL("NumericalIntegrationIndexCdsOptionEngine::doCalc(): failed to compute option payoff: " << e.what());
    }

} // doCalc();

} // namespace QuantExt
