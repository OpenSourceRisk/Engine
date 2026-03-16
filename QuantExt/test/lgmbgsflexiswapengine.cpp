/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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
#include <qle/pricingengines/numericlgmbgsflexiswapengine.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/models/shortrate/onefactormodels/gsr.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/gaussian1dswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/all.hpp>

#include <boost/assign/std/vector.hpp>
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
        fixedScheduleSeasoned = Schedule(effectiveDate - 2 * Years, maturityDate - 2 * Years, 1 * Years, cal,
                                         ModifiedFollowing, ModifiedFollowing, DateGeneration::Forward, false);
        floatingScheduleSeasoned = Schedule(effectiveDate - 2 * Years, maturityDate - 2 * Years, 6 * Months, cal,
                                            ModifiedFollowing, ModifiedFollowing, DateGeneration::Forward, false);
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
    Schedule fixedScheduleSeasoned, floatingScheduleSeasoned;
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

BOOST_FIXTURE_TEST_SUITE(LgmBgsFlexiSwapEngineTest, TestData)

BOOST_AUTO_TEST_CASE(testConsistencyWithFlexiSwapPricing) {

    BOOST_TEST_MESSAGE("Testing LGM BGS Flexi-Swap engine against LGM Flexi-Swap engine...");

    // balance guaranteed swap

    Size nFixed = fixedSchedule.size() - 1, nFloat = floatingSchedule.size() - 1;

    std::vector<std::vector<Real>> trancheNominals{
        {1000.0, 900.0, 800.0, 700.0, 600.0, 500.0, 400.0, 300.0, 200.0, 100.0},
        {300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0}};

    // tranche 0
    QuantLib::ext::shared_ptr<BalanceGuaranteedSwap> bgs0 = QuantLib::ext::make_shared<BalanceGuaranteedSwap>(
        VanillaSwap::Payer, trancheNominals, fixedSchedule, 0, fixedSchedule, std::vector<Real>(nFixed, strike),
        Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0), std::vector<Real>(nFloat, 0.0),
        std::vector<Real>(nFloat, Null<Real>()), std::vector<Real>(nFloat, Null<Real>()), Actual360());

    // tranche 1
    QuantLib::ext::shared_ptr<BalanceGuaranteedSwap> bgs1 = QuantLib::ext::make_shared<BalanceGuaranteedSwap>(
        VanillaSwap::Payer, trancheNominals, fixedSchedule, 1, fixedSchedule, std::vector<Real>(nFixed, strike),
        Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0), std::vector<Real>(nFloat, 0.0),
        std::vector<Real>(nFloat, Null<Real>()), std::vector<Real>(nFloat, Null<Real>()), Actual360());

    Handle<Quote> minCpr(QuantLib::ext::make_shared<SimpleQuote>(0.05));
    Handle<Quote> maxCpr(QuantLib::ext::make_shared<SimpleQuote>(0.25));

    auto bgsEngine = QuantLib::ext::make_shared<NumericLgmBgsFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, minCpr, maxCpr, yts, QuantExt::NumericLgmBgsFlexiSwapEngine::Method::SingleSwaptions);

    bgs0->setPricingEngine(bgsEngine);
    Real bgs0Npv = bgs0->NPV();
    BOOST_TEST_MESSAGE("BGS Npv (tranche 0) = " << bgs0Npv);

    bgs0->setPricingEngine(dscSwapEngine);
    Real bgs0DscNpv = bgs0->NPV();
    BOOST_TEST_MESSAGE("BGS discounting engine Npv (tranche 0) = " << bgs0DscNpv);

    bgs1->setPricingEngine(bgsEngine);
    Real bgs1Npv = bgs1->NPV();
    BOOST_TEST_MESSAGE("BGS Npv (tranche 1) = " << bgs1Npv);

    bgs1->setPricingEngine(dscSwapEngine);
    Real bgs1DscNpv = bgs1->NPV();
    BOOST_TEST_MESSAGE("BGS discounting engine Npv (tranche 1) = " << bgs1DscNpv);

    // flexi swap

    // from manual calculation in Excel

    // tranche 0
    std::vector<Real> fixedNotionals0{1000.0,      835.0,       683.6666667, 545.0590909, 418.3002273,
                                      302.5740795, 197.1236156, 101.2497755, 14.31232412, 0.0};
    std::vector<Real> floatNotionals0(2 * fixedNotionals0.size(), 0.0);
    Size i = 0; // remove in C++14 and instead write [i=0, &fixedNotionals] in the capture below
    std::generate(floatNotionals0.begin(), floatNotionals0.end(),
                  [i, &fixedNotionals0]() mutable { return fixedNotionals0[i++ / 2]; });
    std::vector<Real> lowerNotionals0{1000.0, 575.0, 283.3333333, 84.46969697, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    // tranche 1
    std::vector<Real> fixedNotionals1{300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 235.7342431};
    std::vector<Real> floatNotionals1(2 * fixedNotionals1.size(), 0.0);
    i = 0; // see above
    std::generate(floatNotionals1.begin(), floatNotionals1.end(),
                  [i, &fixedNotionals1]() mutable { return fixedNotionals1[i++ / 2]; });
    std::vector<Real> lowerNotionals1{300.0,       300.0,       300.0,       300.0,       249.905303,
                                      159.6617214, 99.78857586, 60.58592106, 35.34178728, 19.43798301};

    // upper bound for zero CPR

    // tranche 0
    std::vector<Real> fixedNotionals0ZeroCpr{1000.0, 900.0, 800.0, 700.0, 600.0, 500.0, 400.0, 300.0, 200.0, 100.0};
    std::vector<Real> floatNotionals0ZeroCpr(2 * fixedNotionals0ZeroCpr.size(), 0.0);
    i = 0; // see above
    std::generate(floatNotionals0ZeroCpr.begin(), floatNotionals0ZeroCpr.end(),
                  [i, &fixedNotionals0ZeroCpr]() mutable { return fixedNotionals0ZeroCpr[i++ / 2]; });
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap0 = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, fixedNotionals0, floatNotionals0, fixedSchedule, std::vector<Real>(nFixed, strike),
        Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0), std::vector<Real>(nFloat, 0.0),
        std::vector<Real>(nFloat, Null<Real>()), std::vector<Real>(nFloat, Null<Real>()), Actual360(), lowerNotionals0,
        Position::Long);
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap0MinCpr0 = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, fixedNotionals0ZeroCpr, floatNotionals0ZeroCpr, fixedSchedule,
        std::vector<Real>(nFixed, strike), Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0),
        std::vector<Real>(nFloat, 0.0), std::vector<Real>(nFloat, Null<Real>()),
        std::vector<Real>(nFloat, Null<Real>()), Actual360(), lowerNotionals1, Position::Long);

    // tranche 1
    std::vector<Real> fixedNotionals1ZeroCpr(10, 300.0);
    std::vector<Real> floatNotionals1ZeroCpr(20, 300.0);
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap1 = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, fixedNotionals1, floatNotionals1, fixedSchedule, std::vector<Real>(nFixed, strike),
        Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0), std::vector<Real>(nFloat, 0.0),
        std::vector<Real>(nFloat, Null<Real>()), std::vector<Real>(nFloat, Null<Real>()), Actual360(), lowerNotionals1,
        Position::Long);
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap1MinCpr0 = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, fixedNotionals1ZeroCpr, floatNotionals1ZeroCpr, fixedSchedule,
        std::vector<Real>(nFixed, strike), Thirty360(Thirty360::BondBasis), floatingSchedule, euribor6m, std::vector<Real>(nFloat, 1.0),
        std::vector<Real>(nFloat, 0.0), std::vector<Real>(nFloat, Null<Real>()),
        std::vector<Real>(nFloat, Null<Real>()), Actual360(), lowerNotionals1, Position::Long);

    auto flexiEngine = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::SingleSwaptions);

    flexiSwap0->setPricingEngine(flexiEngine);
    Real flexi0Npv = flexiSwap0->NPV();
    BOOST_TEST_MESSAGE("Flexi-Swap Npv (tranche 0) = " << flexi0Npv);

    flexiSwap0MinCpr0->setPricingEngine(dscSwapEngine);
    Real flexi0DscNpv0 = flexiSwap0MinCpr0->NPV();
    BOOST_TEST_MESSAGE("Flexi-Swap (tranche 0, minCPR=0), discounting engine Npv = " << flexi0DscNpv0);

    flexiSwap1->setPricingEngine(flexiEngine);
    Real flexi1Npv = flexiSwap1->NPV();
    BOOST_TEST_MESSAGE("Flexi-Swap Npv (tranche 1) = " << flexi1Npv);

    flexiSwap1MinCpr0->setPricingEngine(dscSwapEngine);
    Real flexi1DscNpv0 = flexiSwap1MinCpr0->NPV();
    BOOST_TEST_MESSAGE("Flexi-Swap (tranche 1, minCPR=0), discounting engine Npv = " << flexi1DscNpv0);

    BOOST_CHECK_CLOSE(bgs0Npv, flexi0Npv, 1E-8);
    BOOST_CHECK_CLOSE(bgs0DscNpv, flexi0DscNpv0, 1E-8);
    BOOST_CHECK_CLOSE(bgs1Npv, flexi1Npv, 1E-8);
    BOOST_CHECK_CLOSE(bgs1DscNpv, flexi1DscNpv0, 1E-8);

} // testConsistencyWithFlexiSwap

