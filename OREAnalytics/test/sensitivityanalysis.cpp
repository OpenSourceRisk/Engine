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
#include <boost/timer/timer.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/filteredsensitivitystream.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/parametricvar.hpp>
#include <orea/engine/riskfilter.hpp>
#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/sensitivitycubestream.hpp>
#include <orea/engine/sensitivityfilestream.hpp>
#include <orea/engine/sensitivityinmemorystream.hpp>
#include <orea/engine/sensitivityrecord.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/engine/stresstest.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/builders/commodityoption.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/commodityforward.hpp>
#include <ored/portfolio/commodityoption.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/to_string.hpp>
#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>
#include <test/testportfolio.hpp>

#include "testmarket.hpp"

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

using boost::timer::cpu_timer;
using boost::timer::default_places;
using testsuite::buildBermudanSwaption;
using testsuite::buildCap;
using testsuite::buildCommodityForward;
using testsuite::buildCommodityOption;
using testsuite::buildCPIInflationSwap;
using testsuite::buildEquityOption;
using testsuite::buildEuropeanSwaption;
using testsuite::buildFloor;
using testsuite::buildFxOption;
using testsuite::buildSwap;
using testsuite::buildYYInflationSwap;
using testsuite::buildZeroBond;
using testsuite::TestConfigurationObjects;
using testsuite::TestMarket;

namespace {
void testPortfolioSensitivity(ObservationMode::Mode om) {
    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(om);

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData =
        TestConfigurationObjects::setupSimMarketData5();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = TestConfigurationObjects::setupSensitivityScenarioData5();

    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // build portfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("CrossCurrencySwap") = "DiscountedCashflows";
    data->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    data->model("BermudanSwaption") = "LGM";
    data->modelParameters("BermudanSwaption")["Calibration"] = "Bootstrap";
    data->modelParameters("BermudanSwaption")["CalibrationStrategy"] = "CoterminalATM";
    data->modelParameters("BermudanSwaption")["Reversion"] = "0.03";
    data->modelParameters("BermudanSwaption")["ReversionType"] = "HullWhite";
    data->modelParameters("BermudanSwaption")["Volatility"] = "0.01";
    data->modelParameters("BermudanSwaption")["VolatilityType"] = "Hagan";
    data->modelParameters("BermudanSwaption")["Tolerance"] = "0.0001";
    data->engine("BermudanSwaption") = "Grid";
    data->engineParameters("BermudanSwaption")["sy"] = "3.0";
    data->engineParameters("BermudanSwaption")["ny"] = "10";
    data->engineParameters("BermudanSwaption")["sx"] = "3.0";
    data->engineParameters("BermudanSwaption")["nx"] = "10";
    data->model("FxForward") = "DiscountedCashflows";
    data->engine("FxForward") = "DiscountingFxForwardEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";
    data->model("CapFloor") = "IborCapModel";
    data->engine("CapFloor") = "IborCapEngine";
    data->model("CapFlooredIborLeg") = "BlackOrBachelier";
    data->engine("CapFlooredIborLeg") = "BlackIborCouponPricer";
    data->model("Bond") = "DiscountedCashflows";
    data->engine("Bond") = "DiscountingRiskyBondEngine";
    data->engineParameters("Bond")["TimestepPeriod"] = "6M";
    data->model("EquityForward") = "DiscountedCashflows";
    data->engine("EquityForward") = "DiscountingEquityForwardEngine";
    data->model("EquityOption") = "BlackScholesMerton";
    data->engine("EquityOption") = "AnalyticEuropeanEngine";
    data->model("CommodityForward") = "DiscountedCashflows";
    data->engine("CommodityForward") = "DiscountingCommodityForwardEngine";
    data->model("CommodityOption") = "BlackScholes";
    data->engine("CommodityOption") = "AnalyticEuropeanEngine";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, simMarket);

