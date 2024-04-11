/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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
//#include <oret/toplevelfixture.hpp>

#include <qle/pricingengines/mcmultilegoptionengine.hpp>

#include <qle/models/crossassetmodel.hpp>
#include <qle/models/fxbsconstantparametrization.hpp>
#include <qle/pricingengines/analyticcclgmfxoptionengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>

#include <ql/cashflows/simplecashflow.hpp>
#include <ql/currencies/america.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <boost/timer/timer.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

namespace {
struct BermudanTestData : public qle::test::TopLevelFixture {
    BermudanTestData()
        : evalDate(12, January, 2015), yts(QuantLib::ext::make_shared<FlatForward>(evalDate, 0.02, Actual365Fixed())),
          euribor6m(QuantLib::ext::make_shared<Euribor>(6 * Months, yts)), effectiveDate(TARGET().advance(evalDate, 2 * Days)),
          startDate(TARGET().advance(effectiveDate, 1 * Years)), maturityDate(TARGET().advance(startDate, 9 * Years)),
          fixedSchedule(startDate, maturityDate, 1 * Years, TARGET(), ModifiedFollowing, ModifiedFollowing,
                        DateGeneration::Forward, false),
          floatingSchedule(startDate, maturityDate, 6 * Months, TARGET(), ModifiedFollowing, ModifiedFollowing,
                           DateGeneration::Forward, false),
          underlying(
              QuantLib::ext::make_shared<VanillaSwap>(VanillaSwap(VanillaSwap::Payer, 1.0, fixedSchedule, 0.02, Thirty360(Thirty360::BondBasis),
                                                          floatingSchedule, euribor6m, 0.0, Actual360()))),
          reversion(0.03) {
        Settings::instance().evaluationDate() = evalDate;
        for (Size i = 0; i < 9; ++i) {
            exerciseDates.push_back(TARGET().advance(fixedSchedule[i], -2 * Days));
        }
        exercise = QuantLib::ext::make_shared<BermudanExercise>(exerciseDates, false);

        swaption = QuantLib::ext::make_shared<Swaption>(underlying, exercise);
        stepDates = std::vector<Date>(exerciseDates.begin(), exerciseDates.end() - 1);
        sigmas = std::vector<Real>(stepDates.size() + 1);
        for (Size i = 0; i < sigmas.size(); ++i) {
            sigmas[i] = 0.0050 + (0.0080 - 0.0050) * std::exp(-0.2 * static_cast<double>(i));
        }
        stepTimes_a = Array(stepDates.size());
        for (Size i = 0; i < stepDates.size(); ++i) {
            stepTimes_a[i] = yts->timeFromReference(stepDates[i]);
        }
        sigmas_a = Array(sigmas.begin(), sigmas.end());
        kappas_a = Array(sigmas_a.size(), reversion);
    }
    Date evalDate;
    Handle<YieldTermStructure> yts;
    QuantLib::ext::shared_ptr<IborIndex> euribor6m;
    Date effectiveDate, startDate, maturityDate;
    Schedule fixedSchedule, floatingSchedule;
    QuantLib::ext::shared_ptr<VanillaSwap> underlying;
    std::vector<Date> exerciseDates, stepDates;
    std::vector<Real> sigmas;
    QuantLib::ext::shared_ptr<Exercise> exercise;
    QuantLib::ext::shared_ptr<Swaption> swaption;
    Array stepTimes_a, sigmas_a, kappas_a;
    Real reversion;
}; // BermudanTestData
} // namespace

BOOST_FIXTURE_TEST_SUITE(OreAmcTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AmcMultiLegOptionTest)

