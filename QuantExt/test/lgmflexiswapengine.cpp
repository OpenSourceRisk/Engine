/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include "toplevelfixture.hpp"

#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/lgm.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>
#include <qle/pricingengines/numericlgmflexiswapengine.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/all.hpp>

#include <boost/assign/std/vector.hpp>
#include <boost/timer/timer.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>

using namespace boost::assign;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;

namespace {

struct TestData : qle::test::TopLevelFixture {
    TestData() : qle::test::TopLevelFixture() {
        cal = TARGET();
        evalDate = Date(5, February, 2016);
        Settings::instance().evaluationDate() = evalDate;
        effectiveDate = Date(cal.advance(evalDate, 2 * Days));
        maturityDate = Date(cal.advance(effectiveDate, 10 * Years));
        fixedSchedule = Schedule(effectiveDate, maturityDate, 1 * Years, cal, ModifiedFollowing, ModifiedFollowing,
                                 DateGeneration::Forward, false);
        floatingSchedule = Schedule(effectiveDate, maturityDate, 6 * Months, cal, ModifiedFollowing, ModifiedFollowing,
                                    DateGeneration::Forward, false);
        rateLevel = 0.02;
        strike = 0.025;
        nominal = 1000.0;
        yts = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(evalDate, rateLevel, Actual365Fixed()));
        euribor6m = QuantLib::ext::make_shared<Euribor>(6 * Months, yts);
        vanillaSwap = QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, nominal, fixedSchedule, strike,
                                                      Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, 0.0, Actual360());
        for (Size i = 0; i < vanillaSwap->floatingLeg().size(); ++i) {
            auto cpn = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(vanillaSwap->floatingLeg()[i]);
            BOOST_REQUIRE(cpn != nullptr);
            if (cpn->fixingDate() > evalDate && i % 2 == 0)
                exerciseDates.push_back(cpn->fixingDate());
        }
        stepDates = std::vector<Date>(exerciseDates.begin(), exerciseDates.end() - 1);
        stepTimes = Array(stepDates.size());
        for (Size i = 0; i < stepDates.size(); ++i) {
            stepTimes[i] = yts->timeFromReference(stepDates[i]);
        }
        sigmas = Array(stepDates.size() + 1);
        for (Size i = 0; i < sigmas.size(); ++i) {
            sigmas[i] = 0.0050 + (0.0080 - 0.0050) * std::exp(-0.2 * static_cast<double>(i));
        }
        reversion = 0.03;
        lgmParam = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
            EURCurrency(), yts, stepTimes, Array(sigmas.begin(), sigmas.end()), stepTimes,
            Array(sigmas.size(), reversion));
        lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgmParam);
        dscSwapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(yts);
        vanillaSwap->setPricingEngine(dscSwapEngine);
    }

    Calendar cal;
    Date evalDate, effectiveDate, maturityDate;
    Schedule fixedSchedule, floatingSchedule;
    Real rateLevel, strike, nominal;
    Handle<YieldTermStructure> yts;
    QuantLib::ext::shared_ptr<IborIndex> euribor6m;
    QuantLib::ext::shared_ptr<VanillaSwap> vanillaSwap;
    std::vector<Date> exerciseDates, stepDates;
    Array stepTimes, sigmas;
    Real reversion;
    QuantLib::ext::shared_ptr<IrLgm1fParametrization> lgmParam;
    QuantLib::ext::shared_ptr<LinearGaussMarkovModel> lgm;
    QuantLib::ext::shared_ptr<DiscountingSwapEngine> dscSwapEngine;
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_FIXTURE_TEST_SUITE(LgmFlexiSwapEngineTest, TestData)