    // QuantLib::ext::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    portfolio->add(buildSwap("1_Swap_EUR", "EUR", true, 10000000.0, 0, 10, 0.03, 0.00, "1Y", "30/360", "6M", "A360",
                             "EUR-EURIBOR-6M"));
    portfolio->add(buildSwap("2_Swap_USD", "USD", true, 10000000.0, 0, 15, 0.02, 0.00, "6M", "30/360", "3M", "A360",
                             "USD-LIBOR-3M"));
    portfolio->add(buildSwap("3_Swap_GBP", "GBP", true, 10000000.0, 0, 20, 0.04, 0.00, "6M", "30/360", "3M", "A360",
                             "GBP-LIBOR-6M"));
    portfolio->add(buildSwap("4_Swap_JPY", "JPY", true, 1000000000.0, 0, 5, 0.01, 0.00, "6M", "30/360", "3M", "A360",
                             "JPY-LIBOR-6M"));
    portfolio->add(buildEuropeanSwaption("5_Swaption_EUR", "Long", "EUR", true, 1000000.0, 10, 10, 0.02, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M", "Physical"));
    portfolio->add(buildEuropeanSwaption("6_Swaption_EUR", "Long", "EUR", true, 1000000.0, 2, 5, 0.02, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M", "Physical"));
    portfolio->add(buildEuropeanSwaption("17_Swaption_EUR", "Long", "EUR", true, 1000000.0, 2, 5, 0.02, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M", "Physical", 1200.0, "EUR",
                                         "2018-04-14"));
    portfolio->add(buildBermudanSwaption("13_Swaption_EUR", "Long", "EUR", true, 1000000.0, 5, 2, 10, 0.02, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildFxOption("7_FxOption_EUR_USD", "Long", "Call", 3, "EUR", 10000000.0, "USD", 11000000.0));
    portfolio->add(buildFxOption("8_FxOption_EUR_GBP", "Long", "Call", 7, "EUR", 10000000.0, "GBP", 11000000.0));
    portfolio->add(buildCap("9_Cap_EUR", "EUR", "Long", 0.05, 1000000.0, 0, 10, "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildFloor("10_Floor_USD", "USD", "Long", 0.01, 1000000.0, 0, 10, "3M", "A360", "USD-LIBOR-3M"));
    portfolio->add(buildZeroBond("11_ZeroBond_EUR", "EUR", 1.0, 10, "0"));
    portfolio->add(buildZeroBond("12_ZeroBond_USD", "USD", 1.0, 10, "0"));
    portfolio->add(buildEquityOption("14_EquityOption_SP5", "Long", "Call", 2, "SP5", "USD", 2147.56, 775));
    portfolio->add(buildCPIInflationSwap("15_CPIInflationSwap_UKRPI", "GBP", true, 100000.0, 0, 10, 0.0, "6M",
                                         "ACT/ACT", "GBP-LIBOR-6M", "1Y", "ACT/ACT", "UKRPI", 201.0, "2M", false,
                                         0.005));
    portfolio->add(buildYYInflationSwap("16_YoYInflationSwap_UKRPI", "GBP", true, 100000.0, 0, 10, 0.0, "1Y", "ACT/ACT",
                                        "GBP-LIBOR-6M", "1Y", "ACT/ACT", "UKRPI", "2M", 2));
    portfolio->add(buildCommodityForward("17_CommodityForward_GOLD", "Long", 1, "COMDTY_GOLD_USD", "USD", 1170.0, 100));
    portfolio->add(buildCommodityForward("18_CommodityForward_OIL", "Short", 4, "COMDTY_WTI_USD", "USD", 46.0, 100000));
    portfolio->add(
        buildCommodityOption("19_CommodityOption_GOLD", "Long", "Call", 1, "COMDTY_GOLD_USD", "USD", 1170.0, 100));
    portfolio->add(
        buildCommodityOption("20_CommodityOption_OIL", "Short", "Put", 4, "COMDTY_WTI_USD", "USD", 46.0, 100000));
    portfolio->build(factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // build the scenario valuation engine
    QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>(
        "1,0W"); // TODO - extend the DateGrid interface so that it can actually take a vector of dates as input
    vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(simMarketData->baseCcy()));
    ValuationEngine engine(today, dg, simMarket,
                           factory->modelBuilders()); // last argument required for model recalibration
    // run scenarios and fill the cube
    cpu_timer t;
    QuantLib::ext::shared_ptr<NPVCube> cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(
        today, portfolio->ids(), vector<Date>(1, today), scenarioGenerator->samples());
    engine.buildCube(portfolio, cube, calculators);
    t.stop();

    struct Results {
        string id;
        string label;
        Real npv;
        Real sensi;
    };

    // clang-format off
    std::vector<Results> cachedResults = {
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/0/6M", -928826, -2.51631},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/1/1Y", -928826, 14.6846},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/2/2Y", -928826, 19.0081},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/3/3Y", -928826, 46.1186},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/4/5Y", -928826, 85.1033},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/5/7Y", -928826, 149.43},
        {"1_Swap_EUR", "Up:DiscountCurve/EUR/6/10Y", -928826, 205.064},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/0/6M", -928826, 2.51644},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/1/1Y", -928826, -14.6863},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/2/2Y", -928826, -19.0137},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/3/3Y", -928826, -46.1338},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/4/5Y", -928826, -85.1406},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/5/7Y", -928826, -149.515},
        {"1_Swap_EUR", "Down:DiscountCurve/EUR/6/10Y", -928826, -205.239},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/0/6M", -928826, -495.013},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/1/1Y", -928826, 14.7304},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", -928826, 38.7816},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", -928826, 94.186},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", -928826, 173.125},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", -928826, 304.648},
        {"1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", -928826, 8479.55},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/0/6M", -928826, 495.037},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/1/1Y", -928826, -14.5864},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", -928826, -38.4045},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", -928826, -93.532},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", -928826, -171.969},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", -928826, -302.864},
        {"1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", -928826, -8478.14},
        {"2_Swap_USD", "Up:DiscountCurve/USD/0/6M", 980404, -1.04797},
        {"2_Swap_USD", "Up:DiscountCurve/USD/1/1Y", 980404, -6.06931},
        {"2_Swap_USD", "Up:DiscountCurve/USD/2/2Y", 980404, -15.8605},
        {"2_Swap_USD", "Up:DiscountCurve/USD/3/3Y", 980404, -38.0708},
        {"2_Swap_USD", "Up:DiscountCurve/USD/4/5Y", 980404, -68.7288},
        {"2_Swap_USD", "Up:DiscountCurve/USD/5/7Y", 980404, -118.405},
        {"2_Swap_USD", "Up:DiscountCurve/USD/6/10Y", 980404, -244.946},
        {"2_Swap_USD", "Up:DiscountCurve/USD/7/15Y", 980404, -202.226},
        {"2_Swap_USD", "Up:DiscountCurve/USD/8/20Y", 980404, 0.0148314},
        {"2_Swap_USD", "Down:DiscountCurve/USD/0/6M", 980404, 1.04797},
        {"2_Swap_USD", "Down:DiscountCurve/USD/1/1Y", 980404, 6.06959},
        {"2_Swap_USD", "Down:DiscountCurve/USD/2/2Y", 980404, 15.8623},
        {"2_Swap_USD", "Down:DiscountCurve/USD/3/3Y", 980404, 38.0784},
        {"2_Swap_USD", "Down:DiscountCurve/USD/4/5Y", 980404, 68.7502},
        {"2_Swap_USD", "Down:DiscountCurve/USD/5/7Y", 980404, 118.458},
        {"2_Swap_USD", "Down:DiscountCurve/USD/6/10Y", 980404, 245.108},
        {"2_Swap_USD", "Down:DiscountCurve/USD/7/15Y", 980404, 202.42},
        {"2_Swap_USD", "Down:DiscountCurve/USD/8/20Y", 980404, -0.0148314},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/0/6M", 980404, -201.015},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/1/1Y", 980404, 18.134},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/2/2Y", 980404, 47.3066},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/3/3Y", 980404, 113.4},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/4/5Y", 980404, 205.068},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/5/7Y", 980404, 352.859},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/6/10Y", 980404, 730.076},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/7/15Y", 980404, 8626.78},
        {"2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/8/20Y", 980404, 5.86437},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/0/6M", 980404, 201.03},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/1/1Y", 980404, -18.0746},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/2/2Y", 980404, -47.1526},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/3/3Y", 980404, -113.136},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/4/5Y", 980404, -204.611},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/5/7Y", 980404, -352.166},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/6/10Y", 980404, -729.248},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/7/15Y", 980404, -8626.13},
        {"2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/8/20Y", 980404, -5.86436},
        {"2_Swap_USD", "Up:FXSpot/EURUSD/0/spot", 980404, -9706.97},
        {"2_Swap_USD", "Down:FXSpot/EURUSD/0/spot", 980404, 9903.07},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/0/6M", 69795.3, 2.12392},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/1/1Y", 69795.3, -0.646097},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/2/2Y", 69795.3, -1.75066},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/3/3Y", 69795.3, -4.24827},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/4/5Y", 69795.3, -7.2252},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/5/7Y", 69795.3, -12.5287},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/6/10Y", 69795.3, -24.7828},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/7/15Y", 69795.3, -39.2456},
        {"3_Swap_GBP", "Up:DiscountCurve/GBP/8/20Y", 69795.3, 31.2081},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/0/6M", 69795.3, -2.12413},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/1/1Y", 69795.3, 0.645698},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/2/2Y", 69795.3, 1.74981},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/3/3Y", 69795.3, 4.2473},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/4/5Y", 69795.3, 7.22426},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/5/7Y", 69795.3, 12.5298},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/6/10Y", 69795.3, 24.7939},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/7/15Y", 69795.3, 39.2773},
        {"3_Swap_GBP", "Down:DiscountCurve/GBP/8/20Y", 69795.3, -31.2925},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/0/6M", 69795.3, -308.49},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/1/1Y", 69795.3, 68.819},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/2/2Y", 69795.3, 81.3735},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/3/3Y", 69795.3, 239.034},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/4/5Y", 69795.3, 372.209},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/5/7Y", 69795.3, 654.949},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/6/10Y", 69795.3, 1343.01},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/7/15Y", 69795.3, 2139.68},
        {"3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/8/20Y", 69795.3, 12633.8},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/0/6M", 69795.3, 308.513},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/1/1Y", 69795.3, -68.7287},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/2/2Y", 69795.3, -81.1438},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/3/3Y", 69795.3, -238.649},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/4/5Y", 69795.3, -371.553},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/5/7Y", 69795.3, -653.972},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/6/10Y", 69795.3, -1341.88},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/7/15Y", 69795.3, -2138.11},
        {"3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/8/20Y", 69795.3, -12632.5},
        {"3_Swap_GBP", "Up:FXSpot/EURGBP/0/spot", 69795.3, -691.043},
        {"3_Swap_GBP", "Down:FXSpot/EURGBP/0/spot", 69795.3, 705.003},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/0/6M", 871.03, -0.00750246},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/1/1Y", 871.03, -0.00147994},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/2/2Y", 871.03, -0.020079},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/3/3Y", 871.03, -0.0667249},
        {"4_Swap_JPY", "Up:DiscountCurve/JPY/4/5Y", 871.03, 4.75708},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/0/6M", 871.03, 0.00747801},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/1/1Y", 871.03, 0.00140807},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/2/2Y", 871.03, 0.0199001},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/3/3Y", 871.03, 0.0664106},
        {"4_Swap_JPY", "Down:DiscountCurve/JPY/4/5Y", 871.03, -4.75978},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/0/6M", 871.03, -193.514},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/1/1Y", 871.03, 2.95767},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/2/2Y", 871.03, 7.81453},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/3/3Y", 871.03, 19.3576},
        {"4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/4/5Y", 871.03, 3832.83},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/0/6M", 871.03, 193.528},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/1/1Y", 871.03, -2.90067},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/2/2Y", 871.03, -7.6631},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/3/3Y", 871.03, -19.0907},
        {"4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/4/5Y", 871.03, -3832.59},
        {"4_Swap_JPY", "Up:FXSpot/EURJPY/0/spot", 871.03, -8.62406},
        {"4_Swap_JPY", "Down:FXSpot/EURJPY/0/spot", 871.03, 8.79829},
        {"5_Swaption_EUR", "Up:DiscountCurve/EUR/6/10Y", 37497.4, -10.0061},
        {"5_Swaption_EUR", "Up:DiscountCurve/EUR/7/15Y", 37497.4, -28.0689},
        {"5_Swaption_EUR", "Up:DiscountCurve/EUR/8/20Y", 37497.4, -17.5118},
        {"5_Swaption_EUR", "Down:DiscountCurve/EUR/6/10Y", 37497.4, 10.0128},
        {"5_Swaption_EUR", "Down:DiscountCurve/EUR/7/15Y", 37497.4, 28.0967},
        {"5_Swaption_EUR", "Down:DiscountCurve/EUR/8/20Y", 37497.4, 17.535},
        {"5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 37497.4, -395.217},
        {"5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/7/15Y", 37497.4, 56.7325},
        {"5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/8/20Y", 37497.4, 722.297},
        {"5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 37497.4, 397.912},
        {"5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/7/15Y", 37497.4, -56.5086},
        {"5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/8/20Y", 37497.4, -713.454},
        {"5_Swaption_EUR", "Up:SwaptionVolatility/EUR/5/10Y/10Y/ATM", 37497.4, 367.34},
        {"5_Swaption_EUR", "Down:SwaptionVolatility/EUR/5/10Y/10Y/ATM", 37497.4, -367.339},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/2/2Y", 10728, -0.485565},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/3/3Y", 10728, -1.08915},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/4/5Y", 10728, -1.98536},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/5/7Y", 10728, -0.589162},
        {"6_Swaption_EUR", "Up:DiscountCurve/EUR/6/10Y", 10728, 0.00671364},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/2/2Y", 10728, 0.485627},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/3/3Y", 10728, 1.08927},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/4/5Y", 10728, 1.9858},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/5/7Y", 10728, 0.589199},
        {"6_Swaption_EUR", "Down:DiscountCurve/EUR/6/10Y", 10728, -0.00671365},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", 10728, -97.3815},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", 10728, 4.02331},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", 10728, 8.90295},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", 10728, 322.905},
        {"6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 10728, 1.2365},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", 10728, 97.9503},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", 10728, -3.98884},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", 10728, -8.83939},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", 10728, -316.852},
        {"6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 10728, -1.23641},
        {"6_Swaption_EUR", "Up:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 10728, 102.403},
        {"6_Swaption_EUR", "Up:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 10728, 0.187171},
        {"6_Swaption_EUR", "Down:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 10728, -102.402},
        {"6_Swaption_EUR", "Down:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 10728, -0.187171},
        {"17_Swaption_EUR", "Up:DiscountCurve/EUR/2/2Y", 9574.97, -0.255216},
        {"17_Swaption_EUR", "Up:DiscountCurve/EUR/3/3Y", 9574.97, -1.08915},
        {"17_Swaption_EUR", "Up:DiscountCurve/EUR/4/5Y", 9574.97, -1.98536},
        {"17_Swaption_EUR", "Up:DiscountCurve/EUR/5/7Y", 9574.97, -0.589162},
        {"17_Swaption_EUR", "Up:DiscountCurve/EUR/6/10Y", 9574.97, 0.00671364},
        {"17_Swaption_EUR", "Down:DiscountCurve/EUR/2/2Y", 9574.97, 0.255232},
        {"17_Swaption_EUR", "Down:DiscountCurve/EUR/3/3Y", 9574.97, 1.08927},
        {"17_Swaption_EUR", "Down:DiscountCurve/EUR/4/5Y", 9574.97, 1.9858},
        {"17_Swaption_EUR", "Down:DiscountCurve/EUR/5/7Y", 9574.97, 0.589199},
        {"17_Swaption_EUR", "Down:DiscountCurve/EUR/6/10Y", 9574.97, -0.00671365},
        {"17_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", 9574.97, -97.3815},
        {"17_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", 9574.97, 4.02331},
        {"17_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", 9574.97, 8.90295},
        {"17_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", 9574.97, 322.905},
        {"17_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 9574.97, 1.2365},
        {"17_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", 9574.97, 97.9503},
        {"17_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", 9574.97, -3.98884},
        {"17_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", 9574.97, -8.83939},
        {"17_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", 9574.97, -316.852},
        {"17_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 9574.97, -1.23641},
        {"17_Swaption_EUR", "Up:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 9574.97, 102.403},
        {"17_Swaption_EUR", "Up:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 9574.97, 0.187171},
        {"17_Swaption_EUR", "Down:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 9574.97, -102.402},
        {"17_Swaption_EUR", "Down:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 9574.97, -0.187171},
        { "13_Swaption_EUR", "Up:DiscountCurve/EUR/2/2Y", 28897.73677078046239330, -0.27803008252885775 },
        { "13_Swaption_EUR", "Up:DiscountCurve/EUR/3/3Y", 28897.73677078046239330, -1.33378866274506436 },
        { "13_Swaption_EUR", "Up:DiscountCurve/EUR/4/5Y", 28897.73677078046239330, -3.18918880432465812 },
        { "13_Swaption_EUR", "Up:DiscountCurve/EUR/5/7Y", 28897.73677078046239330, -5.84492214726196835 },
        { "13_Swaption_EUR", "Up:DiscountCurve/EUR/6/10Y", 28897.73677078046239330, -8.05244884515923331 },
        { "13_Swaption_EUR", "Up:DiscountCurve/EUR/7/15Y", 28897.73677078046239330, -0.69943596490338678 },
        { "13_Swaption_EUR", "Down:DiscountCurve/EUR/2/2Y", 28897.73677078046239330, 0.27805747913225787 },
        { "13_Swaption_EUR", "Down:DiscountCurve/EUR/3/3Y", 28897.73677078046239330, 1.33400263515068218 },
        { "13_Swaption_EUR", "Down:DiscountCurve/EUR/4/5Y", 28897.73677078046239330, 3.19001306503196247 },
        { "13_Swaption_EUR", "Down:DiscountCurve/EUR/5/7Y", 28897.73677078046239330, 5.84723266483342741 },
        { "13_Swaption_EUR", "Down:DiscountCurve/EUR/6/10Y", 28897.73677078046239330, 8.05807870846547303 },
        { "13_Swaption_EUR", "Down:DiscountCurve/EUR/7/15Y", 28897.73677078046239330, 0.69933176309859846 },
        { "13_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", 28897.73677078046239330, -38.21252730777996476 },
        { "13_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", 28897.73677078046239330, -45.32662074925974593 },
        { "13_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", 28897.73677078046239330, -64.33186224827295518 },
        { "13_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", 28897.73677078046239330, -17.67819831141969189 },
        { "13_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 28897.73677078046239330, 303.22767876380385133 },
        { "13_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/7/15Y", 28897.73677078046239330, 284.38032909158937400 },
        { "13_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", 28897.73677078046239330, 38.21580821301176911 },
        { "13_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", 28897.73677078046239330, 45.32991632828270667 },
        { "13_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", 28897.73677078046239330, 66.17154485030550859 },
        { "13_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", 28897.73677078046239330, 18.94715080036257859 },
        { "13_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 28897.73677078046239330, -303.16555956740558031 },
        { "13_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/7/15Y", 28897.73677078046239330, -284.35056285505561391 },
        { "13_Swaption_EUR", "Up:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 28897.73677078046239330, 11.08070607382614980 },
        { "13_Swaption_EUR", "Up:SwaptionVolatility/EUR/1/2Y/10Y/ATM", 28897.73677078046239330, 76.78895257816475350 },
        { "13_Swaption_EUR", "Up:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 28897.73677078046239330, 109.40430923897656612 },
        { "13_Swaption_EUR", "Up:SwaptionVolatility/EUR/3/5Y/10Y/ATM", 28897.73677078046239330, 60.54663518636516528 },
        { "13_Swaption_EUR", "Up:SwaptionVolatility/EUR/4/10Y/5Y/ATM", 28897.73677078046239330, 18.72588467272362323 },
        { "13_Swaption_EUR", "Up:SwaptionVolatility/EUR/5/10Y/10Y/ATM", 28897.73677078046239330, 4.69121790060671628 },
        { "13_Swaption_EUR", "Down:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 28897.73677078046239330, -11.09549747305936762 },
        { "13_Swaption_EUR", "Down:SwaptionVolatility/EUR/1/2Y/10Y/ATM", 28897.73677078046239330, -77.54106330653303303 },
        { "13_Swaption_EUR", "Down:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 28897.73677078046239330, -109.65679842332247063 },
        { "13_Swaption_EUR", "Down:SwaptionVolatility/EUR/3/5Y/10Y/ATM", 28897.73677078046239330, -60.77103383964640670 },
        { "13_Swaption_EUR", "Down:SwaptionVolatility/EUR/4/10Y/5Y/ATM", 28897.73677078046239330, -18.79003667952929391 },
        { "13_Swaption_EUR", "Down:SwaptionVolatility/EUR/5/10Y/10Y/ATM", 28897.73677078046239330, -4.69902158074910403 },
        {"7_FxOption_EUR_USD", "Up:DiscountCurve/EUR/3/3Y", 1.36968e+06, -2107.81},
        {"7_FxOption_EUR_USD", "Up:DiscountCurve/EUR/4/5Y", 1.36968e+06, -3.85768},
        {"7_FxOption_EUR_USD", "Up:DiscountCurve/USD/3/3Y", 1.36968e+06, 1698.91},
        {"7_FxOption_EUR_USD", "Up:DiscountCurve/USD/4/5Y", 1.36968e+06, 3.10717},
        {"7_FxOption_EUR_USD", "Down:DiscountCurve/EUR/3/3Y", 1.36968e+06, 2109.74},
        {"7_FxOption_EUR_USD", "Down:DiscountCurve/EUR/4/5Y", 1.36968e+06, 3.85768},
        {"7_FxOption_EUR_USD", "Down:DiscountCurve/USD/3/3Y", 1.36968e+06, -1698.12},
        {"7_FxOption_EUR_USD", "Down:DiscountCurve/USD/4/5Y", 1.36968e+06, -3.10717},
        {"7_FxOption_EUR_USD", "Up:FXSpot/EURUSD/0/spot", 1.36968e+06, 56850.7},
        {"7_FxOption_EUR_USD", "Down:FXSpot/EURUSD/0/spot", 1.36968e+06, -56537.6},
        {"7_FxOption_EUR_USD", "Up:FXVolatility/EURUSD/0/5Y/ATM", 1.36968e+06, 672236},
        {"7_FxOption_EUR_USD", "Down:FXVolatility/EURUSD/0/5Y/ATM", 1.36968e+06, -329688},
        {"8_FxOption_EUR_GBP", "Up:DiscountCurve/EUR/5/7Y", 798336, -2435.22},
        {"8_FxOption_EUR_GBP", "Up:DiscountCurve/GBP/5/7Y", 798336, 1880.89},
        {"8_FxOption_EUR_GBP", "Down:DiscountCurve/EUR/5/7Y", 798336, 2441.08},
        {"8_FxOption_EUR_GBP", "Down:DiscountCurve/GBP/5/7Y", 798336, -1878.05},
        {"8_FxOption_EUR_GBP", "Up:FXSpot/EURGBP/0/spot", 798336, 27009.9},
        {"8_FxOption_EUR_GBP", "Down:FXSpot/EURGBP/0/spot", 798336, -26700.2},
        {"8_FxOption_EUR_GBP", "Up:FXVolatility/EURGBP/0/5Y/ATM", 798336, 1.36635e+06},
        {"8_FxOption_EUR_GBP", "Down:FXVolatility/EURGBP/0/5Y/ATM", 798336, -798336},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/2/2Y", 289.105, -7.28588e-07},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/3/3Y", 289.105, -0.000381869},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/4/5Y", 289.105, -0.00790528},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/5/7Y", 289.105, -0.0764893},
        {"9_Cap_EUR", "Up:DiscountCurve/EUR/6/10Y", 289.105, -0.162697},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/2/2Y", 289.105, 7.28664e-07},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/3/3Y", 289.105, 0.000381934},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/4/5Y", 289.105, 0.00790776},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/5/7Y", 289.105, 0.0765231},
        {"9_Cap_EUR", "Down:DiscountCurve/EUR/6/10Y", 289.105, 0.162824},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/1/1Y", 289.105, -1.81582e-05},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", 289.105, -0.00670729},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", 289.105, -0.330895},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", 289.105, -2.03937},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", 289.105, -6.42991},
        {"9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 289.105, 15.5182},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/1/1Y", 289.105, 1.97218e-05},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", 289.105, 0.00746096},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", 289.105, 0.353405},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", 289.105, 2.24481},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", 289.105, 7.1522},
        {"9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 289.105, -14.6675},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/4/1Y/0.05", 289.105, 8.49293e-05},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/9/2Y/0.05", 289.105, 0.0150901},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/14/3Y/0.05", 289.105, 0.620393},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/19/5Y/0.05", 289.105, 17.2057},
        {"9_Cap_EUR", "Up:OptionletVolatility/EUR/24/10Y/0.05", 289.105, 24.4267},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/4/1Y/0.05", 289.105, -6.97789e-05},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/9/2Y/0.05", 289.105, -0.0125099},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/14/3Y/0.05", 289.105, -0.554344},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/19/5Y/0.05", 289.105, -16.1212},
        {"9_Cap_EUR", "Down:OptionletVolatility/EUR/24/10Y/0.05", 289.105, -23.0264},
        {"10_Floor_USD", "Up:DiscountCurve/USD/0/6M", 3406.46, -7.03494e-09},
        {"10_Floor_USD", "Up:DiscountCurve/USD/1/1Y", 3406.46, -8.41429e-05},
        {"10_Floor_USD", "Up:DiscountCurve/USD/2/2Y", 3406.46, -0.00329744},
        {"10_Floor_USD", "Up:DiscountCurve/USD/3/3Y", 3406.46, -0.053884},
        {"10_Floor_USD", "Up:DiscountCurve/USD/4/5Y", 3406.46, -0.269714},
        {"10_Floor_USD", "Up:DiscountCurve/USD/5/7Y", 3406.46, -0.989583},
        {"10_Floor_USD", "Up:DiscountCurve/USD/6/10Y", 3406.46, -1.26544},
        {"10_Floor_USD", "Down:DiscountCurve/USD/0/6M", 3406.46, 7.0354e-09},
        {"10_Floor_USD", "Down:DiscountCurve/USD/1/1Y", 3406.46, 8.41464e-05},
        {"10_Floor_USD", "Down:DiscountCurve/USD/2/2Y", 3406.46, 0.00329786},
        {"10_Floor_USD", "Down:DiscountCurve/USD/3/3Y", 3406.46, 0.0538949},
        {"10_Floor_USD", "Down:DiscountCurve/USD/4/5Y", 3406.46, 0.269802},
        {"10_Floor_USD", "Down:DiscountCurve/USD/5/7Y", 3406.46, 0.990038},
        {"10_Floor_USD", "Down:DiscountCurve/USD/6/10Y", 3406.46, 1.26635},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/0/6M", 3406.46, 0.00150733},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/1/1Y", 3406.46, 0.240284},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/2/2Y", 3406.46, 2.17175},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/3/3Y", 3406.46, 7.77249},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/4/5Y", 3406.46, 12.9642},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/5/7Y", 3406.46, 16.8269},
        {"10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/6/10Y", 3406.46, -81.4363},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/0/6M", 3406.46, -0.00139804},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/1/1Y", 3406.46, -0.230558},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/2/2Y", 3406.46, -2.00123},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/3/3Y", 3406.46, -7.14862},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/4/5Y", 3406.46, -11.2003},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/5/7Y", 3406.46, -13.7183},
        {"10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/6/10Y", 3406.46, 84.0113},
        {"10_Floor_USD", "Up:FXSpot/EURUSD/0/spot", 3406.46, -33.7273},
        {"10_Floor_USD", "Down:FXSpot/EURUSD/0/spot", 3406.46, 34.4087},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/0/1Y/0.01", 3406.46, 0.402913},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/5/2Y/0.01", 3406.46, 3.32861},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/10/3Y/0.01", 3406.46, 16.8798},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/15/5Y/0.01", 3406.46, 96.415},
        {"10_Floor_USD", "Up:OptionletVolatility/USD/20/10Y/0.01", 3406.46, 92.2212},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/0/1Y/0.01", 3406.46, -0.37428},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/5/2Y/0.01", 3406.46, -3.14445},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/10/3Y/0.01", 3406.46, -16.3074},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/15/5Y/0.01", 3406.46, -94.5309},
        {"10_Floor_USD", "Down:OptionletVolatility/USD/20/10Y/0.01", 3406.46, -90.9303},
        // Excel calculation with z=5% flat rate, term structure day counter ActAct,
        // time to maturity T = YEARFRAC(14/4/16, 14/4/26, 1) = 9.99800896, yields
        // sensi to up shift d=1bp: exp(-(z+d)*T)-exp(z*T)
        // = -0.00060616719559925
        {"11_ZeroBond_EUR", "Up:YieldCurve/BondCurve0/6/10Y", 0.60659, -0.000606168}, // OK, diff 1e-9
        // sensi to down shift d=-1bp: 0.00060677354516836
        {"11_ZeroBond_EUR", "Down:YieldCurve/BondCurve0/6/10Y", 0.60659, 0.000606774}, // OK, diff < 1e-9
        // A relative shift in yield curve is equivalent to a relative shift in default curve
        {"11_ZeroBond_EUR", "Up:SurvivalProbability/BondIssuer0/6/10Y", 0.60659, -0.000606168},
        {"11_ZeroBond_EUR", "Down:SurvivalProbability/BondIssuer0/6/10Y", 0.60659, 0.000606774},
        // sensi to up shift d=+1bp: exp(-(z+d)*T)*USDEUR - exp(-z*T)*USDEUR
        // = -0.000505139329666004
        {"12_ZeroBond_USD", "Up:YieldCurve/BondCurve0/6/10Y", 0.505492, -0.00050514}, // OK, diff < 1e-8
        // sensi to down shift d=-1bp: 0.000505644620973689
        {"12_ZeroBond_USD", "Down:YieldCurve/BondCurve0/6/10Y", 0.505492, 0.000505645}, // OK, diff < 1e-9
        // A relative shift in yield curve is equivalent to a relative shift in default curve
        {"12_ZeroBond_USD", "Up:SurvivalProbability/BondIssuer0/6/10Y", 0.505492, -0.00050514},
        {"12_ZeroBond_USD", "Down:SurvivalProbability/BondIssuer0/6/10Y", 0.505492, 0.000505645},
        // sensi to EURUSD upshift d=+1%: exp(-z*T)*USDEUR/(1+d) - exp(-z*T)*USDEUR
        // = -0.00500487660122262
        {"12_ZeroBond_USD", "Up:FXSpot/EURUSD/0/spot", 0.505492, -0.00500487}, // OK, diff < 1e-8
        // sensi to EURUSD down shift d=-1%: 0.00510598521942907
        {"12_ZeroBond_USD", "Down:FXSpot/EURUSD/0/spot", 0.505492, 0.00510598}, // OK, diff < 1e-8
        {"14_EquityOption_SP5", "Up:DiscountCurve/USD/2/2Y", 216085, 123.022},
        {"14_EquityOption_SP5", "Up:DiscountCurve/USD/3/3Y", 216085, 1.0169},
        {"14_EquityOption_SP5", "Down:DiscountCurve/USD/2/2Y", 216085, -122.988},
        {"14_EquityOption_SP5", "Down:DiscountCurve/USD/3/3Y", 216085, -1.0169},
        {"14_EquityOption_SP5", "Up:EquitySpot/SP5/0/spot", 216085, 8423.66},
        {"14_EquityOption_SP5", "Down:EquitySpot/SP5/0/spot", 216085, -8277.55},
        {"14_EquityOption_SP5", "Up:FXSpot/EURUSD/0/spot", 216085, -2139.45},
        {"14_EquityOption_SP5", "Down:FXSpot/EURUSD/0/spot", 216085, 2182.67},
        {"14_EquityOption_SP5", "Up:EquityVolatility/SP5/0/5Y/ATM", 216085, 1849.98},
        {"14_EquityOption_SP5", "Down:EquityVolatility/SP5/0/5Y/ATM", 216085, -1850.33},
        {"15_CPIInflationSwap_UKRPI", "Up:DiscountCurve/GBP/0/6M", -32068.5, -0.0306304},
        {"15_CPIInflationSwap_UKRPI", "Up:DiscountCurve/GBP/1/1Y", -32068.5, -0.279201},
        {"15_CPIInflationSwap_UKRPI", "Up:DiscountCurve/GBP/2/2Y", -32068.5, -0.772336},
        {"15_CPIInflationSwap_UKRPI", "Up:DiscountCurve/GBP/3/3Y", -32068.5, -1.80941},
        {"15_CPIInflationSwap_UKRPI", "Up:DiscountCurve/GBP/4/5Y", -32068.5, -3.18149},
        {"15_CPIInflationSwap_UKRPI", "Up:DiscountCurve/GBP/5/7Y", -32068.5, -5.26791},
        {"15_CPIInflationSwap_UKRPI", "Up:DiscountCurve/GBP/6/10Y", -32068.5, 58.9998},
        {"15_CPIInflationSwap_UKRPI", "Down:DiscountCurve/GBP/0/6M", -32068.5, 0.030632},
        {"15_CPIInflationSwap_UKRPI", "Down:DiscountCurve/GBP/1/1Y", -32068.5, 0.279223},
        {"15_CPIInflationSwap_UKRPI", "Down:DiscountCurve/GBP/2/2Y", -32068.5, 0.772443},
        {"15_CPIInflationSwap_UKRPI", "Down:DiscountCurve/GBP/3/3Y", -32068.5, 1.8098},
        {"15_CPIInflationSwap_UKRPI", "Down:DiscountCurve/GBP/4/5Y", -32068.5, 3.18254},
        {"15_CPIInflationSwap_UKRPI", "Down:DiscountCurve/GBP/5/7Y", -32068.5, 5.27039},
        {"15_CPIInflationSwap_UKRPI", "Down:DiscountCurve/GBP/6/10Y", -32068.5, -59.0602},
        {"15_CPIInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/0/6M", -32068.5, -6.17897},
        {"15_CPIInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/1/1Y", -32068.5, 0.672814},
        {"15_CPIInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/2/2Y", -32068.5, 0.804723},
        {"15_CPIInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/3/3Y", -32068.5, 2.4176},
        {"15_CPIInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/4/5Y", -32068.5, 3.61554},
        {"15_CPIInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/5/7Y", -32068.5, 6.77412},
        {"15_CPIInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/6/10Y", -32068.5, 89.6542},
        {"15_CPIInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/0/6M", -32068.5, 6.17927},
        {"15_CPIInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/1/1Y", -32068.5, -0.671026},
        {"15_CPIInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/2/2Y", -32068.5, -0.80017},
        {"15_CPIInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/3/3Y", -32068.5, -2.40996},
        {"15_CPIInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/4/5Y", -32068.5, -3.60255},
        {"15_CPIInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/5/7Y", -32068.5, -6.75478},
        {"15_CPIInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/6/10Y", -32068.5, -89.6393},
        {"15_CPIInflationSwap_UKRPI", "Up:FXSpot/EURGBP/0/spot", -32068.5, 317.51},
        {"15_CPIInflationSwap_UKRPI", "Down:FXSpot/EURGBP/0/spot", -32068.5, -323.924},
        {"15_CPIInflationSwap_UKRPI", "Up:ZeroInflationCurve/UKRPI/0/1Y", -32068.5, -0.0789981},
        {"15_CPIInflationSwap_UKRPI", "Up:ZeroInflationCurve/UKRPI/1/2Y", -32068.5, -0.154098},
        {"15_CPIInflationSwap_UKRPI", "Up:ZeroInflationCurve/UKRPI/2/3Y", -32068.5, -0.381073},
        {"15_CPIInflationSwap_UKRPI", "Up:ZeroInflationCurve/UKRPI/3/5Y", -32068.5, -0.749769},
        {"15_CPIInflationSwap_UKRPI", "Up:ZeroInflationCurve/UKRPI/4/7Y", -32068.5, -1.34474},
        {"15_CPIInflationSwap_UKRPI", "Up:ZeroInflationCurve/UKRPI/5/10Y", -32068.5, -144.53},
        {"15_CPIInflationSwap_UKRPI", "Down:ZeroInflationCurve/UKRPI/0/1Y", -32068.5, 0.0789981},
        {"15_CPIInflationSwap_UKRPI", "Down:ZeroInflationCurve/UKRPI/1/2Y", -32068.5, 0.154083},
        {"15_CPIInflationSwap_UKRPI", "Down:ZeroInflationCurve/UKRPI/2/3Y", -32068.5, 0.381006},
        {"15_CPIInflationSwap_UKRPI", "Down:ZeroInflationCurve/UKRPI/3/5Y", -32068.5, 0.749547},
        {"15_CPIInflationSwap_UKRPI", "Down:ZeroInflationCurve/UKRPI/4/7Y", -32068.5, 1.34416},
        {"15_CPIInflationSwap_UKRPI", "Down:ZeroInflationCurve/UKRPI/5/10Y", -32068.5, 144.404},
        {"16_YoYInflationSwap_UKRPI", "Up:DiscountCurve/GBP/1/1Y", 7005.96, 0.232259},
        {"16_YoYInflationSwap_UKRPI", "Up:DiscountCurve/GBP/2/2Y", 7005.96, -0.239315},
        {"16_YoYInflationSwap_UKRPI", "Up:DiscountCurve/GBP/3/3Y", 7005.96, -0.583046},
        {"16_YoYInflationSwap_UKRPI", "Up:DiscountCurve/GBP/4/5Y", 7005.96, -1.00199},
        {"16_YoYInflationSwap_UKRPI", "Up:DiscountCurve/GBP/5/7Y", 7005.96, -1.72218},
        {"16_YoYInflationSwap_UKRPI", "Up:DiscountCurve/GBP/6/10Y", 7005.96, -1.79744},
        {"16_YoYInflationSwap_UKRPI", "Down:DiscountCurve/GBP/1/1Y", 7005.96, -0.232282},
        {"16_YoYInflationSwap_UKRPI", "Down:DiscountCurve/GBP/2/2Y", 7005.96, 0.239363},
        {"16_YoYInflationSwap_UKRPI", "Down:DiscountCurve/GBP/3/3Y", 7005.96, 0.583198},
        {"16_YoYInflationSwap_UKRPI", "Down:DiscountCurve/GBP/4/5Y", 7005.96, 1.00236},
        {"16_YoYInflationSwap_UKRPI", "Down:DiscountCurve/GBP/5/7Y", 7005.96, 1.72305},
        {"16_YoYInflationSwap_UKRPI", "Down:DiscountCurve/GBP/6/10Y", 7005.96, 1.79882},
        {"16_YoYInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/0/6M", 7005.96, -0.0656954},
        {"16_YoYInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/1/1Y", 7005.96, -11.785},
        {"16_YoYInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/2/2Y", 7005.96, 0.816056},
        {"16_YoYInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/3/3Y", 7005.96, 2.44319},
        {"16_YoYInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/4/5Y", 7005.96, 3.66156},
        {"16_YoYInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/5/7Y", 7005.96, 6.85113},
        {"16_YoYInflationSwap_UKRPI", "Up:IndexCurve/GBP-LIBOR-6M/6/10Y", 7005.96, 90.5575},
        {"16_YoYInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/0/6M", 7005.96, 0.0656954},
        {"16_YoYInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/1/1Y", 7005.96, 11.7862},
        {"16_YoYInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/2/2Y", 7005.96, -0.80686},
        {"16_YoYInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/3/3Y", 7005.96, -2.42775},
        {"16_YoYInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/4/5Y", 7005.96, -3.63532},
        {"16_YoYInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/5/7Y", 7005.96, -6.81206},
        {"16_YoYInflationSwap_UKRPI", "Down:IndexCurve/GBP-LIBOR-6M/6/10Y", 7005.96, -90.5274},
        {"16_YoYInflationSwap_UKRPI", "Up:FXSpot/EURGBP/0/spot", 7005.96, -69.3659},
        {"16_YoYInflationSwap_UKRPI", "Down:FXSpot/EURGBP/0/spot", 7005.96, 70.7673},
        {"16_YoYInflationSwap_UKRPI", "Up:YoYInflationCurve/UKRPI/0/1Y", 7005.96, -12.1136},
        {"16_YoYInflationSwap_UKRPI", "Up:YoYInflationCurve/UKRPI/1/2Y", 7005.96, -11.4741},
        {"16_YoYInflationSwap_UKRPI", "Up:YoYInflationCurve/UKRPI/2/3Y", 7005.96, -16.3788},
        {"16_YoYInflationSwap_UKRPI", "Up:YoYInflationCurve/UKRPI/3/5Y", 7005.96, -20.4522},
        {"16_YoYInflationSwap_UKRPI", "Up:YoYInflationCurve/UKRPI/4/7Y", 7005.96, -23.3381},
        {"16_YoYInflationSwap_UKRPI", "Up:YoYInflationCurve/UKRPI/5/10Y", 7005.96, -17.2056},
        {"16_YoYInflationSwap_UKRPI", "Down:YoYInflationCurve/UKRPI/0/1Y", 7005.96, 12.1136},
        {"16_YoYInflationSwap_UKRPI", "Down:YoYInflationCurve/UKRPI/1/2Y", 7005.96, 11.4741},
        {"16_YoYInflationSwap_UKRPI", "Down:YoYInflationCurve/UKRPI/2/3Y", 7005.96, 16.3788},
        {"16_YoYInflationSwap_UKRPI", "Down:YoYInflationCurve/UKRPI/3/5Y", 7005.96, 20.4522},
        {"16_YoYInflationSwap_UKRPI", "Down:YoYInflationCurve/UKRPI/4/7Y", 7005.96, 23.3381},
        {"16_YoYInflationSwap_UKRPI", "Down:YoYInflationCurve/UKRPI/5/10Y", 7005.96, 17.2056},
        {"17_CommodityForward_GOLD", "Up:DiscountCurve/USD/1/1Y", -735.964496751649, 0.073448445224},
        {"17_CommodityForward_GOLD", "Down:DiscountCurve/USD/1/1Y", -735.964496751649, -0.073455776029},
        {"17_CommodityForward_GOLD", "Up:FXSpot/EURUSD/0/spot", -735.964496751649, 7.286777195561},
        {"17_CommodityForward_GOLD", "Down:FXSpot/EURUSD/0/spot", -735.964496751649, -7.433984815673},
        {"17_CommodityForward_GOLD", "Up:CommodityCurve/COMDTY_GOLD_USD/1/1Y", -735.964496751649, 938.880422284606},
        {"17_CommodityForward_GOLD", "Down:CommodityCurve/COMDTY_GOLD_USD/1/1Y", -735.964496751649, -938.880422284606},
        {"18_CommodityForward_OIL", "Up:DiscountCurve/USD/3/3Y", -118575.997564574063, 23.666326609469},
        {"18_CommodityForward_OIL", "Up:DiscountCurve/USD/4/5Y", -118575.997564574063, 23.759329674402},
        {"18_CommodityForward_OIL", "Down:DiscountCurve/USD/3/3Y", -118575.997564574063, -23.671051063342},
        {"18_CommodityForward_OIL", "Down:DiscountCurve/USD/4/5Y", -118575.997564574063, -23.764091336881},
        {"18_CommodityForward_OIL", "Up:FXSpot/EURUSD/0/spot", -118575.997564574063, 1174.019777867070},
        {"18_CommodityForward_OIL", "Down:FXSpot/EURUSD/0/spot", -118575.997564574063, -1197.737349137125},
        {"18_CommodityForward_OIL", "Up:CommodityCurve/COMDTY_WTI_USD/2/2Y", -118575.997564574063, -10938.550513848924},
        {"18_CommodityForward_OIL", "Up:CommodityCurve/COMDTY_WTI_USD/3/5Y", -118575.997564574063, -24245.826202620548},
        {"18_CommodityForward_OIL", "Down:CommodityCurve/COMDTY_WTI_USD/2/2Y", -118575.997564574063,
         10938.550513849448},
        {"18_CommodityForward_OIL", "Down:CommodityCurve/COMDTY_WTI_USD/3/5Y", -118575.997564574063,
         24245.826202621072},
        {"19_CommodityOption_GOLD", "Up:DiscountCurve/USD/1/1Y", 5266.437412224631, -0.516232985022},
        {"19_CommodityOption_GOLD", "Up:DiscountCurve/USD/2/2Y", 5266.437412224631, -0.018723533876},
        {"19_CommodityOption_GOLD", "Down:DiscountCurve/USD/1/1Y", 5266.437412224631, 0.516283587557},
        {"19_CommodityOption_GOLD", "Down:DiscountCurve/USD/2/2Y", 5266.437412224631, 0.018723579571},
        {"19_CommodityOption_GOLD", "Up:FXSpot/EURUSD/0/spot", 5266.437412224631, -52.142944675492},
        {"19_CommodityOption_GOLD", "Down:FXSpot/EURUSD/0/spot", 5266.437412224631, 53.196337497218},
        {"19_CommodityOption_GOLD", "Up:CommodityCurve/COMDTY_GOLD_USD/1/1Y", 5266.437412224631, 490.253537097216},
        {"19_CommodityOption_GOLD", "Down:CommodityCurve/COMDTY_GOLD_USD/1/1Y", 5266.437412224631, -465.274919275530},
        {"19_CommodityOption_GOLD", "Up:CommodityVolatility/COMDTY_GOLD_USD/6/1Y/ATM", 5266.437412224631,
         56.110511491685},
        {"19_CommodityOption_GOLD", "Down:CommodityVolatility/COMDTY_GOLD_USD/6/1Y/ATM", 5266.437412224631,
         -56.112114940141},
        {"20_CommodityOption_OIL", "Up:DiscountCurve/USD/3/3Y", -491152.228798501019, 98.775116046891},
        {"20_CommodityOption_OIL", "Up:DiscountCurve/USD/4/5Y", -491152.228798501019, 97.292577287881},
        {"20_CommodityOption_OIL", "Down:DiscountCurve/USD/3/3Y", -491152.228798501019, -98.794984069362},
        {"20_CommodityOption_OIL", "Down:DiscountCurve/USD/4/5Y", -491152.228798501019, -97.311852635990},
        {"20_CommodityOption_OIL", "Up:FXSpot/EURUSD/0/spot", -491152.228798501019, 4862.893354440632},
        {"20_CommodityOption_OIL", "Down:FXSpot/EURUSD/0/spot", -491152.228798501019, -4961.133624227310},
        {"20_CommodityOption_OIL", "Up:CommodityCurve/COMDTY_WTI_USD/2/2Y", -491152.228798501019, 4223.515679404372},
        {"20_CommodityOption_OIL", "Up:CommodityCurve/COMDTY_WTI_USD/3/5Y", -491152.228798501019, 9317.978340855800},
        {"20_CommodityOption_OIL", "Down:CommodityCurve/COMDTY_WTI_USD/2/2Y", -491152.228798501019, -4256.075631047075},
        {"20_CommodityOption_OIL", "Down:CommodityCurve/COMDTY_WTI_USD/3/5Y", -491152.228798501019, -9477.947397496144},
        { "20_CommodityOption_OIL", "Up:CommodityVolatility/COMDTY_WTI_USD/3/1Y/0.95", -491152.228798501019, -169.914415647450 },
        { "20_CommodityOption_OIL", "Up:CommodityVolatility/COMDTY_WTI_USD/6/1Y/ATM", -491152.228798501019, -167.260480643541 },
        { "20_CommodityOption_OIL", "Up:CommodityVolatility/COMDTY_WTI_USD/5/5Y/0.95", -491152.228798501019, -2553.579689398874 },
        { "20_CommodityOption_OIL", "Up:CommodityVolatility/COMDTY_WTI_USD/8/5Y/ATM", -491152.228798501019, -2513.783958086802 },
        { "20_CommodityOption_OIL", "Down:CommodityVolatility/COMDTY_WTI_USD/3/1Y/0.95", -491152.228798501019, 168.278235032340 },
        { "20_CommodityOption_OIL", "Down:CommodityVolatility/COMDTY_WTI_USD/6/1Y/ATM", -491152.228798501019, 165.649017560529 },
        { "20_CommodityOption_OIL", "Down:CommodityVolatility/COMDTY_WTI_USD/5/5Y/0.95", -491152.228798501019, 2540.538653619646 },
        { "20_CommodityOption_OIL", "Down:CommodityVolatility/COMDTY_WTI_USD/8/5Y/ATM", -491152.228798501019, 2500.755505821493 }};
    // clang-format on

    std::map<pair<string, string>, Real> npvMap, sensiMap;
    std::set<pair<string, string>> coveredSensis;
    for (Size i = 0; i < cachedResults.size(); ++i) {
        pair<string, string> p(cachedResults[i].id, cachedResults[i].label);
        npvMap[p] = cachedResults[i].npv;
        sensiMap[p] = cachedResults[i].sensi;
    }

    Real tiny = 1.0e-10;
    Real tolerance = 0.01;
    Size count = 0;
    vector<ShiftScenarioGenerator::ScenarioDescription> desc = scenarioGenerator->scenarioDescriptions();
    size_t currentTradeIdx = 0;
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        Real npv0 = cube->getT0(currentTradeIdx, 0);
        for (Size j = 1; j < scenarioGenerator->samples(); ++j) { // skip j = 0, this is the base scenario
            Real npv = cube->get(currentTradeIdx, 0, j, 0);
            Real sensi = npv - npv0;
            string label = to_string(desc[j]);
            if (fabs(sensi) > tiny) {
                count++;
                BOOST_TEST_MESSAGE("{ \"" << tradeId << "\", \"" << label << "\", " << std::fixed
                                          << std::setprecision(12) << npv0 << ", " << sensi << " },");
                pair<string, string> p(tradeId, label);
                QL_REQUIRE(npvMap.find(p) != npvMap.end(),
                           "pair (" << p.first << ", " << p.second << ") not found in npv map");
                QL_REQUIRE(sensiMap.find(p) != sensiMap.end(),
                           "pair (" << p.first << ", " << p.second << ") not found in sensi map");
                BOOST_CHECK_MESSAGE(fabs(npv0 - npvMap[p]) < tolerance || fabs((npv0 - npvMap[p]) / npv0) < tolerance,
                                    "npv regression failed for pair (" << p.first << ", " << p.second << "): " << npv0
                                                                       << " vs " << npvMap[p]);
                BOOST_CHECK_MESSAGE(fabs(sensi - sensiMap[p]) < tolerance ||
                                        fabs((sensi - sensiMap[p]) / sensi) < tolerance,
                                    "sensitivity regression failed for pair ("
                                        << p.first << ", " << p.second << "): " << sensi << " vs " << sensiMap[p]);
                coveredSensis.insert(p);
            }
        }
        currentTradeIdx++;
    }
    currentTradeIdx = 0;
    BOOST_CHECK_MESSAGE(count == cachedResults.size(), "number of non-zero sensitivities ("
                                                           << count << ") do not match regression data ("
                                                           << cachedResults.size() << ")");
    for (auto const& p : sensiMap) {
        if (coveredSensis.find(p.first) == coveredSensis.end()) {
            BOOST_TEST_MESSAGE("sensi in expected, but not in calculated results: " << p.first.first << " "
                                                                                    << p.first.second);
        }
    }

    // Repeat analysis using the SensitivityAnalysis class and spot check a few deltas and gammas
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, false);
    sa->generateSensitivities();
    map<pair<string, string>, Real> deltaMap;
    map<pair<string, string>, Real> gammaMap;
    std::set<string> sensiTrades;
    for (const auto& [pid, p] : portfolio->trades()) {
        sensiTrades.insert(pid);
        for (const auto& f : sa->sensiCube()->factors()) {
            auto des = sa->sensiCube()->factorDescription(f);
            deltaMap[make_pair(pid, des)] = sa->sensiCube()->delta(pid, f);
            gammaMap[make_pair(pid, des)] = sa->sensiCube()->gamma(pid, f);
        }
    }

    std::vector<Results> cachedResults2 = {
        // trade, factor, delta, gamma
        {"11_ZeroBond_EUR", "YieldCurve/BondCurve0/6/10Y", -0.000606168, 6.06352e-07}, // gamma OK see case 1 below
        {"12_ZeroBond_USD", "YieldCurve/BondCurve0/6/10Y", -0.00050514, 5.05294e-07},  // gamma OK, see case 2 below
        {"12_ZeroBond_USD", "FXSpot/EURUSD/0/spot", -0.00500487, 0.000101108}          // gamma OK, see case 3
    };

    // Validation of cached gammas:
    // gamma * (dx)^2 = \partial^2_x NPV(x) * (dx)^2
    //               \approx (NPV(x_up) - 2 NPV(x) + NPV(x_down)) = sensi(up) + sensi(down)
    // Case 1: "11_ZeroBond_EUR", "YieldCurve/BondCurve1/6/10Y"
    // NPV(x_up) - NPV(x) = -0.000606168, NPV(x_down) - NPV(x) = 0.000606774
    // gamma * (dx)^2 = -0.000606168 + 0.000606774 = 0.000000606 = 6.06e-7
    //
    // Case 2: "12_ZeroBond_USD", "YieldCurve/BondCurve1/6/10Y"
    // NPV(x_up) - NPV(x) = -0.00050514, NPV(x_down) - NPV(x) = 0.000505645
    // gamma * (dx)^2 =  -0.00050514 + 0.000505645 = 0.000000505 = 5.05e-7
    //
    // Case 3: "12_ZeroBond_USD", "FXSpot/EURUSD/0/spot"
    // NPV(x_up) - NPV(x) = -0.00500487, NPV(x_down) - NPV(x) = 0.00510598
    // gamma * (dx)^2 =  -0.00500487 + 0.00510598 = 0.00010111
    //
    for (Size i = 0; i < cachedResults2.size(); ++i) {
        pair<string, string> p(cachedResults2[i].id, cachedResults2[i].label);
        Real delta = cachedResults2[i].npv;   // is delta
        Real gamma = cachedResults2[i].sensi; // is gamma
        BOOST_CHECK_MESSAGE(fabs((delta - deltaMap[p]) / delta) < tolerance,
                            "delta regression failed for trade " << p.first << " factor " << p.second << ", cached="
                                                                 << delta << ", computed=" << deltaMap[p]);
        BOOST_CHECK_MESSAGE(fabs((gamma - gammaMap[p]) / gamma) < tolerance,
                            "gamma regression failed for trade " << p.first << " factor " << p.second << ", cached="
                                                                 << gamma << ", computed=" << gammaMap[p]);
    }

    BOOST_TEST_MESSAGE("Cube generated in " << t.format(default_places, "%w") << " seconds");
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SensitivityAnalysisTest)

