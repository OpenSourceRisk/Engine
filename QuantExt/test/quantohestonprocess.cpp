/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <boost/test/unit_test.hpp>
#include <test/toplevelfixture.hpp>

#include <qle/processes/quantohestonprocess.hpp>
#include <qle/processes/multiassetquantohestonprocess.hpp>
#include <qle/methods/multipathvariategenerator.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/models/equity/hestonmodel.hpp>
#include <ql/pricingengines/vanilla/analytichestonengine.hpp>
#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

namespace {
  /*
struct HestonData {
    Real v0;
    Real kappa;
    Real theta;
    Real sigma;
    Real rho;
};

struct QuantoHestonData {
    HestonProcess::Discretization disc;
    Size steps;
    Real spotCorrelation;
    Size equityCase; // 0-4, see above
    Size fxCase;     // 0-3, see above
    Size process;    // 0: QuantoHeston, 1: MultiAssetQuantoHeston, 2: MultiAssetQuantoHeston reversed
};
  */
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(QuantoHestonProcessTest)

BOOST_AUTO_TEST_CASE(testMinimalExample) {

    Date today(8, December, 2025);
    Settings::instance().evaluationDate() = today;
    Date maturity = today + 1*Years;
    DayCounter dc = Actual365Fixed();
    Time maturityTime = dc.yearFraction(today, maturity);
    SequenceType sequenceType = SobolBrownianBridge; //MersenneTwister; //Burley2020SobolBrownianBridge;
    Size paths = 10000;
    auto yieldCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.06, dc));
    auto dividendCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.01, dc));
    auto domesticYieldCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.03, dc));
    auto equitySpot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(100.0));
    auto fxSpot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));

    Real eqForward = equitySpot->value() / yieldCurve->discount(maturity) * dividendCurve->discount(maturity);
    Real fxForward = fxSpot->value() * domesticYieldCurve->discount(maturity) / yieldCurve->discount(maturity);

    BOOST_TEST_MESSAGE("equity forward: " << eqForward);
    BOOST_TEST_MESSAGE("fx forward:     " << fxForward);
 
    HestonProcess::Discretization disc = HestonProcess::QuadraticExponential;
    Size steps = 60;
    Real spotCorrelation = 0.8;
    Real eq_v0 = 0.04;
    Real eq_kappa = 1.0;
    Real eq_theta = 0.04;
    Real eq_sigma = 1e-4;
    Real eq_rho = 0.0;
    Real fx_v0 = 0.02;
    Real fx_kappa = 1.0;
    Real fx_theta = 0.02;
    Real fx_sigma = 1e-4;
    Real fx_rho = 0.0;
    
    // auto eqProcess = QuantLib::ext::make_shared<HestonProcess>(yieldCurve, dividendCurve, equitySpot, eq_v0,
    //     						       eq_kappa, eq_theta, eq_sigma, eq_rho, disc);
    // auto fxProcess = QuantLib::ext::make_shared<HestonProcess>(yieldCurve, domesticYieldCurve, fxSpot, fx_v0,
    //     						       fx_kappa, fx_theta, fx_sigma, fx_rho, disc);

    // auto quantoProcess = QuantLib::ext::make_shared<QuantoHestonProcess>(eqProcess, fxProcess, spotCorrelation);

    // QuantLib::ext::shared_ptr<StochasticProcess> process = quantoProcess;

    // TimeGrid timeGrid(maturityTime, steps);
    // auto generator = makeMultiPathVariateGenerator(sequenceType, 4, steps, 42);

    // Array initialState = process->initialValues();

    // Real eq = 0, fx = 0, eqfx = 0;
    // Size eqIndex = 0; 
    // Size fxIndex = 2;
    
    // for (Size path = 0; path < paths; ++path) {

    //     Array state = initialState;
    //     auto p = generator->next();

    //     for (Size i = 0; i < timeGrid.size() - 1; ++i) {
    //         Real t0 = timeGrid[i];
    //         Real dt = timeGrid[i + 1] - t0;
    //         const Array& dW = p.value[i];
    //         state = process->evolve(t0, state, dt, dW);
    //     }

    //     Real e = state[eqIndex];
    //     Real x = state[fxIndex];
    //     eq += e / paths;
    //     fx += x / paths;
    //     eqfx += e * x / paths;
        
    // }

    // // Check consistency of forwards
    // BOOST_TEST_MESSAGE("EQ: " << std::fixed << std::setprecision(8) << eq << " vs " << eqForward
    //     	       << " error " << std::scientific << fabs(eq - eqForward) / eqForward);
    
    // BOOST_TEST_MESSAGE("FX: " << std::fixed << std::setprecision(8) << fx << " vs " << fxForward
    //     	       << " error " << std::scientific << fabs(fx - fxForward) / fxForward);

    // Real expected = fxForward * eqForward;
    // Real error = std::fabs(eqfx - expected) / expected;
    // BOOST_TEST_MESSAGE("EQ*FX: " << std::fixed << std::setprecision(6) << eqfx << " vs " << expected
    //     	       << " error " << std::scientific << error << " checked");

    // Real tolerance = 3.0 / sqrt(paths);
    // BOOST_CHECK_SMALL(std::fabs(eqfx - fxForward * eqForward) / (fxForward * eqForward), tolerance);

    BOOST_CHECK(true);
}