BOOST_AUTO_TEST_CASE(testSingleSwaption) {

    BOOST_TEST_MESSAGE("Testing LGM Flexi-Swap engine in single swaption case...");

    // vanilla bermudan swaption

    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates, false);
    QuantLib::ext::shared_ptr<Swaption> swaption = QuantLib::ext::make_shared<Swaption>(vanillaSwap, exercise);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngine =
        QuantLib::ext::make_shared<NumericLgmSwaptionEngine>(lgm, 7.0, 16, 7.0, 32);

    swaption->setPricingEngine(swaptionEngine);
    boost::timer::cpu_timer timer;
    Real swaptionNpv = swaption->NPV();
    timer.stop();

    Real swapNpv = vanillaSwap->NPV();

    BOOST_TEST_MESSAGE("swaption npv = " << swaptionNpv << " swap npv = " << swapNpv
                                         << " fix = " << vanillaSwap->legNPV(0) << " float =" << vanillaSwap->legNPV(1)
                                         << " timing = " << timer.elapsed().wall * 1e-6 << " ms");

    // flexi swap

    Size nFixed = fixedSchedule.size() - 1, nFloat = floatingSchedule.size() - 1;
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, std::vector<Real>(nFixed, nominal), std::vector<Real>(nFloat, nominal), fixedSchedule,
        std::vector<Real>(nFixed, strike), Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0),
        std::vector<Real>(nFloat, 0.0), std::vector<Real>(nFloat, Null<Real>()),
        std::vector<Real>(nFloat, Null<Real>()), Actual360(), std::vector<Real>(nFixed, 0.0), Position::Long);
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap2 = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Receiver, std::vector<Real>(nFixed, nominal), std::vector<Real>(nFloat, nominal), fixedSchedule,
        std::vector<Real>(nFixed, strike), Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0),
        std::vector<Real>(nFloat, 0.0), std::vector<Real>(nFloat, Null<Real>()),
        std::vector<Real>(nFloat, Null<Real>()), Actual360(), std::vector<Real>(nFixed, 0.0), Position::Short);

    auto flexiEngine = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::SwaptionArray);
    auto flexiEngine2 = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::SingleSwaptions);

    flexiSwap->setPricingEngine(dscSwapEngine);
    flexiSwap2->setPricingEngine(dscSwapEngine);
    Real flexiUnderlyingNpvAnalytical = flexiSwap->NPV();
    Real flexiUnderlyingNpvAnalytical2 = flexiSwap2->NPV();

    flexiSwap->setPricingEngine(flexiEngine);
    flexiSwap2->setPricingEngine(flexiEngine);
    timer.start();
    Real flexiNpv = flexiSwap->NPV();
    timer.stop();
    Real timing2 = timer.elapsed().wall * 1e-6;
    Real flexiUnderlyingNpv = flexiSwap->underlyingValue();
    Real flexiOptionNpv = flexiNpv - flexiUnderlyingNpv;
    Real flexiNpv2 = flexiSwap2->NPV();
    Real flexiUnderlyingNpv2 = flexiSwap2->underlyingValue();
    Real flexiOptionNpv2 = flexiNpv2 - flexiUnderlyingNpv2;

    flexiSwap->setPricingEngine(flexiEngine2);
    flexiSwap2->setPricingEngine(flexiEngine2);
    timer.start();
    Real flexiNpvb = flexiSwap->NPV();
    timer.stop();
    Real timing3 = timer.elapsed().wall * 1e-6;
    Real flexiUnderlyingNpvb = flexiSwap->underlyingValue();
    Real flexiOptionNpvb = flexiNpv - flexiUnderlyingNpv;
    Real flexiNpvb2 = flexiSwap2->NPV();
    Real flexiUnderlyingNpvb2 = flexiSwap2->underlyingValue();
    Real flexiOptionNpvb2 = flexiNpv2 - flexiUnderlyingNpv2;

    BOOST_TEST_MESSAGE("A1 flexi npv = " << flexiNpv << " flexi underlying npv = " << flexiUnderlyingNpv
                                         << " flexi option npv = " << flexiOptionNpv
                                         << " flexi analytical underlying npv = " << flexiUnderlyingNpvAnalytical
                                         << " timing = " << timing2 << " ms (method=SwaptionArray)");
    BOOST_TEST_MESSAGE("A2 flexi npv = " << flexiNpv2 << " flexi underlying npv = " << flexiUnderlyingNpv2
                                         << " flexi option npv = " << flexiOptionNpv2
                                         << " flexi analytical underlying npv = " << flexiUnderlyingNpvAnalytical2);

    BOOST_TEST_MESSAGE("B1 flexi npv = " << flexiNpvb << " flexi underlying npv = " << flexiUnderlyingNpvb
                                         << " flexi option npv = " << flexiOptionNpvb
                                         << " flexi analytical underlying npv = " << flexiUnderlyingNpvAnalytical
                                         << " timing = " << timing3 << " ms (method=SingleSwaptions)");
    BOOST_TEST_MESSAGE("B2 flexi npv = " << flexiNpvb2 << " flexi underlying npv = " << flexiUnderlyingNpvb2
                                         << " flexi option npv = " << flexiOptionNpvb2
                                         << " flexi analytical underlying npv = " << flexiUnderlyingNpvAnalytical2);

    // checks

    Real tol = 3E-5 * nominal; // 0.3 bp on nominal

    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpv + swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiOptionNpv - swaptionNpv), tol);

    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpv2 - swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiOptionNpv2 + swaptionNpv), tol);

    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpvAnalytical + swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpvAnalytical2 - swapNpv), tol);

    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpvb + swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiOptionNpvb - swaptionNpv), tol);

    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpvb2 - swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiOptionNpvb2 + swaptionNpv), tol);

    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpvAnalytical + swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpvAnalytical2 - swapNpv), tol);

} // testSingleSwaption