BOOST_FIXTURE_TEST_CASE(testBermudanSwaption, BermudanTestData) {

    BOOST_TEST_MESSAGE("Testing pricing of bermudan swaption as multi leg option vs numeric swaption engine");

    auto multiLegOption = QuantLib::ext::make_shared<MultiLegOption>(
        std::vector<Leg>{underlying->leg(0), underlying->leg(1)}, std::vector<bool>{true, false},
        std::vector<Currency>{EURCurrency(), EURCurrency()}, exercise);

    auto lgm_p = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(EURCurrency(), yts, stepTimes_a, sigmas_a,
                                                                              stepTimes_a, kappas_a);

    auto xasset = Handle<CrossAssetModel>(
        QuantLib::ext::make_shared<CrossAssetModel>(std::vector<QuantLib::ext::shared_ptr<Parametrization>>{lgm_p}));
    auto lgm = QuantLib::ext::make_shared<LinearGaussMarkovModel>(lgm_p);

    auto swaptionEngineLgm = QuantLib::ext::make_shared<NumericLgmSwaptionEngine>(lgm, 7.0, 16, 7.0, 32);
    auto swapEngine = QuantLib::ext::make_shared<DiscountingSwapEngine>(yts);

    auto mcMultiLegOptionEngine = QuantLib::ext::make_shared<McMultiLegOptionEngine>(
        xasset, SobolBrownianBridge, SobolBrownianBridge, 25000, 0, 42, 42, 4, LsmBasisSystem::Monomial);

    underlying->setPricingEngine(swapEngine);
    swaption->setPricingEngine(swaptionEngineLgm);
    Real npvUnd0 = underlying->NPV();
    Real npv0 = swaption->NPV();
    BOOST_TEST_MESSAGE("npv (numeric lgm swaption engine): underlying = " << npvUnd0 << ", option = " << npv0);

    boost::timer::cpu_timer timer;
    multiLegOption->setPricingEngine(mcMultiLegOptionEngine);
    Real npvUnd1 = multiLegOption->result<Real>("underlyingNpv");
    Real npv1 = multiLegOption->NPV();
    timer.stop();
    BOOST_TEST_MESSAGE("npv (multi leg option engine)    : underlying = "
                       << npvUnd1 << ", option = " << npv1 << ", timing " << timer.elapsed().wall * 1e-6 << " ms");

    BOOST_CHECK_SMALL(std::abs(npvUnd0 - npvUnd1), 1.0E-4);
    BOOST_CHECK_SMALL(std::abs(npv0 - npv1), 1.0E-4);

} // testBermudanSwaption

BOOST_AUTO_TEST_CASE(testFxOption) {

    BOOST_TEST_MESSAGE("Testing pricing of fx option as multi leg option vs analytic engine");

    SavedSettings backup;
    Date refDate(12, January, 2015);
    Settings::instance().evaluationDate() = refDate;

    auto yts_eur = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(refDate, 0.02, Actual365Fixed()));
    auto yts_usd = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(refDate, 0.03, Actual365Fixed()));

    auto lgm_eur_p = QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(EURCurrency(), yts_eur, 0.01, 0.01);
    auto lgm_usd_p = QuantLib::ext::make_shared<IrLgm1fConstantParametrization>(USDCurrency(), yts_usd, 0.01, 0.01);

    Handle<Quote> fxspot(QuantLib::ext::make_shared<SimpleQuote>(0.9));

    auto fx_p = QuantLib::ext::make_shared<FxBsConstantParametrization>(USDCurrency(), fxspot, 0.15);

    Matrix corr(3, 3);
    // clang-format off
    corr[0][0] = 1.0; corr[0][1] = 0.2; corr[0][2] = 0.5;
    corr[1][0] = 0.2; corr[1][1] = 1.0; corr[1][2] = 0.4;
    corr[2][0] = 0.5; corr[2][1] = 0.4; corr[2][2] = 1.0;
    // clang-format on

    auto xasset = Handle<CrossAssetModel>(QuantLib::ext::make_shared<CrossAssetModel>(
        std::vector<QuantLib::ext::shared_ptr<Parametrization>>{lgm_eur_p, lgm_usd_p, fx_p}, corr));

    Date exDate(12, January, 2020);
    auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(exDate);
    auto fxOption =
        QuantLib::ext::make_shared<VanillaOption>(QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Call, 0.8), exercise);

    Leg usdFlow, eurFlow;
    usdFlow.push_back(QuantLib::ext::make_shared<SimpleCashFlow>(1.0, exDate + 1));
    eurFlow.push_back(QuantLib::ext::make_shared<SimpleCashFlow>(-0.8, exDate + 1));

    auto multiLegOption =
        QuantLib::ext::make_shared<MultiLegOption>(std::vector<Leg>{eurFlow, usdFlow}, std::vector<bool>{false, false},
                                           std::vector<Currency>{EURCurrency(), USDCurrency()}, exercise);

    auto analyticFxOptionEngine = QuantLib::ext::make_shared<AnalyticCcLgmFxOptionEngine>(*xasset, 0);
    fxOption->setPricingEngine(analyticFxOptionEngine);
    Real npv0 = fxOption->NPV();
    BOOST_TEST_MESSAGE("npv (analytic cclgm fx option engine): " << npv0);

    // for european options there is no traning phase actually
    auto mcMultiLegOptionEngine = QuantLib::ext::make_shared<McMultiLegOptionEngine>(
        xasset, SobolBrownianBridge, SobolBrownianBridge, 25000, 0, 42, 42, 4, LsmBasisSystem::Monomial);

    multiLegOption->setPricingEngine(mcMultiLegOptionEngine);
    boost::timer::cpu_timer timer;
    Real npv1 = multiLegOption->NPV();
    timer.stop();
    BOOST_TEST_MESSAGE("npv (multi leg option engine)        : " << npv1 << ", timing " << timer.elapsed().wall * 1e-6
                                                                 << " ms");

    BOOST_CHECK_SMALL(std::abs(npv1 - npv0), 1.0E-4);

} // testFxOption

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
