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

#include <qle/models/defaultableequityjumpdiffusionmodel.hpp>

#include "toplevelfixture.hpp"

#include <ql/currencies/europe.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/models/marketmodels/browniangenerators/sobolbrowniangenerator.hpp>
#include <ql/option.hpp>
#include <ql/pricingengines/blackformula.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/timegrid.hpp>

#include <boost/test/unit_test.hpp>

using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DefaultableEquityJumpDiffusionModelTest)

namespace {

// run a naive mc simulation and price defaultable zero bonds and equity call options at the payoffTimes

void runMcSimulation(const QuantLib::ext::shared_ptr<DefaultableEquityJumpDiffusionModel>& model, const Size nPaths,
                     const Size seed, const Size timeSteps, const std::vector<Real>& payoffTimes,
                     const std::vector<Real>& equityCallStrikes, std::vector<Real>& resultDefaultableBonds,
                     std::vector<Real>& resultEquityOptions) {

    TimeGrid grid(payoffTimes.begin(), payoffTimes.end(), timeSteps);

    std::vector<Size> payoffIndices;
    for (auto const& t : payoffTimes) {
        payoffIndices.push_back(grid.index(t));
    }

    resultDefaultableBonds.clear();
    resultEquityOptions.clear();
    resultDefaultableBonds.resize(payoffTimes.size(), 0.0);
    resultEquityOptions.resize(payoffTimes.size(), 0.0);

    auto pathGen = QuantLib::ext::make_shared<SobolBrownianGenerator>(1, grid.size() - 1, SobolBrownianGenerator::Steps, seed,
                                                              SobolRsg::JoeKuoD7);
    MersenneTwisterUniformRng mt(seed);
    std::vector<Real> out(1);

    for (Size p = 0; p < nPaths; ++p) {
        Real S = model->equity()->equitySpot()->value();
        Real z = std::log(S);
        Real B = 1.0;
        bool jump = false;
        Size payoffIndex = 0;
        pathGen->nextPath();
        for (Size i = 1; i < grid.size(); ++i) {
            // simulate path (without jumps)
            pathGen->nextStep(out);
            if (close_enough(S, 0.0))
                continue;
            Real t0 = grid[i - 1];
            Real t1 = grid[i];
            Real r = model->r(t0);
            Real q = model->q(t0);
            Real h = model->h(t0, S);
            Real sigma = model->sigma(t0);
            z += (r - q + model->eta() * h - 0.5 * sigma * sigma) * (t1 - t0) + sigma * std::sqrt(t1 - t0) * out[0];
            B *= std::exp(-r * (t1 - t0));

            // do we have a jump?
            jump = jump || mt.nextReal() < h * (t1 - t0);

            // jump to eta * S(t), as in process definition
            S = std::exp(z) * (jump ? (1.0 - model->eta()) : 1.0);

            // jump to zero, corresponding to calibration methodology
            // S = std::exp(z) * (jump ? 0.0 : 1.0);

            // simulate payoffs
            if (i == payoffIndices[payoffIndex]) {
                if (!jump) {
                    resultDefaultableBonds[payoffIndex] += B / static_cast<Real>(nPaths);
                }
                resultEquityOptions[payoffIndex] +=
                    std::max(S - equityCallStrikes[payoffIndex], 0.0) * B / static_cast<Real>(nPaths);
                ++payoffIndex;
            }
        }
    }
}

} // namespace