BOOST_AUTO_TEST_CASE(testMultipleSwaptions) {

    BOOST_TEST_MESSAGE("Testing LGM Flexi-Swap engine in multiple swaption case...");

    // flexi swap

    Size nFixed = fixedSchedule.size() - 1, nFloat = floatingSchedule.size() - 1;
    std::vector<Real> fixedNotionals{900.0, 1000.0, 1000.0, 800.0, 500.0, 500.0, 500.0, 500.0, 500.0, 500.0};
    std::vector<Real> floatNotionals{900.0, 900.0, 1000.0, 1000.0, 1000.0, 1000.0, 800.0, 800.0, 500.0, 500.0,
                                     500.0, 500.0, 500.0,  500.0,  500.0,  500.0,  500.0, 500.0, 500.0, 500.0};
    std::vector<Real> lowerNotionals{900.0, 1000.0, 750.0, 600.0, 250.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, fixedNotionals, floatNotionals, fixedSchedule, std::vector<Real>(nFixed, strike),
        Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0), std::vector<Real>(nFloat, 0.0),
        std::vector<Real>(nFloat, Null<Real>()), std::vector<Real>(nFloat, Null<Real>()), Actual360(), lowerNotionals,
        Position::Long);

    auto flexiEngine = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::SwaptionArray);
    auto flexiEngine2 = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::SingleSwaptions);

    flexiSwap->setPricingEngine(flexiEngine);
    boost::timer::cpu_timer timer;
    Real flexiNpv = flexiSwap->NPV();
    timer.stop();
    Real timing1 = timer.elapsed().wall * 1e-6;
    Real flexiUnderlyingNpv = flexiSwap->underlyingValue();
    Real flexiOptionNpv = flexiNpv - flexiUnderlyingNpv;

    flexiSwap->setPricingEngine(flexiEngine2);
    timer.start();
    Real flexiNpv2 = flexiSwap->NPV();
    timer.stop();
    Real timing3 = timer.elapsed().wall * 1e-6;
    Real flexiUnderlyingNpv2 = flexiSwap->underlyingValue();
    Real flexiOptionNpv2 = flexiNpv2 - flexiUnderlyingNpv2;

    flexiSwap->setPricingEngine(dscSwapEngine);
    Real flexiUnderlyingNpvAnalytical = flexiSwap->NPV();

    // replicating swaptions

    // 1) vol = 200, start 2, end 3
    Schedule fixedSchedule1(fixedSchedule[2], fixedSchedule[3], 1 * Years, cal, ModifiedFollowing, ModifiedFollowing,
                            DateGeneration::Forward, false);
    Schedule floatingSchedule1(floatingSchedule[4], floatingSchedule[6], 6 * Months, cal, ModifiedFollowing,
                               ModifiedFollowing, DateGeneration::Forward, false);
    auto swap1 = QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, 200.0, fixedSchedule1, strike, Thirty360(Thirty360::BondBasis),
                                                 floatingSchedule1, euribor6m, 0.0, Actual360());
    std::vector<Date> exerciseDates1(exerciseDates.begin() + 1, exerciseDates.begin() + 2);
    auto exercise1 = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates1, false);
    auto swaption1 = QuantLib::ext::make_shared<Swaption>(swap1, exercise1);

    // 1) vol = 50, start 2, end 4
    Schedule fixedSchedule2(fixedSchedule[2], fixedSchedule[4], 1 * Years, cal, ModifiedFollowing, ModifiedFollowing,
                            DateGeneration::Forward, false);
    Schedule floatingSchedule2(floatingSchedule[4], floatingSchedule[8], 6 * Months, cal, ModifiedFollowing,
                               ModifiedFollowing, DateGeneration::Forward, false);
    auto swap2 = QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, 50.0, fixedSchedule2, strike, Thirty360(Thirty360::BondBasis),
                                                 floatingSchedule2, euribor6m, 0.0, Actual360());
    std::vector<Date> exerciseDates2(exerciseDates.begin() + 1, exerciseDates.begin() + 3);
    auto exercise2 = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates2, false);
    auto swaption2 = QuantLib::ext::make_shared<Swaption>(swap2, exercise2);

    // 3) vol = 150, start 3, end 4
    Schedule fixedSchedule3(fixedSchedule[3], fixedSchedule[4], 1 * Years, cal, ModifiedFollowing, ModifiedFollowing,
                            DateGeneration::Forward, false);
    Schedule floatingSchedule3(floatingSchedule[6], floatingSchedule[8], 6 * Months, cal, ModifiedFollowing,
                               ModifiedFollowing, DateGeneration::Forward, false);
    auto swap3 = QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, 150.0, fixedSchedule3, strike, Thirty360(Thirty360::BondBasis),
                                                 floatingSchedule3, euribor6m, 0.0, Actual360());
    std::vector<Date> exerciseDates3(exerciseDates.begin() + 2, exerciseDates.begin() + 3);
    auto exercise3 = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates3, false);
    auto swaption3 = QuantLib::ext::make_shared<Swaption>(swap3, exercise3);

    // 4) vol = 250, start 4, end 10
    Schedule fixedSchedule4(fixedSchedule[4], fixedSchedule[10], 1 * Years, cal, ModifiedFollowing, ModifiedFollowing,
                            DateGeneration::Forward, false);
    Schedule floatingSchedule4(floatingSchedule[8], floatingSchedule[20], 6 * Months, cal, ModifiedFollowing,
                               ModifiedFollowing, DateGeneration::Forward, false);
    auto swap4 = QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, 250.0, fixedSchedule4, strike, Thirty360(Thirty360::BondBasis),
                                                 floatingSchedule4, euribor6m, 0.0, Actual360());
    std::vector<Date> exerciseDates4(exerciseDates.begin() + 3, exerciseDates.begin() + 9);
    auto exercise4 = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates4, false);
    auto swaption4 = QuantLib::ext::make_shared<Swaption>(swap4, exercise4);

    // 5) vol = 250, start 5, end 10
    Schedule fixedSchedule5(fixedSchedule[5], fixedSchedule[10], 1 * Years, cal, ModifiedFollowing, ModifiedFollowing,
                            DateGeneration::Forward, false);
    Schedule floatingSchedule5(floatingSchedule[10], floatingSchedule[20], 6 * Months, cal, ModifiedFollowing,
                               ModifiedFollowing, DateGeneration::Forward, false);
    auto swap5 = QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap::Receiver, 250.0, fixedSchedule5, strike, Thirty360(Thirty360::BondBasis),
                                                 floatingSchedule5, euribor6m, 0.0, Actual360());
    std::vector<Date> exerciseDates5(exerciseDates.begin() + 4, exerciseDates.begin() + 9);
    auto exercise5 = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates5, false);
    auto swaption5 = QuantLib::ext::make_shared<Swaption>(swap5, exercise5);

    QuantLib::ext::shared_ptr<PricingEngine> swaptionEngine =
        QuantLib::ext::make_shared<NumericLgmSwaptionEngine>(lgm, 7.0, 16, 7.0, 32);
    swaption1->setPricingEngine(swaptionEngine);
    swaption2->setPricingEngine(swaptionEngine);
    swaption3->setPricingEngine(swaptionEngine);
    swaption4->setPricingEngine(swaptionEngine);
    swaption5->setPricingEngine(swaptionEngine);
    timer.start();
    Real swaptionNpv = swaption1->NPV() + swaption2->NPV() + swaption3->NPV() + swaption4->NPV() + swaption5->NPV();
    timer.stop();
    Real timing2 = timer.elapsed().wall * 1e-6;

    BOOST_TEST_MESSAGE("swaption basket npv =" << swaptionNpv << " timing = " << timing2 << " ms");
    BOOST_TEST_MESSAGE("A flexi npv = " << flexiNpv << " flexi underlying npv = " << flexiUnderlyingNpv
                                        << " flexi option npv = " << flexiOptionNpv
                                        << " flexi analytical underlying npv = " << flexiUnderlyingNpvAnalytical
                                        << " timing = " << timing1 << " ms (method=SwaptionArray)");
    BOOST_TEST_MESSAGE("B flexi npv = " << flexiNpv2 << " flexi underlying npv = " << flexiUnderlyingNpv2
                                        << " flexi option npv = " << flexiOptionNpv2
                                        << " flexi analytical underlying npv = " << flexiUnderlyingNpvAnalytical
                                        << " timing = " << timing3 << " ms (method=SingleSwaptions)");
    // checks

    Real tol = 3E-5 * nominal; // 0.3 bp on nominal

    BOOST_CHECK_SMALL(std::abs(flexiOptionNpv - swaptionNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpv - flexiUnderlyingNpvAnalytical), tol);
    BOOST_CHECK_SMALL(std::abs(flexiNpv - flexiUnderlyingNpv - flexiOptionNpv), 1E-10);

    BOOST_CHECK_SMALL(std::abs(flexiOptionNpv2 - swaptionNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpv2 - flexiUnderlyingNpvAnalytical), tol);
    BOOST_CHECK_SMALL(std::abs(flexiNpv2 - flexiUnderlyingNpv2 - flexiOptionNpv2), 1E-10);
}

