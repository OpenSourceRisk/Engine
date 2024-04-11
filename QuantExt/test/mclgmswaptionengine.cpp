/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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
//#include <oret/toplevelfixture.hpp>
#include <test/toplevelfixture.hpp>

#include <algorithm>
#include <boost/assign/std/vector.hpp>

#include <qle/models/gaussian1dcrossassetadaptor.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/lgm.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>

#include <qle/pricingengines/mclgmswaptionengine.hpp>

#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/all.hpp>

#include <boost/timer/timer.hpp>

using namespace boost::assign;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(OreAmcTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AmcMcLgmSwaptionEngineTest)

BOOST_AUTO_TEST_CASE(testAgainstSwaptionEngines) {

    BOOST_TEST_MESSAGE("Testing MC LGM Bermudan swaption engine against other bermudan swaption engines...");

    Calendar cal = TARGET();
    Date evalDate(5, February, 2016);
    Date effectiveDate(cal.advance(evalDate, 2 * Days));
    Date startDate(cal.advance(effectiveDate, 1 * Years));
    Date maturityDate(cal.advance(startDate, 9 * Years));

    Settings::instance().evaluationDate() = evalDate;

    // Setup the bermudan swaption and its underlying swap
    Real nominal = 1.0;
    // Fixed leg
    Rate fixedRate = 0.02;
    DayCounter fixedDayCount = Thirty360(Thirty360::BondBasis);
    Schedule fixedSchedule(startDate, maturityDate, 1 * Years, cal, ModifiedFollowing, ModifiedFollowing,
                           DateGeneration::Forward, false);

    // Floating leg
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(evalDate, 0.02, Actual365Fixed()));
    QuantLib::ext::shared_ptr<IborIndex> euribor6m(QuantLib::ext::make_shared<Euribor>(6 * Months, yts));
    Spread spread = 0.0;
    DayCounter floatingDayCount = Actual360();
    Schedule floatingSchedule(startDate, maturityDate, 6 * Months, cal, ModifiedFollowing, ModifiedFollowing,
                              DateGeneration::Forward, false);

    // Underlying swap
    QuantLib::ext::shared_ptr<VanillaSwap> undlSwap = QuantLib::ext::make_shared<VanillaSwap>(
        VanillaSwap(VanillaSwap::Payer, nominal, fixedSchedule, fixedRate, fixedDayCount, floatingSchedule, euribor6m,
                    spread, floatingDayCount));

    undlSwap->setPricingEngine(QuantLib::ext::make_shared<DiscountingSwapEngine>(yts));
    BOOST_TEST_MESSAGE("Underlying value analytic = " << undlSwap->NPV());

    // Bermudan swaption
    std::vector<Date> exerciseDates;
    for (Size i = 0; i < 9; ++i) {
        exerciseDates.push_back(cal.advance(fixedSchedule[i], -2 * Days));
    }
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates, false);
    QuantLib::ext::shared_ptr<Swaption> swaption = QuantLib::ext::make_shared<Swaption>(undlSwap, exercise);

    // Setup models and model adaptors
    std::vector<Date> stepDates = std::vector<Date>(exerciseDates.begin(), exerciseDates.end() - 1);
    Array stepTimes(stepDates.size());
    for (Size i = 0; i < stepDates.size(); ++i) {
        stepTimes[i] = yts->timeFromReference(stepDates[i]);
    }

    std::vector<Real> sigmas(stepDates.size() + 1);
    for (Size i = 0; i < sigmas.size(); ++i) {
        sigmas[i] = 0.0050 + (0.0080 - 0.0050) * std::exp(-0.2 * static_cast<double>(i));
    }

    Real reversion = 0.03;
    QuantLib::ext::shared_ptr<Gsr> gsr = QuantLib::ext::make_shared<Gsr>(yts, stepDates, sigmas, reversion, 50.0);

    // The Hull White adaptor for the LGM parametrization should lead to equal Bermudan swaption prices
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgmParam = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        EURCurrency(), yts, stepTimes, Array(sigmas.begin(), sigmas.end()), stepTimes, Array(sigmas.size(), reversion));

    // fix any T forward measure
    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgmParam);
    QuantLib::ext::shared_ptr<Gaussian1dModel> lgmGaussian1d = QuantLib::ext::make_shared<Gaussian1dCrossAssetAdaptor>(lgm);

    // Setup the different pricing engines
    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineGsr =
        QuantLib::ext::make_shared<Gaussian1dSwaptionEngine>(gsr, 64, 7.0, true, false);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineLgm =
        QuantLib::ext::make_shared<Gaussian1dSwaptionEngine>(lgmGaussian1d, 64, 7.0, true, false);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineLgm2 =
        QuantLib::ext::make_shared<NumericLgmSwaptionEngine>(lgm, 7.0, 16, 7.0, 32);

    Size polynomOrder = 4;
    LsmBasisSystem::PolynomialType polynomType = LsmBasisSystem::Monomial;
    Size tSamples = 10000;
    Size pSamples = 10000;
    QuantLib::ext::shared_ptr<StochasticProcess> stateProcess = lgm->stateProcess();

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngineLgmMC = QuantLib::ext::shared_ptr<McLgmSwaptionEngine>(
        new McLgmSwaptionEngine(lgm, MersenneTwisterAntithetic, SobolBrownianBridge, tSamples, pSamples, 42, 43,
                                polynomOrder, polynomType));

    // Calculate the T0 price of the bermudan swaption using different engines
    swaption->setPricingEngine(swaptionEngineGsr);
    Real npvGsr = swaption->NPV();

    swaption->setPricingEngine(swaptionEngineLgm);
    Real npvLgm = swaption->NPV();

    swaption->setPricingEngine(swaptionEngineLgm2);
    Real npvLgm2 = swaption->NPV();

    boost::timer::cpu_timer timer;
    swaption->setPricingEngine(swaptionEngineLgmMC);
    Real npvLgmMc = swaption->NPV();
    Real undNpvMc = swaption->result<Real>("underlyingNpv");
    BOOST_TEST_MESSAGE("Underlying value mc   = " << undNpvMc);
    BOOST_TEST_MESSAGE("npvGsr: " << npvGsr << ", npvLgm: " << npvLgm << ", npvLgm2: " << npvLgm
                                  << ", npvLgmMc: " << npvLgmMc);
    timer.stop();
    BOOST_TEST_MESSAGE("timing mc engine: " << timer.elapsed().wall * 1e-6 << " ms");

    Real tol = 2E-4; // basis point tolerance
    BOOST_CHECK_SMALL(std::fabs(undNpvMc - undlSwap->NPV()), tol);
    BOOST_CHECK_SMALL(std::fabs(npvGsr - npvLgm), tol);
    BOOST_CHECK_SMALL(std::fabs(npvGsr - npvLgm2), tol);
    BOOST_CHECK_SMALL(std::fabs(npvGsr - npvLgmMc), tol);
} // testAgainstSwaptionEngines

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