BOOST_AUTO_TEST_CASE(testConsistencyWithFlexiSwapPricingSeasonedDeal) {

    BOOST_TEST_MESSAGE("Testing LGM BGS Flexi-Swap engine against LGM Flexi-Swap engine (seasoned deal)...");

    euribor6m->addFixing(Date(6, August, 2015), 0.01); // need for pricing

    // balance guaranteed swap

    Size nFixed = fixedScheduleSeasoned.size() - 1, nFloat = floatingScheduleSeasoned.size() - 1;

    std::vector<std::vector<Real>> trancheNominals{
        {1000.0, 900.0, 800.0, 700.0, 600.0, 500.0, 400.0, 300.0, 200.0, 100.0},
        {300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0, 300.0}};

    // tranche 0
    QuantLib::ext::shared_ptr<BalanceGuaranteedSwap> bgs0 = QuantLib::ext::make_shared<BalanceGuaranteedSwap>(
        VanillaSwap::Payer, trancheNominals, fixedScheduleSeasoned, 0, fixedScheduleSeasoned,
        std::vector<Real>(nFixed, strike), Thirty360(Thirty360::BondBasis), floatingScheduleSeasoned, euribor6m,
        std::vector<Real>(nFloat, 1.0), std::vector<Real>(nFloat, 0.0), std::vector<Real>(nFloat, Null<Real>()),
        std::vector<Real>(nFloat, Null<Real>()), Actual360());

    Handle<Quote> minCpr(QuantLib::ext::make_shared<SimpleQuote>(0.05));
    Handle<Quote> maxCpr(QuantLib::ext::make_shared<SimpleQuote>(0.25));

    auto bgsEngine = QuantLib::ext::make_shared<NumericLgmBgsFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, minCpr, maxCpr, yts, QuantExt::NumericLgmBgsFlexiSwapEngine::Method::SingleSwaptions);

    bgs0->setPricingEngine(bgsEngine);
    Real bgs0Npv = bgs0->NPV();
    BOOST_TEST_MESSAGE("BGS Npv (tranche 0) = " << bgs0Npv);

    bgs0->setPricingEngine(dscSwapEngine);
    Real bgs0DscNpv = bgs0->NPV();
    BOOST_TEST_MESSAGE("BGS discounting engine Npv (tranche 0) = " << bgs0DscNpv);

    // flexi swap

    // from manual calculation in Excel;
    // the prepayments start in the 3rd fixed period (this is the first period with a future start date)

    // tranche 0
    std::vector<Real> fixedNotionals0{1000.0,      900,       740.0,       593.4545455, 459.4363636,
                                      337.0827273, 225.59325, 124.2288375, 32.31258938, 0.0};
    std::vector<Real> floatNotionals0(2 * fixedNotionals0.size(), 0.0);
    Size i = 0; // remove in C++14 and instead write [i=0, &fixedNotionals] in the capture below
    std::generate(floatNotionals0.begin(), floatNotionals0.end(),
                  [i, &fixedNotionals0]() mutable { return fixedNotionals0[i++ / 2]; });
    std::vector<Real> lowerNotionals0{1000.0, 900.0, 500.0, 227.2727273, 42.72727273, 0.0, 0.0, 0.0, 0.0, 0.0};

    // upper bound for zero CPR

    // tranche 0
    std::vector<Real> fixedNotionals0ZeroCpr{1000.0, 900.0, 800.0, 700.0, 600.0, 500.0, 400.0, 300.0, 200.0, 100.0};
    std::vector<Real> floatNotionals0ZeroCpr(2 * fixedNotionals0ZeroCpr.size(), 0.0);
    i = 0; // see above
    std::generate(floatNotionals0ZeroCpr.begin(), floatNotionals0ZeroCpr.end(),
                  [i, &fixedNotionals0ZeroCpr]() mutable { return fixedNotionals0ZeroCpr[i++ / 2]; });
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap0 = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, fixedNotionals0, floatNotionals0, fixedScheduleSeasoned, std::vector<Real>(nFixed, strike),
        Thirty360(Thirty360::BondBasis), floatingScheduleSeasoned, euribor6m, std::vector<Real>(nFloat, 1.0),
        std::vector<Real>(nFloat, 0.0), std::vector<Real>(nFloat, Null<Real>()),
        std::vector<Real>(nFloat, Null<Real>()), Actual360(), lowerNotionals0, Position::Long);
    QuantLib::ext::shared_ptr<FlexiSwap> flexiSwap0MinCpr0 = QuantLib::ext::make_shared<FlexiSwap>(
        VanillaSwap::Payer, fixedNotionals0ZeroCpr, floatNotionals0ZeroCpr, fixedScheduleSeasoned,
        std::vector<Real>(nFixed, strike), Thirty360(Thirty360::BondBasis), floatingScheduleSeasoned, euribor6m,
        std::vector<Real>(nFloat, 1.0), std::vector<Real>(nFloat, 0.0), std::vector<Real>(nFloat, Null<Real>()),
        std::vector<Real>(nFloat, Null<Real>()), Actual360(), lowerNotionals0, Position::Long);

    auto flexiEngine = QuantLib::ext::make_shared<NumericLgmFlexiSwapEngine>(
        lgm, 7.0, 16, 7.0, 32, yts, QuantExt::NumericLgmFlexiSwapEngine::Method::SingleSwaptions);

    flexiSwap0->setPricingEngine(flexiEngine);
    Real flexi0Npv = flexiSwap0->NPV();
    BOOST_TEST_MESSAGE("Flexi-Swap Npv (tranche 0) = " << flexi0Npv);

    flexiSwap0MinCpr0->setPricingEngine(dscSwapEngine);
    Real flexi0DscNpv0 = flexiSwap0MinCpr0->NPV();
    BOOST_TEST_MESSAGE("Flexi-Swap (tranche 0, minCPR=0), discounting engine Npv = " << flexi0DscNpv0);

    BOOST_CHECK_CLOSE(bgs0Npv, flexi0Npv, 1E-8);
    BOOST_CHECK_CLOSE(bgs0DscNpv, flexi0DscNpv0, 1E-8);
} // testConsistencyWithFlexiSwap

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