BOOST_AUTO_TEST_CASE(test_zero_p) {

    BOOST_TEST_MESSAGE("Test defaultable equity jump diffusion model calibration with closed form bootstrap for p=0");

    Real S0 = 100.0;
    std::vector<Real> stepTimes = {1.0, 2.0, 3.0, 4.0, 5.0};

    Handle<YieldTermStructure> rate(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> dividend(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    Handle<BlackVolTermStructure> vol(QuantLib::ext::make_shared<BlackConstantVol>(0, NullCalendar(), 0.3, Actual365Fixed()));
    Handle<DefaultProbabilityTermStructure> creditCurve(
        QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0050, Actual365Fixed()));

    auto equity = QuantLib::ext::make_shared<EquityIndex2>("myEqIndex", NullCalendar(), EURCurrency(),
                                                  Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(S0)), rate, dividend);

    std::vector<Real> strikes;
    for (auto const& t : stepTimes) {
        Real forward = equity->equitySpot()->value() * equity->equityDividendCurve()->discount(t) /
                       equity->equityForecastCurve()->discount(t);
        strikes.push_back(forward);
    }

    /* for smaller etas (like 0.5) the mc simulation will produce higher call prices than the market because
       - in the model calibration we assume that after a jump to default the equity does not contribute
         to the option payoff
       - in the mc simulation the equity might contribute to the call payoff for eta << 1, if the after-jump
         equity price is still above the strike (atm forward)
    */

    std::vector<Real> etas = {1.0, 0.9, 0.8};

    for (auto const& eta : etas) {

        std::vector<Real> resultDefaultableBonds, resultEquityOptions;
        // mode does not matter, since p=0 and we don't enforce Fokker-Planck bootstrap
        auto modelBuilder = QuantLib::ext::make_shared<DefaultableEquityJumpDiffusionModelBuilder>(
            stepTimes, equity, vol, creditCurve, 0.0, eta, false, 24, 100, 1E-4, 1.5, Null<Real>(),
            DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Simultaneously, false);
        auto model = *modelBuilder->model();

        runMcSimulation(model, 100000, 121, 5 * 24, stepTimes, strikes, resultDefaultableBonds, resultEquityOptions);

        Real tol = 0.1; // 0.1 percent
        BOOST_TEST_MESSAGE(std::right << std::setw(5) << "p" << std::setw(5) << "eta" << std::setw(5) << "t"
                                      << std::setw(16) << "h0" << std::setw(16) << "sigma" << std::setw(16) << "bond mc"
                                      << std::setw(16) << "bond mkt" << std::setw(16) << "equityCall mc"
                                      << std::setw(16) << "equityCall mkt" << std::setw(16) << "bond err %"
                                      << std::setw(16) << "eqCall err %");
        for (Size i = 0; i < stepTimes.size(); ++i) {
            Real bondMarket = rate->discount(stepTimes[i]) * creditCurve->survivalProbability(stepTimes[i]);
            Real eqOptionMarket =
                blackFormula(Option::Call, strikes[i], strikes[i],
                             std::sqrt(vol->blackVariance(stepTimes[i], strikes[i])), rate->discount(stepTimes[i]));
            BOOST_TEST_MESSAGE(std::right
                               << std::setw(5) << 0.0 << std::setw(5) << eta << std::setw(5) << stepTimes[i]
                               << std::setw(16) << model->h0()[i] << std::setw(16) << model->sigma()[i] << std::setw(16)
                               << resultDefaultableBonds[i] << std::setw(16) << bondMarket << std::setw(16)
                               << resultEquityOptions[i] << std::setw(16) << eqOptionMarket << std::setw(16)
                               << 100.0 * (resultDefaultableBonds[i] - bondMarket) / bondMarket << std::setw(16)
                               << 100.0 * (resultEquityOptions[i] - eqOptionMarket) / eqOptionMarket);
            BOOST_CHECK_CLOSE(resultDefaultableBonds[i], bondMarket, tol);
            BOOST_CHECK_CLOSE(resultEquityOptions[i], eqOptionMarket, tol);
        }
        BOOST_TEST_MESSAGE("done.");
    }
}