/*

BOOST_AUTO_TEST_CASE(testMartingales) {

    BOOST_TEST_MESSAGE("Testing Quanto Heston Martingales");

    std::vector<HestonData> equityHestonData = {
        {0.04, 2.0, 0.04, 1e-4, 0.0}, // BS
        {0.04, 2.0, 0.04, 0.3, -0.7}, // Feller > 1
        {0.04, 2.0, 0.04, 0.5, -0.7}, // Feller < 1
        {0.04, 2.0, 0.04, 0.5, +0.7}  // Feller < 1
    };

    std::vector<HestonData> fxHestonData = {
        {0.02, 1.0, 0.02, 1e-4, 0.0}, // BS
        {0.02, 1.0, 0.02, 0.1, +0.4}, // Feller > 1
        {0.02, 1.0, 0.02, 0.3, +0.4}  // Feller < 1
    };

    std::vector<QuantoHestonData> testData = {
        {HestonProcess::FullTruncation,       60, 0.0, 0, 0, 0},
	{HestonProcess::PartialTruncation,    60, 0.0, 0, 0, 0},
        {HestonProcess::Reflection,           60, 0.0, 0, 0, 0},
	{HestonProcess::QuadraticExponential, 60, 0.0, 0, 0, 0},
        {HestonProcess::FullTruncation,       60, 0.8, 1, 1, 0},
	{HestonProcess::PartialTruncation,    60, 0.8, 1, 1, 0},
        {HestonProcess::Reflection,           60, 0.8, 1, 1, 0},
	{HestonProcess::QuadraticExponential, 60, 0.8, 1, 1, 0},
        {HestonProcess::FullTruncation,       60, 0.8, 2, 2, 0},
	{HestonProcess::QuadraticExponential, 60, 0.8, 2, 2, 0},
        {HestonProcess::FullTruncation,       60, 0.8, 1, 1, 1},
	{HestonProcess::FullTruncation,       60, 0.8, 2, 2, 1},
        {HestonProcess::FullTruncation,       60, 0.8, 1, 1, 2},
	{HestonProcess::FullTruncation,       60, 0.8, 2, 2, 2}
    };

    Date today(8, December, 2025);
    Settings::instance().evaluationDate() = today;
    Date maturity = today + 1*Years;
    DayCounter dc = Actual365Fixed();
    Time maturityTime = dc.yearFraction(today, maturity);
    SequenceType sequenceType = SobolBrownianBridge; //MersenneTwister; //Burley2020SobolBrownianBridge;
    Size paths = 10000;
    auto yieldCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.06, dc));
    auto dividendCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.01, dc));
    auto domesticYieldCurve = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(today, 0.03, dc));
    auto equitySpot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(100.0));
    auto fxSpot = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));

    Real eqForward = equitySpot->value() / yieldCurve->discount(maturity) * dividendCurve->discount(maturity);
    Real fxForward = fxSpot->value() * domesticYieldCurve->discount(maturity) / yieldCurve->discount(maturity);

    BOOST_TEST_MESSAGE("equity forward: " << eqForward);
    BOOST_TEST_MESSAGE("fx forward:     " << fxForward);

    for (Size d = 0; d < testData.size(); ++d) {
      
        BOOST_TEST_MESSAGE("Testing Quanto Heston Martingale case " << d+1 << "/" << testData.size());

	HestonProcess::Discretization disc = testData[d].disc;
        Size steps = testData[d].steps;
	Real spotCorrelation = testData[d].spotCorrelation;
        HestonData e = equityHestonData[testData[d].equityCase];
        HestonData x = fxHestonData[testData[d].fxCase];
        
        auto eqProcess = QuantLib::ext::make_shared<HestonProcess>(yieldCurve, dividendCurve, equitySpot, e.v0,
								   e.kappa, e.theta, e.sigma, e.rho, disc);
        auto fxProcess = QuantLib::ext::make_shared<HestonProcess>(yieldCurve, domesticYieldCurve, fxSpot, x.v0,
                                                                   x.kappa, x.theta, x.sigma, x.rho, disc);

	Matrix c(2, 2, 1.0);
	c[0][1] = c[1][0] = spotCorrelation;
        QuantLib::ext::shared_ptr<StochasticProcess> process;
	// locate the equity price and fx rate in the state array
	Size eqIndex = 0; 
	Size fxIndex = 2;
	Matrix parsimonious, sqrtCorr;
	std::vector<QuantLib::ext::shared_ptr<HestonProcess>> hestonProcesses;
	std::vector<Size> indices;
        if (testData[d].process == 0) {
            // single EQ+FX version
            auto quantoProcess = QuantLib::ext::make_shared<QuantoHestonProcess>(eqProcess, fxProcess, spotCorrelation);
	    sqrtCorr = quantoProcess->sqrtCorrelation();
	    parsimonious = quantoProcess->parsimoniousCorrelation();
	    hestonProcesses = {eqProcess, fxProcess};
	    indices = {1, Null<Size>()};
	    process = quantoProcess;
        } else {
            if (testData[d].process == 1) {
                // multi-asset version
                hestonProcesses = {eqProcess, fxProcess};
                indices = {1, Null<Size>()};
            } else if (testData[d].process == 2) {
                // multi-asset with reversed process order
                hestonProcesses = {fxProcess, eqProcess};
                indices = {Null<Size>(), 0};
                eqIndex = 2;
                fxIndex = 0;
            }
            auto quantoProcess = QuantLib::ext::make_shared<MultiAssetQuantoHestonProcess>(hestonProcesses, indices, c);
	    sqrtCorr = quantoProcess->sqrtCorrelation();
	    parsimonious = quantoProcess->parsimoniousCorrelation();
	    process = quantoProcess;
        }

        TimeGrid timeGrid(maturityTime, steps);
	auto generator = makeMultiPathVariateGenerator(sequenceType, 4, steps, 42);

        Array initialState = process->initialValues();

	Real eq = 0, fx = 0, eqfx = 0;
        std::vector<Real> strike = {eqForward * 0.95, eqForward, eqForward * 1.05};
        std::vector<Real> call(3, 0.0), put(3, 0.0), callfx(3, 0.0), putfx(3, 0.0);
	std::vector<Matrix> cor(timeGrid.size() - 1, Matrix(process->size(), process->size(), 0.0));

	for (Size path = 0; path < paths; ++path) {

            Array state = initialState;
            auto p = generator->next();

            for (Size i = 0; i < timeGrid.size() - 1; ++i) {
                Real t0 = timeGrid[i];
                Real dt = timeGrid[i + 1] - t0;
                const Array& dW = p.value[i];		
		state = process->evolve(t0, state, dt, dW);

		// test consistency with the parsimonious matrix and the internal rho's below
                Array dwc = sqrtCorr * dW;
                for (Size j = 0; j < dwc.size(); ++j) {
                    for (Size k = 0; k < dwc.size(); ++k)
                        cor[i][j][k] += dwc[j] * dwc[k];
                }
            }

            Real e = state[eqIndex];
	    Real x = state[fxIndex];
	    eq += e / paths;
            fx += x / paths;
            eqfx += e * x / paths;
            for (Size i = 0; i < strike.size(); ++i) {
                call[i] += std::max(e - strike[i], 0.0) / paths;
                put[i] += std::max(strike[i] - e, 0.0) / paths;
                callfx[i] += x * std::max(e - strike[i], 0.0) / paths;
                putfx[i] += x * std::max(strike[i] - e, 0.0) / paths;
            }
        }

	Real tolerance = 3.0 / sqrt(paths);

	// Check consistency of simulated correlations
        for (Size i = 0; i < timeGrid.size() - 1; ++i) {
            for (Size j = 0; j < parsimonious.rows(); j++) {
                for (Size k = 0; k < parsimonious.columns(); k++) {
                    Real c = cor[i][j][k] / paths;
                    // BOOST_TEST_MESSAGE("timeStep " << i << " correlation(" << j << "," << k << ") = " << c << " vs "
                    //                                << parsimonious[j][k]);
                    BOOST_CHECK_SMALL(fabs(c - parsimonious[j][k]), tolerance);
                }
            }
            for (Size j = 0; j < hestonProcesses.size(); ++j) {
                Real c = cor[i][2 * j][2 * j + 1] / paths;
                // BOOST_TEST_MESSAGE("timeStep " << i << "correlation(" << 2 * j << "," << 2 * j + 1 << ") = " << c
                //                                << " vs " << hestonProcesses[j]->rho());
                BOOST_CHECK_SMALL(fabs(c - hestonProcesses[j]->rho()), tolerance);
            }

	    // TODO: Add a test of the decorrelated variates here
	    
	}

        // Check consistency of forwards
        BOOST_TEST_MESSAGE("EQ: " << std::fixed << std::setprecision(8) << eq << " vs " << eqForward
                                  << " error " << std::scientific << fabs(eq - eqForward) / eqForward);

        BOOST_TEST_MESSAGE("FX: " << std::fixed << std::setprecision(8) << fx << " vs " << fxForward
                                  << " error " << std::scientific << fabs(fx - fxForward) / fxForward);

        Real expected = fxForward * eqForward;
        Real error = std::fabs(eqfx - expected) / expected;
        BOOST_TEST_MESSAGE("EQ*FX: " << std::fixed << std::setprecision(6) << eqfx << " vs " << expected
			   << " error " << std::scientific << error << " checked");
        BOOST_CHECK_SMALL(std::fabs(eqfx - fxForward * eqForward) / (fxForward * eqForward), tolerance);

	// Check consistency of vanilla options
        auto hestonModel = QuantLib::ext::make_shared<HestonModel>(eqProcess);
        auto hestonAnalyticEngine = QuantLib::ext::make_shared<AnalyticHestonEngine>(hestonModel);
        for (Size i = 0; i < strike.size(); ++i) {
            Real K = strike[i];
            auto callPayoff = QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Call, K);
            auto putPayoff = QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Put, K);
            auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(maturity);
            VanillaOption callOption(callPayoff, exercise);
            VanillaOption putOption(putPayoff, exercise);
            callOption.setPricingEngine(hestonAnalyticEngine);
            putOption.setPricingEngine(hestonAnalyticEngine);
            Real callCompounded = callOption.NPV() / yieldCurve->discount(maturity);
            Real putCompounded = putOption.NPV() / yieldCurve->discount(maturity);

            BOOST_TEST_MESSAGE("Strike " << K << " Call " << call[i] << " vs " << callCompounded << " error "
                                         << std::scientific << fabs(call[i] - callCompounded) / callCompounded);
            BOOST_TEST_MESSAGE("Strike " << K << " Put  " << put[i] << " vs " << putCompounded << " error "
                                         << std::scientific << fabs(put[i] - putCompounded) / putCompounded);

            Real callExpected = callCompounded * fxForward;
	    Real callError = fabs(callfx[i] - callExpected) / callExpected;
	    Real putExpected = putCompounded * fxForward;
	    Real putError = fabs(putfx[i] - putExpected) / putExpected;
            BOOST_TEST_MESSAGE("Strike " << K << " Call*FX " << callfx[i] << " vs " << callExpected << " error "
			       << std::scientific << callError << " checked");
            BOOST_TEST_MESSAGE("Strike " << K << " Put*FX " << putfx[i] << " vs " << putExpected << " error "
			       << std::scientific << putError << " checked");

            BOOST_CHECK_SMALL(std::fabs(callfx[i] - callCompounded * fxForward) / (callCompounded * fxForward), tolerance);
            BOOST_CHECK_SMALL(std::fabs(putfx[i] - putCompounded * fxForward) / (putCompounded * fxForward), tolerance);
        }
    }
}
*/

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