BOOST_AUTO_TEST_CASE(testPortfolioSensitivityNoneObs) {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity (None observation mode)");
    testPortfolioSensitivity(ObservationMode::Mode::None);
}

BOOST_AUTO_TEST_CASE(testPortfolioSensitivityDisableObs) {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity (Disable observation mode)");
    testPortfolioSensitivity(ObservationMode::Mode::Disable);
}

BOOST_AUTO_TEST_CASE(testPortfolioSensitivityDeferObs) {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity (Defer observation mode)");
    testPortfolioSensitivity(ObservationMode::Mode::Defer);
}

BOOST_AUTO_TEST_CASE(testPortfolioSensitivityUnregisterObs) {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity (Unregister observation mode)");
    testPortfolioSensitivity(ObservationMode::Mode::Unregister);
}

void test1dShifts(bool granular) {
    BOOST_TEST_MESSAGE("Testing 1d shifts " << (granular ? "granular" : "sparse"));

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData =
        TestConfigurationObjects::setupSimMarketData2();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData;
    if (granular)
        sensiData = TestConfigurationObjects::setupSensitivityScenarioData2b();
    else
        sensiData = TestConfigurationObjects::setupSensitivityScenarioData2();

    // build sim market
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);

    // cache initial zero rates
    vector<Period> tenors = simMarketData->yieldCurveTenors("");
    vector<Real> initialZeros(tenors.size());
    vector<Real> times(tenors.size());
    string ccy = simMarketData->ccys()[0];
    Handle<YieldTermStructure> ts = initMarket->discountCurve(ccy);
    DayCounter dc = ts->dayCounter();
    for (Size j = 0; j < tenors.size(); ++j) {
        Date d = today + simMarketData->yieldCurveTenors("")[j];
        initialZeros[j] = ts->zeroRate(d, dc, Continuous);
        times[j] = dc.yearFraction(today, d);
    }

    // apply zero shifts for tenors on the shift curve
    // collect shifted data at tenors of the underlying curve
    // aggregate "observed" shifts
    // compare to expected total shifts
    vector<Period> shiftTenors = sensiData->discountCurveShiftData()["EUR"]->shiftTenors;
    vector<Time> shiftTimes(shiftTenors.size());
    for (Size i = 0; i < shiftTenors.size(); ++i)
        shiftTimes[i] = dc.yearFraction(today, today + shiftTenors[i]);

    vector<Real> shiftedZeros(tenors.size());
    vector<Real> diffAbsolute(tenors.size(), 0.0);
    vector<Real> diffRelative(tenors.size(), 0.0);
    Real shiftSize = 0.01;
    ShiftType shiftTypeAbsolute = ShiftType::Absolute;
    ShiftType shiftTypeRelative = ShiftType::Relative;
    for (Size i = 0; i < shiftTenors.size(); ++i) {
        scenarioGenerator->applyShift(i, shiftSize, true, shiftTypeAbsolute, shiftTimes, initialZeros, times,
                                      shiftedZeros, true);
        for (Size j = 0; j < tenors.size(); ++j)
            diffAbsolute[j] += shiftedZeros[j] - initialZeros[j];
        scenarioGenerator->applyShift(i, shiftSize, true, shiftTypeRelative, shiftTimes, initialZeros, times,
                                      shiftedZeros, true);
        for (Size j = 0; j < tenors.size(); ++j)
            diffRelative[j] += shiftedZeros[j] / initialZeros[j] - 1.0;
    }

    Real tolerance = 1.0e-10;
    for (Size j = 0; j < tenors.size(); ++j) {
        // BOOST_TEST_MESSAGE("1d shift: checking sensitivity to curve tenor point " << j << ": " << diff[j]);
        BOOST_CHECK_MESSAGE(fabs(diffAbsolute[j] - shiftSize) < tolerance,
                            "inconsistency in absolute 1d shifts at curve tenor point " << j);
        BOOST_CHECK_MESSAGE(fabs(diffRelative[j] - shiftSize) < tolerance,
                            "inconsistency in relative 1d shifts at curve tenor point " << j);
    }
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