BOOST_AUTO_TEST_CASE(test_nonzero_p) {

    BOOST_TEST_MESSAGE("Test defaultable equity jump diffusion model calibration with Fokker-Planck bootstrap");

    Real S0 = 100.0;
    std::vector<Real> stepTimes = {1.0, 2.0, 3.0, 4.0, 5.0};

    Handle<YieldTermStructure> rate(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> dividend(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.02, Actual365Fixed()));
    Handle<BlackVolTermStructure> vol(QuantLib::ext::make_shared<BlackConstantVol>(0, NullCalendar(), 0.3, Actual365Fixed()));
    Handle<DefaultProbabilityTermStructure> creditCurve(
        QuantLib::ext::make_shared<FlatHazardRate>(0, NullCalendar(), 0.0050, Actual365Fixed()));

    auto equity = QuantLib::ext::make_shared<EquityIndex2>("myEqIndex", NullCalendar(), EURCurrency(),
                                                  Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(S0)), rate, dividend);

    std::vector<Real> strikes;
    for (auto const& t : stepTimes) {
        Real forward = equity->equitySpot()->value() * equity->equityDividendCurve()->discount(t) /
                       equity->equityForecastCurve()->discount(t);
        strikes.push_back(forward);
    }

    /* see above for a comment on small etas in the test */

    std::vector<DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode> modes = {
        DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Alternating,
        DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Simultaneously};
    std::vector<Real> ps = {0.0, 0.5};
    std::vector<Real> etas = {1.0, 0.9};

    for (auto const& mode : modes) {
        BOOST_TEST_MESSAGE(
            "Bootstrap mode = " << (mode == DefaultableEquityJumpDiffusionModelBuilder::BootstrapMode::Alternating
                                        ? "Alternating"
                                        : "Simultaneously"));
        for (auto const& p : ps) {
            for (auto const& eta : etas) {

                std::vector<Real> resultDefaultableBonds, resultEquityOptions;
                auto modelBuilder = QuantLib::ext::make_shared<DefaultableEquityJumpDiffusionModelBuilder>(
                    stepTimes, equity, vol, creditCurve, p, eta, false, 24, 400, 1E-5, 1.5, Null<Real>(), mode, true);
                auto model = *modelBuilder->model();

                runMcSimulation(model, 100000, 121, 5 * 24, stepTimes, strikes, resultDefaultableBonds,
                                resultEquityOptions);

                Real tol = 0.2; // 0.2 percent
                BOOST_TEST_MESSAGE(std::right << std::setw(5) << "p" << std::setw(5) << "eta" << std::setw(5) << "t"
                                              << std::setw(16) << "h0" << std::setw(16) << "sigma" << std::setw(16)
                                              << "bond mc" << std::setw(16) << "bond mkt" << std::setw(16)
                                              << "equityCall mc" << std::setw(16) << "equityCall mkt" << std::setw(16)
                                              << "bond err %" << std::setw(16) << "eqCall err %");
                for (Size i = 0; i < stepTimes.size(); ++i) {
                    Real bondMarket = rate->discount(stepTimes[i]) * creditCurve->survivalProbability(stepTimes[i]);
                    Real eqOptionMarket = blackFormula(Option::Call, strikes[i], strikes[i],
                                                       std::sqrt(vol->blackVariance(stepTimes[i], strikes[i])),
                                                       rate->discount(stepTimes[i]));
                    BOOST_TEST_MESSAGE(std::right
                                       << std::setw(5) << p << std::setw(5) << eta << std::setw(5) << stepTimes[i]
                                       << std::setw(16) << model->h0()[i] << std::setw(16) << model->sigma()[i]
                                       << std::setw(16) << resultDefaultableBonds[i] << std::setw(16) << bondMarket
                                       << std::setw(16) << resultEquityOptions[i] << std::setw(16) << eqOptionMarket
                                       << std::setw(16) << 100.0 * (resultDefaultableBonds[i] - bondMarket) / bondMarket
                                       << std::setw(16)
                                       << 100.0 * (resultEquityOptions[i] - eqOptionMarket) / eqOptionMarket);
                    BOOST_CHECK_CLOSE(resultDefaultableBonds[i], bondMarket, tol);
                    BOOST_CHECK_CLOSE(resultEquityOptions[i], eqOptionMarket, tol);
                }
                BOOST_TEST_MESSAGE("done.");
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