BOOST_AUTO_TEST_CASE(testDeterministicCase) {

    BOOST_TEST_MESSAGE("Testing LGM Flexi-Swap engine in deterministic case (zero swaptions)...");

    // vanilla swap

    Real swapNpv = vanillaSwap->NPV();
    BOOST_TEST_MESSAGE("swap npv = " << swapNpv << " fix = " << vanillaSwap->legNPV(0)
                                     << " float =" << vanillaSwap->legNPV(1));

    // flexi swap

    Size nFixed = fixedSchedule.size() - 1, nFloat = floatingSchedule.size() - 1;
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, std::vector<Real>(nFixed, nominal), std::vector<Real>(nFloat, nominal), fixedSchedule,
        std::vector<Real>(nFixed, strike), Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0),
        std::vector<Real>(nFloat, 0.0), std::vector<Real>(nFloat, Null<Real>()),
        std::vector<Real>(nFloat, Null<Real>()), Actual360(), std::vector<Real>(nFixed, nominal), Position::Long);

    auto flexiEngine = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::SwaptionArray);
    auto flexiEngine2 = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::SingleSwaptions);
    auto flexiEngine3 = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::Automatic);

    flexiSwap->setPricingEngine(flexiEngine);
    Real flexiNpv = flexiSwap->NPV();
    Real flexiUnderlyingNpv = flexiSwap->underlyingValue();
    Real flexiOptionNpv = flexiNpv - flexiUnderlyingNpv;
    flexiSwap->setPricingEngine(flexiEngine2);
    Real flexiNpv2 = flexiSwap->NPV();
    Real flexiUnderlyingNpv2 = flexiSwap->underlyingValue();
    Real flexiOptionNpv2 = flexiNpv2 - flexiUnderlyingNpv2;
    flexiSwap->setPricingEngine(flexiEngine3);
    Real flexiNpv3 = flexiSwap->NPV();
    Real flexiUnderlyingNpv3 = flexiSwap->underlyingValue();
    Real flexiOptionNpv3 = flexiNpv3 - flexiUnderlyingNpv3;

    BOOST_TEST_MESSAGE("1 flexi npv = " << flexiNpv << " flexi underlying npv = " << flexiUnderlyingNpv
                                        << " flexi option npv = " << flexiOptionNpv << " (method=SwaptionArray)");
    BOOST_TEST_MESSAGE("2 flexi npv = " << flexiNpv2 << " flexi underlying npv = " << flexiUnderlyingNpv2
                                        << " flexi option npv = " << flexiOptionNpv2 << " (method=SwaptionArray)");
    BOOST_TEST_MESSAGE("3 flexi npv = " << flexiNpv3 << " flexi underlying npv = " << flexiUnderlyingNpv3
                                        << " flexi option npv = " << flexiOptionNpv3 << " (method=SwaptionArray)");

    // checks

    Real tol = 3E-5 * nominal; // 0.3 bp on nominal

    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpv + swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiOptionNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpv2 + swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiOptionNpv2), tol);
    BOOST_CHECK_SMALL(std::abs(flexiUnderlyingNpv3 + swapNpv), tol);
    BOOST_CHECK_SMALL(std::abs(flexiOptionNpv3), tol);

} // testDeterministicCase

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