BOOST_AUTO_TEST_CASE(test1dShiftsSparse) { test1dShifts(false); }

BOOST_AUTO_TEST_CASE(test1dShiftsGranular) { test1dShifts(true); }

BOOST_AUTO_TEST_CASE(test2dShifts) {
    BOOST_TEST_MESSAGE("Testing 2d shifts");

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData =
        TestConfigurationObjects::setupSimMarketData2();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = TestConfigurationObjects::setupSensitivityScenarioData2();

    // build sim market
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);

    // cache initial zero rates
    vector<Period> expiries = simMarketData->swapVolExpiries("");
    vector<Period> terms = simMarketData->swapVolTerms("");
    vector<vector<Real>> initialData(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<Real> expiryTimes(expiries.size());
    vector<Real> termTimes(terms.size());
    string ccy = simMarketData->ccys()[0];
    Handle<SwaptionVolatilityStructure> ts = initMarket->swaptionVol(ccy);
    DayCounter dc = ts->dayCounter();
    for (Size i = 0; i < expiries.size(); ++i)
        expiryTimes[i] = dc.yearFraction(today, today + expiries[i]);
    for (Size j = 0; j < terms.size(); ++j)
        termTimes[j] = dc.yearFraction(today, today + terms[j]);
    for (Size i = 0; i < expiries.size(); ++i) {
        for (Size j = 0; j < terms.size(); ++j)
            initialData[i][j] = ts->volatility(expiries[i], terms[j], Null<Real>()); // ATM
    }

    // apply shifts for tenors on the 2d shift grid
    // collect shifted data at tenors of the underlying 2d grid (different from the grid above)
    // aggregate "observed" shifts
    // compare to expected total shifts
    vector<Period> expiryShiftTenors = sensiData->swaptionVolShiftData()["EUR"].shiftExpiries;
    vector<Period> termShiftTenors = sensiData->swaptionVolShiftData()["EUR"].shiftTerms;
    vector<Real> shiftExpiryTimes(expiryShiftTenors.size());
    vector<Real> shiftTermTimes(termShiftTenors.size());
    for (Size i = 0; i < expiryShiftTenors.size(); ++i)
        shiftExpiryTimes[i] = dc.yearFraction(today, today + expiryShiftTenors[i]);
    for (Size j = 0; j < termShiftTenors.size(); ++j)
        shiftTermTimes[j] = dc.yearFraction(today, today + termShiftTenors[j]);

    vector<vector<Real>> shiftedData(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<vector<Real>> diffAbsolute(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<vector<Real>> diffRelative(expiries.size(), vector<Real>(terms.size(), 0.0));
    Real shiftSize = 0.01; // arbitrary
    ShiftType shiftTypeAbsolute = ShiftType::Absolute;
    ShiftType shiftTypeRelative = ShiftType::Relative;
    for (Size i = 0; i < expiryShiftTenors.size(); ++i) {
        for (Size j = 0; j < termShiftTenors.size(); ++j) {
            scenarioGenerator->applyShift(i, j, shiftSize, true, shiftTypeAbsolute, shiftExpiryTimes, shiftTermTimes,
                                          expiryTimes, termTimes, initialData, shiftedData, true);
            for (Size k = 0; k < expiries.size(); ++k) {
                for (Size l = 0; l < terms.size(); ++l)
                    diffAbsolute[k][l] += shiftedData[k][l] - initialData[k][l];
            }
            scenarioGenerator->applyShift(i, j, shiftSize, true, shiftTypeRelative, shiftExpiryTimes, shiftTermTimes,
                                          expiryTimes, termTimes, initialData, shiftedData, true);
            for (Size k = 0; k < expiries.size(); ++k) {
                for (Size l = 0; l < terms.size(); ++l)
                    diffRelative[k][l] += shiftedData[k][l] / initialData[k][l] - 1.0;
            }
        }
    }

    Real tolerance = 1.0e-10;
    for (Size k = 0; k < expiries.size(); ++k) {
        for (Size l = 0; l < terms.size(); ++l) {
            // BOOST_TEST_MESSAGE("2d shift: checking sensitivity to underlying grid point (" << k << ", " << l << "): "
            // << diff[k][l]);
            BOOST_CHECK_MESSAGE(fabs(diffAbsolute[k][l] - shiftSize) < tolerance,
                                "inconsistency in absolute 2d shifts at grid point (" << k << ", " << l
                                                                                      << "): " << diffAbsolute[k][l]);
            BOOST_CHECK_MESSAGE(fabs(diffRelative[k][l] - shiftSize) < tolerance,
                                "inconsistency in relative 2d shifts at grid point (" << k << ", " << l
                                                                                      << "): " << diffRelative[k][l]);
        }
    }
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

BOOST_AUTO_TEST_CASE(testEquityOptionDeltaGamma) {

    BOOST_TEST_MESSAGE("Testing Equity option sensitivities against QL analytic greeks");

    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);
    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData =
        TestConfigurationObjects::setupSimMarketData5();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = TestConfigurationObjects::setupSensitivityScenarioData5();

    map<string, SensitivityScenarioData::VolShiftData>& eqvs = sensiData->equityVolShiftData();
    for (auto& it : eqvs) {
        it.second.shiftSize = 0.0001; // want a smaller shift size than 1.0 to test the analytic sensitivities
    }
    map<string, SensitivityScenarioData::SpotShiftData>& eqs = sensiData->equityShiftData();
    for (auto& it : eqs) {
        it.second.shiftSize = 0.0001; // want a smaller shift size to test the analytic sensitivities
    }

    // build sim market
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // build portfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("EquityForward") = "DiscountedCashflows";
    data->engine("EquityForward") = "DiscountingEquityForwardEngine";
    data->model("EquityOption") = "BlackScholesMerton";
    data->engine("EquityOption") = "AnalyticEuropeanEngine";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, simMarket);

    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    Size trnCount = 0;
    portfolio->add(buildEquityOption("Call_SP5", "Long", "Call", 2, "SP5", "USD", 2147.56, 1000));
    trnCount++;
    portfolio->add(buildEquityOption("Put_SP5", "Long", "Put", 2, "SP5", "USD", 2147.56, 1000));
    trnCount++;
    // portfolio->add(buildEquityForward("Fwd_SP5", "Long", 2, "SP5", "USD", 2147.56, 1000));
    // trnCount++;
    portfolio->add(buildEquityOption("Call_Luft", "Short", "Call", 2, "Lufthansa", "EUR", 12.75, 1000));
    trnCount++;
    portfolio->add(buildEquityOption("Put_Luft", "Short", "Put", 2, "Lufthansa", "EUR", 12.75, 1000));
    trnCount++;
    // portfolio->add(buildEquityForward("Fwd_Luft", "Short", 2, "Lufthansa", "EUR", 12.75, 1000));
    // trnCount++;
    portfolio->build(factory);
    BOOST_CHECK_EQUAL(portfolio->size(), trnCount);

    struct AnalyticInfo {
        string id;
        string name;
        string npvCcy;
        Real spot;
        Real fx;
        Real baseNpv;
        Real qlNpv;
        Real delta;
        Real gamma;
        Real vega;
        Real rho;
        Real divRho;
    };
    map<string, AnalyticInfo> qlInfoMap;
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        AnalyticInfo info;
        QuantLib::ext::shared_ptr<ore::data::EquityOption> eqoTrn = QuantLib::ext::dynamic_pointer_cast<ore::data::EquityOption>(trade);
        BOOST_CHECK(eqoTrn);
        info.id = tradeId;
        info.name = eqoTrn->equityName();
        info.npvCcy = trade->npvCurrency();

        info.spot = initMarket->equitySpot(info.name)->value();
        string pair = info.npvCcy + simMarketData->baseCcy();
        info.fx = initMarket->fxRate(pair)->value();
        info.baseNpv = trade->instrument()->NPV() * info.fx;
        QuantLib::ext::shared_ptr<QuantLib::VanillaOption> qlOpt =
            QuantLib::ext::dynamic_pointer_cast<QuantLib::VanillaOption>(trade->instrument()->qlInstrument());
        BOOST_CHECK(qlOpt);
        Position::Type positionType = parsePositionType(eqoTrn->option().longShort());
        Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
        info.qlNpv = qlOpt->NPV() * eqoTrn->quantity() * bsInd;
        info.delta = qlOpt->delta() * eqoTrn->quantity() * bsInd;
        info.gamma = qlOpt->gamma() * eqoTrn->quantity() * bsInd;
        info.vega = qlOpt->vega() * eqoTrn->quantity() * bsInd;
        info.rho = qlOpt->rho() * eqoTrn->quantity() * bsInd;
        info.divRho = qlOpt->dividendRho() * eqoTrn->quantity() * bsInd;
        qlInfoMap[info.id] = info;
    }

    bool recalibrateModels = true; // nothing to calibrate here
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, recalibrateModels);
    sa->generateSensitivities();

    map<pair<string, string>, Real> deltaMap;
    map<pair<string, string>, Real> gammaMap;
    std::set<string> sensiTrades;
    for (auto [pid, p] : portfolio->trades()) {
        sensiTrades.insert(pid);
        for (const auto& f : sa->sensiCube()->factors()) {
            auto des = sa->sensiCube()->factorDescription(f);
            deltaMap[make_pair(pid, des)] = sa->sensiCube()->delta(pid, f);
            gammaMap[make_pair(pid, des)] = sa->sensiCube()->gamma(pid, f);
        }
    }

    struct SensiResults {
        string id;
        Real baseNpv;
        Real discountDelta;
        Real ycDelta;
        Real equitySpotDelta;
        Real equityVolDelta;
        Real equitySpotGamma;
    };

    Real epsilon = 1.e-15; // a small number
    string equitySpotStr = "EquitySpot";
    string equityVolStr = "EquityVolatility";

    for (auto it : qlInfoMap) {
        string id = it.first;
        BOOST_CHECK(sensiTrades.find(id) != sensiTrades.end());
        AnalyticInfo qlInfo = it.second;
        SensiResults res = {string(""), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        for (auto it2 : deltaMap) {
            pair<string, string> sensiKey = it2.first;
            string sensiTrnId = it2.first.first;
            if (sensiTrnId != id)
                continue;
            res.id = sensiTrnId;
            res.baseNpv = sa->sensiCube()->npv(sensiTrnId);
            string sensiId = it2.first.second;
            Real sensiVal = it2.second;
            if (std::fabs(sensiVal) < epsilon) // not interested in zero sensis
                continue;
            vector<string> tokens;
            boost::split(tokens, sensiId, boost::is_any_of("/-"));
            BOOST_CHECK(tokens.size() > 0);
            bool isEquitySpot = (tokens[0] == equitySpotStr);
            bool isEquityVol = (tokens[0] == equityVolStr);
            if (isEquitySpot) {
                BOOST_CHECK(tokens.size() > 2);
                bool hasGamma = (gammaMap.find(sensiKey) != gammaMap.end());
                BOOST_CHECK(hasGamma);
                Real gammaVal = 0.0;
                if (hasGamma) {
                    gammaVal = gammaMap[sensiKey];
                }
                res.equitySpotDelta += sensiVal;
                res.equitySpotGamma += gammaVal;
                continue;
            } else if (isEquityVol) {
                BOOST_CHECK(tokens.size() > 2);
                res.equityVolDelta += sensiVal;
                continue;
            } else {
                continue;
            }
        }

        Real bp = 1.e-4;
        Real tol = 0.5; // % relative tolerance

        BOOST_TEST_MESSAGE("SA: id=" << res.id << ", npv=" << res.baseNpv << ", equitySpotDelta=" << res.equitySpotDelta
                                     << ", equityVolDelta=" << res.equityVolDelta
                                     << ", equitySpotGamma=" << res.equitySpotGamma);
        BOOST_TEST_MESSAGE("QL: id=" << qlInfo.id << ", fx=" << qlInfo.fx << ", npv=" << qlInfo.baseNpv
                                     << ", ccyNpv=" << qlInfo.qlNpv << ", delta=" << qlInfo.delta
                                     << ", gamma=" << qlInfo.gamma << ", vega=" << qlInfo.vega
                                     << ", spotDelta=" << (qlInfo.delta * qlInfo.fx * bp * qlInfo.spot));

        Real eqVol =
            initMarket->equityVol(qlInfo.name)->blackVol(1.0, 1.0, true); // TO-DO more appropriate vol extraction
        BOOST_CHECK_CLOSE(res.equityVolDelta, qlInfo.vega * qlInfo.fx * (bp * eqVol), tol);

        BOOST_CHECK_CLOSE(res.equitySpotDelta, qlInfo.delta * qlInfo.fx * (bp * qlInfo.spot), tol);
        BOOST_CHECK_CLOSE(res.equitySpotGamma, qlInfo.gamma * qlInfo.fx * (pow(bp * qlInfo.spot, 2)), tol);
    }
}

BOOST_AUTO_TEST_CASE(testFxOptionDeltaGamma) {

    BOOST_TEST_MESSAGE("Testing FX option sensitivities against QL analytic greeks");

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData =
        TestConfigurationObjects::setupSimMarketData5();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = TestConfigurationObjects::setupSensitivityScenarioData5();

    map<string, SensitivityScenarioData::VolShiftData>& fxvs = sensiData->fxVolShiftData();
    for (auto& it : fxvs) {
        it.second.shiftSize = 0.0001; // want a smaller shift size than 1.0 to test the analytic sensitivities
    }
    map<string, SensitivityScenarioData::SpotShiftData>& fxs = sensiData->fxShiftData();
    for (auto& it : fxs) {
        it.second.shiftSize = 0.0001; // want a smaller shift size to test the analytic sensitivities
    }

    // build sim market
    auto simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // build portfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, simMarket);

    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    Size trnCount = 0;
    portfolio->add(buildFxOption("Call_1", "Long", "Call", 1, "USD", 100000000.0, "EUR", 100000000.0));
    trnCount++;
    portfolio->add(buildFxOption("Put_1", "Long", "Put", 1, "USD", 100000000.0, "EUR", 100000000.0));
    trnCount++;
    portfolio->add(buildFxOption("Call_2", "Short", "Call", 2, "GBP", 100000000.0, "CHF", 130000000.0));
    trnCount++;
    portfolio->add(buildFxOption("Put_2", "Short", "Put", 2, "GBP", 100000000.0, "CHF", 130000000.0));
    trnCount++;
    portfolio->add(buildFxOption("Call_3", "Long", "Call", 1, "EUR", 100000000.0, "USD", 100000000.0));
    trnCount++;
    portfolio->add(buildFxOption("Put_3", "Short", "Put", 1, "EUR", 100000000.0, "USD", 100000000.0));
    trnCount++;
    portfolio->add(buildFxOption("Call_4", "Long", "Call", 1, "JPY", 10000000000.0, "EUR", 100000000.0));
    trnCount++;
    portfolio->add(buildFxOption("Call_5", "Long", "Call", 1, "EUR", 100000000.0, "JPY", 10000000000.0));
    trnCount++;
    portfolio->build(factory);
    BOOST_CHECK_EQUAL(portfolio->size(), trnCount);

    struct AnalyticInfo {
        string id;
        string npvCcy;
        string forCcy;
        string domCcy;
        Real fx;
        Real trnFx;
        Real baseNpv;
        Real qlNpv;
        Real delta;
        Real gamma;
        Real vega;
        Real rho;
        Real divRho;
        Real fxForBase;
    };
    map<string, AnalyticInfo> qlInfoMap;
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        AnalyticInfo info;
        QuantLib::ext::shared_ptr<ore::data::FxOption> fxoTrn = QuantLib::ext::dynamic_pointer_cast<ore::data::FxOption>(trade);
        BOOST_CHECK(fxoTrn);
        info.id = tradeId;
        info.npvCcy = trade->npvCurrency();
        info.forCcy = fxoTrn->boughtCurrency();
        info.domCcy = fxoTrn->soldCurrency();
        BOOST_CHECK_EQUAL(info.npvCcy, info.domCcy);
        string pair = info.npvCcy + simMarketData->baseCcy();
        info.fx = initMarket->fxRate(pair)->value();
        string trnPair = info.forCcy + info.domCcy;
        info.trnFx = initMarket->fxRate(trnPair)->value();
        string forPair = info.forCcy + simMarketData->baseCcy();
        info.fxForBase = initMarket->fxRate(forPair)->value();
        info.baseNpv = trade->instrument()->NPV() * info.fx;
        QuantLib::ext::shared_ptr<QuantLib::VanillaOption> qlOpt =
            QuantLib::ext::dynamic_pointer_cast<QuantLib::VanillaOption>(trade->instrument()->qlInstrument());
        BOOST_CHECK(qlOpt);
        Position::Type positionType = parsePositionType(fxoTrn->option().longShort());
        Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
        info.qlNpv = qlOpt->NPV() * fxoTrn->boughtAmount() * bsInd;
        info.delta = qlOpt->delta() * fxoTrn->boughtAmount() * bsInd;
        info.gamma = qlOpt->gamma() * fxoTrn->boughtAmount() * bsInd;
        info.vega = qlOpt->vega() * fxoTrn->boughtAmount() * bsInd;
        info.rho = qlOpt->rho() * fxoTrn->boughtAmount() * bsInd;
        info.divRho = qlOpt->dividendRho() * fxoTrn->boughtAmount() * bsInd;
        BOOST_CHECK_CLOSE(info.fx, info.baseNpv / info.qlNpv, 0.01);
        qlInfoMap[info.id] = info;
    }

    bool recalibrateModels = true;           // nothing to calibrate here
    bool useOriginalFxForBaseCcyConv = true; // convert sensi to EUR using original FX rate (not the shifted rate)
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, recalibrateModels, nullptr,
        nullptr, useOriginalFxForBaseCcyConv);
    sa->generateSensitivities();

    map<pair<string, string>, Real> deltaMap;
    map<pair<string, string>, Real> gammaMap;
    std::set<string> sensiTrades;
    for (const auto& [pid, _] : portfolio->trades()) {
        sensiTrades.insert(pid);
        for (const auto& f : sa->sensiCube()->factors()) {
            auto des = sa->sensiCube()->factorDescription(f);
            deltaMap[make_pair(pid, des)] = sa->sensiCube()->delta(pid, f);
            gammaMap[make_pair(pid, des)] = sa->sensiCube()->gamma(pid, f);
        }
    }

    struct SensiResults {
        string id;
        Real baseNpv;
        Real forDiscountDelta;
        Real forIndexDelta;
        Real forYcDelta;
        Real domDiscountDelta;
        Real domIndexDelta;
        Real domYcDelta;
        Real fxSpotDeltaFor;
        Real fxSpotDeltaDom;
        Real fxVolDelta;
        Real fxSpotGammaFor;
        Real fxSpotGammaDom;
        Real fxRateSensiFor;
        Real fxRateSensiDom;
        bool hasFxSpotDomSensi;
        bool hasFxSpotForSensi;
        string fxSensiForCcy;
        string fxSensiDomCcy;
    };

    Real epsilon = 1.e-15; // a small number
    string discountCurveStr = "DiscountCurve";
    string indexCurveStr = "IndexCurve";
    string yieldCurveStr = "YieldCurve";
    string fxSpotStr = "FXSpot";
    string fxVolStr = "FXVolatility";
    string swaptionStr = "SwaptionVolatility";
    string capStr = "OptionletVolatility";
    for (auto it : qlInfoMap) {
        string id = it.first;
        BOOST_CHECK(sensiTrades.find(id) != sensiTrades.end());
        AnalyticInfo qlInfo = it.second;
        SensiResults res = {string(""), 0.0, 0.0, 0.0, 0.0, 0.0,   0.0,   0.0,        0.0,       0.0,
                            0.0,        0.0, 0.0, 0.0, 0.0, false, false, string(""), string("")};
        for (auto it2 : deltaMap) {
            pair<string, string> sensiKey = it2.first;
            string sensiTrnId = it2.first.first;
            if (sensiTrnId != id)
                continue;
            res.id = sensiTrnId;
            res.baseNpv = sa->sensiCube()->npv(sensiTrnId);
            string sensiId = it2.first.second;
            Real sensiVal = it2.second;
            if (std::fabs(sensiVal) < epsilon) // not interested in zero sensis
                continue;
            vector<string> tokens;
            boost::split(tokens, sensiId, boost::is_any_of("/-"));
            BOOST_CHECK(tokens.size() > 0);
            bool isDiscountCurve = (tokens[0] == discountCurveStr);
            bool isIndexCurve = (tokens[0] == indexCurveStr);
            bool isYieldCurve = (tokens[0] == yieldCurveStr);
            bool isFxSpot = (tokens[0] == fxSpotStr);
            bool isFxVol = (tokens[0] == fxVolStr);
            bool isSwaption = (tokens[0] == swaptionStr);
            bool isCapFloorlet = (tokens[0] == capStr);
            BOOST_CHECK(!(isSwaption || isCapFloorlet)); // no relation to fx options
            if (isDiscountCurve || isIndexCurve || isYieldCurve) {
                BOOST_CHECK(tokens.size() > 2);
                string ccy = tokens[1];
                bool isFgnCcySensi = (ccy == qlInfo.forCcy);
                bool isDomCcySensi = (ccy == qlInfo.domCcy);
                BOOST_CHECK(isFgnCcySensi || isDomCcySensi);
                if (isDiscountCurve) {
                    if (isFgnCcySensi)
                        res.forDiscountDelta += sensiVal;
                    else if (isDomCcySensi)
                        res.domDiscountDelta += sensiVal;
                } else if (isIndexCurve) {
                    if (isFgnCcySensi)
                        res.forIndexDelta += sensiVal;
                    else if (isDomCcySensi)
                        res.domIndexDelta += sensiVal;
                } else if (isYieldCurve) {
                    if (isFgnCcySensi)
                        res.forYcDelta += sensiVal;
                    if (isDomCcySensi)
                        res.domYcDelta += sensiVal;
                }
                continue;
            } else if (isFxSpot) {
                BOOST_CHECK(tokens.size() > 2);
                string pair = tokens[1];
                BOOST_CHECK_EQUAL(pair.length(), 6);
                string sensiForCcy = pair.substr(0, 3);
                string sensiDomCcy = pair.substr(3, 3);
                Real fxSensi = initMarket->fxRate(pair)->value();
                bool isSensiForBase = (sensiForCcy == simMarketData->baseCcy());
                bool isSensiDomBase = (sensiDomCcy == simMarketData->baseCcy());
                // TO-DO this could be relaxed to handle case where market stores the currency pairs the other way
                // around
                BOOST_CHECK(isSensiForBase && !isSensiDomBase);
                bool hasGamma = (gammaMap.find(sensiKey) != gammaMap.end());
                BOOST_CHECK(hasGamma);
                Real gammaVal = 0.0;
                if (hasGamma) {
                    gammaVal = gammaMap[sensiKey];
                }
                if (isSensiForBase) {
                    if (sensiDomCcy == qlInfo.forCcy) {
                        res.fxSpotDeltaFor += sensiVal;
                        res.fxRateSensiFor = fxSensi;
                        res.hasFxSpotForSensi = true;
                        res.fxSpotGammaFor += gammaVal;
                    } else if (sensiDomCcy == qlInfo.domCcy) {
                        res.fxSpotDeltaDom += sensiVal;
                        res.fxRateSensiDom = fxSensi;
                        res.hasFxSpotDomSensi = true;
                        res.fxSpotGammaDom += gammaVal;
                    }
                    res.fxSensiForCcy = sensiForCcy;
                    res.fxSensiDomCcy = sensiDomCcy;
                } else {
                    BOOST_ERROR("This ccy pair configuration not supported yet by this test");
                }
                continue;
            } else if (isFxVol) {
                BOOST_CHECK(tokens.size() > 2);
                string pair = tokens[1];
                BOOST_CHECK_EQUAL(pair.length(), 6);
                string sensiForCcy = pair.substr(0, 3);
                string sensiDomCcy = pair.substr(3, 3);
                BOOST_CHECK((sensiForCcy == qlInfo.forCcy) || (sensiForCcy == qlInfo.domCcy));
                BOOST_CHECK((sensiDomCcy == qlInfo.forCcy) || (sensiDomCcy == qlInfo.domCcy));
                res.fxVolDelta += sensiVal;
                continue;
            } else {
                BOOST_ERROR("Unrecognised sensitivity factor - " << sensiId);
            }
        }
        // HERE COME THE ACTUAL SENSI COMPARISONS
        // index and yc sensis not expected
        BOOST_CHECK_EQUAL(res.forIndexDelta, 0.0);
        BOOST_CHECK_EQUAL(res.domIndexDelta, 0.0);
        BOOST_CHECK_EQUAL(res.forYcDelta, 0.0);
        BOOST_CHECK_EQUAL(res.domYcDelta, 0.0);
        BOOST_TEST_MESSAGE(
            "SA: id=" << res.id << ", npv=" << res.baseNpv << ", forDiscountDelta=" << res.forDiscountDelta
                      << ", domDiscountDelta=" << res.domDiscountDelta << ", fxSpotDeltaFor=" << res.fxSpotDeltaFor
                      << ", fxSpotDeltaDom=" << res.fxSpotDeltaDom << ", fxVolDelta=" << res.fxVolDelta
                      << ", fxSpotGammaFor=" << res.fxSpotGammaFor << ", fxSpotGammaDom=" << res.fxSpotGammaDom
                      << ", hasFxDom=" << res.hasFxSpotDomSensi << ", hasFxFor=" << res.hasFxSpotForSensi);
        BOOST_TEST_MESSAGE("QL: id=" << qlInfo.id << ", forCcy=" << qlInfo.forCcy << ", domCcy=" << qlInfo.domCcy
                                     << ", fx=" << qlInfo.fx << ", npv=" << qlInfo.baseNpv
                                     << ", ccyNpv=" << qlInfo.qlNpv << ", delta=" << qlInfo.delta
                                     << ", gamma=" << qlInfo.gamma << ", vega=" << qlInfo.vega << ", rho=" << qlInfo.rho
                                     << ", divRho=" << qlInfo.divRho);
        Real bp = 1.e-4;
        Real tol = 1.0; // % relative tolerance
        // rate sensis are 1bp absolute shifts
        // fx vol sensis are 1bp relative shifts
        // fx spot sensis are 1pb relative shifts
        BOOST_CHECK_CLOSE(res.domDiscountDelta, qlInfo.rho * qlInfo.fx * bp, tol);
        BOOST_CHECK_CLOSE(res.forDiscountDelta, qlInfo.divRho * qlInfo.fx * bp, tol);
        Real fxVol = initMarket->fxVol(qlInfo.forCcy + qlInfo.domCcy)
                         ->blackVol(1.0, 1.0, true); // TO-DO more appropriate vol extraction
        BOOST_CHECK_CLOSE(res.fxVolDelta, qlInfo.vega * qlInfo.fx * (bp * fxVol), tol);
        if (res.hasFxSpotDomSensi) {
            Real qlGamma = qlInfo.gamma;
            if ((res.fxSensiForCcy == qlInfo.domCcy) && (res.fxSensiDomCcy == qlInfo.forCcy)) {
                // the QL sensi is relative to the inverted FX quote, so we need to convert to the sensi that we want
                // (via chain rule)
                Real ql_fx = qlInfo.trnFx;
                qlGamma = 2 * pow(ql_fx, 3) * qlInfo.delta + pow(ql_fx, 4) * qlInfo.gamma;
            } else if ((res.fxSensiForCcy == qlInfo.forCcy) && (res.fxSensiDomCcy == qlInfo.domCcy)) {
                qlGamma = qlInfo.gamma;
            } else {
                // perform the necessary conversion for cross quotes
                Real otherFx = 1.0 / qlInfo.fxForBase;
                qlGamma = qlInfo.gamma / pow(otherFx, 2);
            }
            BOOST_CHECK_CLOSE(res.fxSpotDeltaDom, qlInfo.delta * qlInfo.fx * (bp * qlInfo.trnFx), tol);
            BOOST_CHECK_CLOSE(res.fxSpotGammaDom, qlGamma * qlInfo.fx * (pow(bp * res.fxRateSensiDom, 2)), tol);
        }
        if (res.hasFxSpotForSensi) {
            Real qlGamma = qlInfo.gamma;
            if ((res.fxSensiForCcy == qlInfo.domCcy) && (res.fxSensiDomCcy == qlInfo.forCcy)) {
                // the QL sensi is relative to the inverted FX quote, so we need to convert to the sensi that we want
                // (via chain rule)
                Real ql_fx = qlInfo.trnFx;
                qlGamma = 2 * pow(ql_fx, 3) * qlInfo.delta + pow(ql_fx, 4) * qlInfo.gamma;
            } else if ((res.fxSensiForCcy == qlInfo.forCcy) && (res.fxSensiDomCcy == qlInfo.domCcy)) {
                qlGamma = qlInfo.gamma;
            } else {
                // perform the necessary conversion for cross quotes
                Real y = 1.0 / qlInfo.fx;        // BASE/TrnDom
                Real z = 1.0 / qlInfo.fxForBase; // BASE/TrnFor
                qlGamma = ((2.0 * y) / pow(z, 3)) * qlInfo.delta + (y / pow(z, 4)) * qlInfo.gamma;
            }
            BOOST_CHECK_CLOSE(res.fxSpotDeltaFor, qlInfo.delta * qlInfo.fx * (-bp * qlInfo.trnFx), tol);
            BOOST_CHECK_CLOSE(res.fxSpotGammaFor, qlGamma * qlInfo.fx * (pow(-bp * res.fxRateSensiFor, 2)), tol);
        }
    }
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

BOOST_AUTO_TEST_CASE(testCrossGamma) {

    BOOST_TEST_MESSAGE("Testing cross-gamma sensitivities against cached results");

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData =
        TestConfigurationObjects::setupSimMarketData5();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = TestConfigurationObjects::setupSensitivityScenarioData5();
    vector<pair<string, string>>& cgFilter = sensiData->crossGammaFilter();
    BOOST_CHECK_EQUAL(cgFilter.size(), 0);
    cgFilter.push_back(pair<string, string>("DiscountCurve/EUR", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/EUR", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("IndexCurve/EUR", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("IndexCurve/USD", "IndexCurve/USD"));
    cgFilter.push_back(pair<string, string>("DiscountCurve/USD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("OptionletVolatility/EUR", "OptionletVolatility/EUR"));
    cgFilter.push_back(pair<string, string>("OptionletVolatility/EUR", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("OptionletVolatility/EUR", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("OptionletVolatility/USD", "DiscountCurve/USD"));
    cgFilter.push_back(pair<string, string>("OptionletVolatility/USD", "OptionletVolatility/USD"));
    cgFilter.push_back(pair<string, string>("SwaptionVolatility/EUR", "SwaptionVolatility/EUR"));
    cgFilter.push_back(pair<string, string>("SwaptionVolatility/EUR", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("SwaptionVolatility/EUR", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("FXVolatility/EURUSD", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("FXSpot/EURUSD", "DiscountCurve/EUR"));
    cgFilter.push_back(pair<string, string>("FXSpot/EURUSD", "IndexCurve/EUR"));
    cgFilter.push_back(pair<string, string>("FXSpot/EURGBP", "DiscountCurve/GBP"));

    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // build portfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("CrossCurrencySwap") = "DiscountedCashflows";
    data->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    data->model("BermudanSwaption") = "LGM";
    data->engine("BermudanSwaption") = "Grid";
    map<string, string> bermudanModelParams;
    bermudanModelParams["Calibration"] = "Bootstrap";
    bermudanModelParams["CalibrationStrategy"] = "Coterminal";
    bermudanModelParams["Reversion"] = "0.03";
    bermudanModelParams["ReversionType"] = "HullWhite";
    bermudanModelParams["Volatility"] = "0.01";
    bermudanModelParams["CalibrationType"] = "Hagan";
    bermudanModelParams["Tolerance"] = "0.0001";
    data->modelParameters("BermudanSwaption") = bermudanModelParams;
    map<string, string> bermudanEngineParams;
    bermudanEngineParams["sy"] = "3.0";
    bermudanEngineParams["ny"] = "10";
    bermudanEngineParams["sx"] = "3.0";
    bermudanEngineParams["nx"] = "10";
    data->engineParameters("BermudanSwaption") = bermudanEngineParams;
    data->model("FxForward") = "DiscountedCashflows";
    data->engine("FxForward") = "DiscountingFxForwardEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";
    data->model("CapFloor") = "IborCapModel";
    data->engine("CapFloor") = "IborCapEngine";
    data->model("CapFlooredIborLeg") = "BlackOrBachelier";
    data->engine("CapFlooredIborLeg") = "BlackIborCouponPricer";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, simMarket);

    // QuantLib::ext::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    Size trnCount = 0;
    portfolio->add(buildSwap("1_Swap_EUR", "EUR", true, 10000000.0, 0, 10, 0.03, 0.00, "1Y", "30/360", "6M", "A360",
                             "EUR-EURIBOR-6M"));
    trnCount++;
    portfolio->add(buildSwap("2_Swap_USD", "USD", true, 10000000.0, 0, 15, 0.02, 0.00, "6M", "30/360", "3M", "A360",
                             "USD-LIBOR-3M"));
    trnCount++;
    portfolio->add(buildSwap("3_Swap_GBP", "GBP", true, 10000000.0, 0, 20, 0.04, 0.00, "6M", "30/360", "3M", "A360",
                             "GBP-LIBOR-6M"));
    trnCount++;
    portfolio->add(buildSwap("4_Swap_JPY", "JPY", true, 1000000000.0, 0, 5, 0.01, 0.00, "6M", "30/360", "3M", "A360",
                             "JPY-LIBOR-6M"));
    trnCount++;
    portfolio->add(buildEuropeanSwaption("5_Swaption_EUR", "Long", "EUR", true, 1000000.0, 10, 10, 0.02, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M", "Physical"));
    trnCount++;
    portfolio->add(buildEuropeanSwaption("6_Swaption_EUR", "Long", "EUR", true, 1000000.0, 2, 5, 0.02, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M", "Physical"));
    trnCount++;
    portfolio->add(buildFxOption("7_FxOption_EUR_USD", "Long", "Call", 3, "EUR", 10000000.0, "USD", 11000000.0));
    trnCount++;
    portfolio->add(buildFxOption("8_FxOption_EUR_GBP", "Long", "Call", 7, "EUR", 10000000.0, "GBP", 11000000.0));
    trnCount++;
    portfolio->add(buildCap("9_Cap_EUR", "EUR", "Long", 0.05, 1000000.0, 0, 10, "6M", "A360", "EUR-EURIBOR-6M"));
    trnCount++;
    portfolio->add(buildFloor("10_Floor_USD", "USD", "Long", 0.01, 1000000.0, 0, 10, "3M", "A360", "USD-LIBOR-3M"));
    trnCount++;
    portfolio->build(factory);
    BOOST_CHECK_EQUAL(trnCount, portfolio->size());

    bool useOriginalFxForBaseCcyConv = false;
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa =
        QuantLib::ext::make_shared<SensitivityAnalysis>(portfolio, initMarket, Market::defaultConfiguration, data,
                                                simMarketData, sensiData, useOriginalFxForBaseCcyConv);
    sa->generateSensitivities();

    std::vector<ore::analytics::SensitivityScenarioGenerator::ScenarioDescription> scenDesc =
        sa->scenarioGenerator()->scenarioDescriptions();

    struct GammaResult {
        string id;
        string factor1;
        string factor2;
        Real crossgamma;
    };

    vector<GammaResult> cachedResults = {
        {"10_Floor_USD", "DiscountCurve/USD/1/1Y", "OptionletVolatility/USD/0/1Y/0.01", -1.14292006e-05},
        {"10_Floor_USD", "DiscountCurve/USD/1/1Y", "OptionletVolatility/USD/5/2Y/0.01", -4.75325714e-06},
        {"10_Floor_USD", "DiscountCurve/USD/2/2Y", "OptionletVolatility/USD/0/1Y/0.01", -5.72627955e-05},
        {"10_Floor_USD", "DiscountCurve/USD/2/2Y", "OptionletVolatility/USD/10/3Y/0.01", -6.0423848e-05},
        {"10_Floor_USD", "DiscountCurve/USD/2/2Y", "OptionletVolatility/USD/5/2Y/0.01", -0.0003282313},
        {"10_Floor_USD", "DiscountCurve/USD/3/3Y", "DiscountCurve/USD/4/5Y", 2.38844859e-06},
        {"10_Floor_USD", "DiscountCurve/USD/3/3Y", "OptionletVolatility/USD/10/3Y/0.01", -0.0032767365},
        {"10_Floor_USD", "DiscountCurve/USD/3/3Y", "OptionletVolatility/USD/15/5Y/0.01", -0.00124021334},
        {"10_Floor_USD", "DiscountCurve/USD/3/3Y", "OptionletVolatility/USD/5/2Y/0.01", -0.000490482465},
        {"10_Floor_USD", "DiscountCurve/USD/4/5Y", "DiscountCurve/USD/5/7Y", 4.56303869e-05},
        {"10_Floor_USD", "DiscountCurve/USD/4/5Y", "OptionletVolatility/USD/10/3Y/0.01", -0.00309734116},
        {"10_Floor_USD", "DiscountCurve/USD/4/5Y", "OptionletVolatility/USD/15/5Y/0.01", -0.0154732663},
        {"10_Floor_USD", "DiscountCurve/USD/4/5Y", "OptionletVolatility/USD/20/10Y/0.01", -0.0011774169},
        {"10_Floor_USD", "DiscountCurve/USD/4/5Y", "OptionletVolatility/USD/5/2Y/0.01", -1.15352532e-06},
        {"10_Floor_USD", "DiscountCurve/USD/5/7Y", "DiscountCurve/USD/6/10Y", 0.00024726356},
        {"10_Floor_USD", "DiscountCurve/USD/5/7Y", "OptionletVolatility/USD/10/3Y/0.01", -1.30253466e-06},
        {"10_Floor_USD", "DiscountCurve/USD/5/7Y", "OptionletVolatility/USD/15/5Y/0.01", -0.0325565983},
        {"10_Floor_USD", "DiscountCurve/USD/5/7Y", "OptionletVolatility/USD/20/10Y/0.01", -0.026175823},
        {"10_Floor_USD", "DiscountCurve/USD/6/10Y", "OptionletVolatility/USD/15/5Y/0.01", -0.0151532607},
        {"10_Floor_USD", "DiscountCurve/USD/6/10Y", "OptionletVolatility/USD/20/10Y/0.01", -0.0524224726},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/0/6M", "IndexCurve/USD-LIBOR-3M/1/1Y", -0.000206363102},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/0/6M", "IndexCurve/USD-LIBOR-3M/2/2Y", -9.83482187e-06},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", "IndexCurve/USD-LIBOR-3M/2/2Y", -0.0181056744},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.000292001105},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.197980608},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.000472459871},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/3/3Y", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.506924993},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/4/5Y", "IndexCurve/USD-LIBOR-3M/5/7Y", -1.31308851},
        {"10_Floor_USD", "IndexCurve/USD-LIBOR-3M/5/7Y", "IndexCurve/USD-LIBOR-3M/6/10Y", -1.79643202},
        {"10_Floor_USD", "OptionletVolatility/USD/0/1Y/0.01", "OptionletVolatility/USD/5/2Y/0.01", 0.0214845769},
        {"10_Floor_USD", "OptionletVolatility/USD/10/3Y/0.01", "OptionletVolatility/USD/15/5Y/0.01", 0.224709734},
        {"10_Floor_USD", "OptionletVolatility/USD/15/5Y/0.01", "OptionletVolatility/USD/20/10Y/0.01", 0.693920762},
        {"10_Floor_USD", "OptionletVolatility/USD/5/2Y/0.01", "OptionletVolatility/USD/10/3Y/0.01", 0.0649121282},
        {"1_Swap_EUR", "DiscountCurve/EUR/1/1Y", "DiscountCurve/EUR/2/2Y", 0.000439456664},
        {"1_Swap_EUR", "DiscountCurve/EUR/1/1Y", "IndexCurve/EUR-EURIBOR-6M/0/6M", 0.0488603441},
        {"1_Swap_EUR", "DiscountCurve/EUR/1/1Y", "IndexCurve/EUR-EURIBOR-6M/1/1Y", -0.0725961695},
        {"1_Swap_EUR", "DiscountCurve/EUR/1/1Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.0499326873},
        {"1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "DiscountCurve/EUR/3/3Y", 0.00136525929},
        {"1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/0/6M", 0.00108389393},
        {"1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/1/1Y", 0.141865394},
        {"1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.191425738},
        {"1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.1454702},
        {"1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "DiscountCurve/EUR/4/5Y", 0.00183080882},
        {"1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/1/1Y", 0.000784549396},
        {"1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 0.425320865},
        {"1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.337527203},
        {"1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.560276813},
        {"1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "DiscountCurve/EUR/5/7Y", -0.00376823638},
        {"1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 0.000516382745},
        {"1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 0.91807051},
        {"1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.606871969},
        {"1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -1.1789221},
        {"1_Swap_EUR", "DiscountCurve/EUR/5/7Y", "DiscountCurve/EUR/6/10Y", -0.0210602414},
        {"1_Swap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 1.93838247},
        {"1_Swap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.964284878},
        {"1_Swap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -2.50079601},
        {"1_Swap_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 3.43097423},
        {"1_Swap_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -4.9024972},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/0/6M", "IndexCurve/EUR-EURIBOR-6M/1/1Y", -0.048865166},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/0/6M", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.00108389556},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/1/1Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.0924553103},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/1/1Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.000784546835},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.281394335},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.000516386237},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.359848329},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.779536431},
        {"1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.989040876},
        {"2_Swap_USD", "DiscountCurve/USD/0/6M", "DiscountCurve/USD/1/1Y", 7.85577577e-05},
        {"2_Swap_USD", "DiscountCurve/USD/1/1Y", "DiscountCurve/USD/2/2Y", 0.00034391915},
        {"2_Swap_USD", "DiscountCurve/USD/2/2Y", "DiscountCurve/USD/3/3Y", 0.00101750751},
        {"2_Swap_USD", "DiscountCurve/USD/3/3Y", "DiscountCurve/USD/4/5Y", 0.00129107304},
        {"2_Swap_USD", "DiscountCurve/USD/4/5Y", "DiscountCurve/USD/5/7Y", 0.00885138742},
        {"2_Swap_USD", "DiscountCurve/USD/5/7Y", "DiscountCurve/USD/6/10Y", 0.0236235501},
        {"2_Swap_USD", "DiscountCurve/USD/6/10Y", "DiscountCurve/USD/7/15Y", 0.07325946},
        {"2_Swap_USD", "DiscountCurve/USD/7/15Y", "DiscountCurve/USD/8/20Y", -2.22151866e-05},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/0/6M", "IndexCurve/USD-LIBOR-3M/1/1Y", -0.0202145245},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/0/6M", "IndexCurve/USD-LIBOR-3M/2/2Y", -0.000431735534},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", "IndexCurve/USD-LIBOR-3M/2/2Y", -0.0379707172},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.000316063757},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.11422779},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.000207132776},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/3/3Y", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.137591099},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/4/5Y", "IndexCurve/USD-LIBOR-3M/5/7Y", -0.305644142},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/5/7Y", "IndexCurve/USD-LIBOR-3M/6/10Y", -0.37816313},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/6/10Y", "IndexCurve/USD-LIBOR-3M/7/15Y", -0.431405343},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/6/10Y", "IndexCurve/USD-LIBOR-3M/8/20Y", -0.000289136427},
        {"2_Swap_USD", "IndexCurve/USD-LIBOR-3M/7/15Y", "IndexCurve/USD-LIBOR-3M/8/20Y", 0.00042894634},
        {"3_Swap_GBP", "DiscountCurve/GBP/0/6M", "FXSpot/EURGBP/0/spot", -0.0210289143},
        {"3_Swap_GBP", "DiscountCurve/GBP/1/1Y", "FXSpot/EURGBP/0/spot", 0.00639700286},
        {"3_Swap_GBP", "DiscountCurve/GBP/2/2Y", "FXSpot/EURGBP/0/spot", 0.0173332273},
        {"3_Swap_GBP", "DiscountCurve/GBP/3/3Y", "FXSpot/EURGBP/0/spot", 0.0420620699},
        {"3_Swap_GBP", "DiscountCurve/GBP/4/5Y", "FXSpot/EURGBP/0/spot", 0.0715365904},
        {"3_Swap_GBP", "DiscountCurve/GBP/5/7Y", "FXSpot/EURGBP/0/spot", 0.124046364},
        {"3_Swap_GBP", "DiscountCurve/GBP/6/10Y", "FXSpot/EURGBP/0/spot", 0.245374591},
        {"3_Swap_GBP", "DiscountCurve/GBP/7/15Y", "FXSpot/EURGBP/0/spot", 0.388570486},
        {"3_Swap_GBP", "DiscountCurve/GBP/8/20Y", "FXSpot/EURGBP/0/spot", -0.308991311},
        {"5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "DiscountCurve/EUR/7/15Y", 0.00500290218},
        {"5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "DiscountCurve/EUR/8/20Y", -0.000119650445},
        {"5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.193956982},
        {"5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/7/15Y", -0.274626882},
        {"5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", -0.0230959074},
        {"5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", -0.0783525323},
        {"5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "DiscountCurve/EUR/8/20Y", 0.00909222141},
        {"5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.318897412},
        {"5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "IndexCurve/EUR-EURIBOR-6M/7/15Y", -0.113123194},
        {"5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", -0.492342945},
        {"5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", -0.277872723},
        {"5_Swaption_EUR", "DiscountCurve/EUR/8/20Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.0231524316},
        {"5_Swaption_EUR", "DiscountCurve/EUR/8/20Y", "IndexCurve/EUR-EURIBOR-6M/7/15Y", 0.586686233},
        {"5_Swaption_EUR", "DiscountCurve/EUR/8/20Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", -0.741062084},
        {"5_Swaption_EUR", "DiscountCurve/EUR/8/20Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", -0.207022576},
        {"5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "IndexCurve/EUR-EURIBOR-6M/7/15Y", -0.438748346},
        {"5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", -4.80598188},
        {"5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", 0.0374673201},
        {"5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/7/15Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", 0.578274874},
        {"5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/7/15Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", -0.00750543873},
        {"5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/8/20Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", -0.134678679},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "DiscountCurve/EUR/3/3Y", 7.34225287e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "DiscountCurve/EUR/4/5Y", -1.39672557e-06},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "DiscountCurve/EUR/5/7Y", -4.54013752e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 0.00762697723},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.00743193871},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.000253325068},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.0091003291},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -3.51917679e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 0.000131859928},
        {"6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "DiscountCurve/EUR/4/5Y", 9.36521301e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "DiscountCurve/EUR/5/7Y", 1.17673517e-06},
        {"6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 0.0217662195},
        {"6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.0173020895},
        {"6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.0288530865},
        {"6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.000542137221},
        {"6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.0105191516},
        {"6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", -1.92268253e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "DiscountCurve/EUR/5/7Y", 0.000380955356},
        {"6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.000175687061},
        {"6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 0.0470703001},
        {"6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.0309543681},
        {"6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.0603712949},
        {"6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 3.56844794e-06},
        {"6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.0194332275},
        {"6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", -3.55200336e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "DiscountCurve/EUR/6/10Y", -3.53218638e-06},
        {"6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.00907584063},
        {"6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 0.000465011277},
        {"6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 0.100206381},
        {"6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.110760564},
        {"6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.000747127526},
        {"6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.0212825534},
        {"6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", -3.89078705e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -7.04072845e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 2.90610478e-06},
        {"6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 0.00015803353},
        {"6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 1.87784499e-05},
        {"6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -2.49396362e-06},
        {"6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -5.68973592e-05},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.0374868064},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.0510088999},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -1.83061212},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.00707882478},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 0.0237742927},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", 4.3887334e-05},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.0162251326},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.0753026757},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.000291552333},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.00109766971},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", -2.02629781e-06},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.126414823},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.000493449001},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.00244118512},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", -4.50652442e-06},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.0233867156},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.116493942},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", -0.000215046299},
        {"6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.000336383262},
        {"6_Swaption_EUR", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", 2.31331433e-06},
        {"7_FxOption_EUR_USD", "DiscountCurve/EUR/3/3Y", "DiscountCurve/EUR/4/5Y", 0.0027612336},
        {"7_FxOption_EUR_USD", "DiscountCurve/EUR/3/3Y", "FXSpot/EURUSD/0/spot", -42.4452352},
        {"7_FxOption_EUR_USD", "DiscountCurve/EUR/3/3Y", "FXVolatility/EURUSD/0/5Y/ATM", 168.577072},
        {"7_FxOption_EUR_USD", "DiscountCurve/EUR/4/5Y", "FXSpot/EURUSD/0/spot", -0.0776202832},
        {"7_FxOption_EUR_USD", "DiscountCurve/EUR/4/5Y", "FXVolatility/EURUSD/0/5Y/ATM", 0.308961544},
        {"7_FxOption_EUR_USD", "DiscountCurve/USD/3/3Y", "DiscountCurve/USD/4/5Y", 0.00206353236},
        {"8_FxOption_EUR_GBP", "DiscountCurve/GBP/5/7Y", "FXSpot/EURGBP/0/spot", 40.247185},
        {"9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 1.89362237e-06},
        {"9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 1.60204674e-05},
        {"9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -3.54807444e-05},
        {"9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "OptionletVolatility/EUR/14/3Y/0.05", -7.41440071e-05},
        {"9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "OptionletVolatility/EUR/19/5Y/0.05", -2.8717396e-05},
        {"9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "OptionletVolatility/EUR/9/2Y/0.05", -3.95373826e-06},
        {"9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "DiscountCurve/EUR/5/7Y", 1.87918619e-06},
        {"9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 0.000141954676},
        {"9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 0.000136532169},
        {"9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.000558091084},
        {"9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "OptionletVolatility/EUR/14/3Y/0.05", -0.000195855626},
        {"9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "OptionletVolatility/EUR/19/5Y/0.05", -0.0013501175},
        {"9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "OptionletVolatility/EUR/24/10Y/0.05", -9.03819837e-05},
        {"9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "DiscountCurve/EUR/6/10Y", 2.44087892e-05},
        {"9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 0.00131097735},
        {"9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.000537751659},
        {"9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.00376190752},
        {"9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "OptionletVolatility/EUR/19/5Y/0.05", -0.00650057233},
        {"9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "OptionletVolatility/EUR/24/10Y/0.05", -0.00529335126},
        {"9_Cap_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.00677440175},
        {"9_Cap_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.0101355366},
        {"9_Cap_EUR", "DiscountCurve/EUR/6/10Y", "OptionletVolatility/EUR/19/5Y/0.05", -0.00512368197},
        {"9_Cap_EUR", "DiscountCurve/EUR/6/10Y", "OptionletVolatility/EUR/24/10Y/0.05", -0.0166702108},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/1/1Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -3.22099407e-06},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.00114858136},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -3.32910605e-06},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.0325351415},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.22049032},
        {"9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.599739496},
        {"9_Cap_EUR", "OptionletVolatility/EUR/14/3Y/0.05", "OptionletVolatility/EUR/19/5Y/0.05", 0.0480747768},
        {"9_Cap_EUR", "OptionletVolatility/EUR/19/5Y/0.05", "OptionletVolatility/EUR/24/10Y/0.05", 0.670249341},
        {"9_Cap_EUR", "OptionletVolatility/EUR/4/1Y/0.05", "OptionletVolatility/EUR/9/2Y/0.05", 2.49049523e-05},
        {"9_Cap_EUR", "OptionletVolatility/EUR/9/2Y/0.05", "OptionletVolatility/EUR/14/3Y/0.05", 0.00180372518}};

    map<tuple<string, string, string>, Real> cachedMap;
    for (Size i = 0; i < cachedResults.size(); ++i) {
        tuple<string, string, string> p(cachedResults[i].id, cachedResults[i].factor1, cachedResults[i].factor2);
        cachedMap[p] = cachedResults[i].crossgamma;
    }

    vector<tuple<string, string, string>> ids;
    Real rel_tol = 0.005;
    Real threshold = 1.e-6;
    Size count = 0;
    for (const auto& [tradeId, _] : portfolio->trades()) {
        for (auto const& s : scenDesc) {
            if (s.type() == ShiftScenarioGenerator::ScenarioDescription::Type::Cross) {
                string factor1 = s.factor1();
                string factor2 = s.factor2();
                ostringstream os;
                os << tradeId << "_" << factor1 << "_" << factor2;
                string keyStr = os.str();
                tuple<string, string, string> key = make_tuple(tradeId, factor1, factor2);
                Real crossgamma = sa->sensiCube()->crossGamma(tradeId, make_pair(s.key1(), s.key2()));
                if (fabs(crossgamma) >= threshold) {
                    ids.push_back(make_tuple(tradeId, factor1, factor2));
                    // BOOST_TEST_MESSAGE("{ \"" << id << std::setprecision(9) << "\", \"" << factor1 << "\", \"" <<
                    // factor2 <<
                    // "\", " << crossgamma << " },");
                    auto cached_it = cachedMap.find(key);
                    BOOST_CHECK_MESSAGE(cached_it != cachedMap.end(), keyStr << " not found in cached results");
                    if (cached_it != cachedMap.end()) {
                        Real cached_cg = cached_it->second;
                        BOOST_CHECK_CLOSE(crossgamma, cached_cg, rel_tol);
                        count++;
                    }
                }
            }
        }
    }
    BOOST_TEST_MESSAGE("number of cross-gammas checked = " << count);
    BOOST_CHECK_MESSAGE(count == cachedResults.size(), "number of non-zero sensitivities ("
                                                           << count << ") do not match regression data ("
                                                           << cachedResults.size() << ")");
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
