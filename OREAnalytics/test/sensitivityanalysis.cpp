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

#include <test/sensitivityanalysis.hpp>
#include <test/testmarket.hpp>
#include <test/testportfolio.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/log.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <ored/model/lgmdata.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <ql/time/date.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <orea/engine/all.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <orea/engine/observationmode.hpp>
#include <boost/timer.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

namespace testsuite {

boost::shared_ptr<data::Conventions> conv() {
    boost::shared_ptr<data::Conventions> conventions(new data::Conventions());

    boost::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    boost::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    return conventions;
}

void SensitivityAnalysisTest::testPortfolioSensitivity() {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity");

    SavedSettings backup;

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");
    ccys.push_back("CHF");
    ccys.push_back("USD");
    ccys.push_back("JPY");

    // Init market
    boost::shared_ptr<Market> initMarket = boost::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = { "EUR", "GBP", "USD", "CHF", "JPY" };
    simMarketData->yieldCurveTenors() = { 1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,  4 * Years,
                                          5 * Years,  7 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years };
    simMarketData->indices() = { "EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M" };
    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = { 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolExpiries() = { 6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                         5 * Years,  7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolCcys() = ccys;
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true; // false;

    simMarketData->fxVolExpiries() = { 1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years };
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true; // false;
    simMarketData->fxVolCcyPairs() = { "EURUSD", "EURGBP", "EURCHF", "EURJPY" };

    simMarketData->fxCcyPairs() = { "EURUSD", "EURGBP", "EURCHF", "EURJPY" };

    simMarketData->simulateCapFloorVols() = false;

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();
    // sensiData->discountCurrencies() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    sensiData->discountDomain() = "ZERO";
    sensiData->discountShiftTenors() = {
        1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years
    }; // multiple tenors: triangular shifts
    sensiData->discountShiftType() = "Absolute";
    sensiData->discountShiftSize() = 0.0001;
    sensiData->discountLabel() = "YIELD_DISCOUNT";

    // sensiData->indexNames() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    sensiData->indexDomain() = "ZERO";
    sensiData->indexShiftTenors() = {
        1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years
    }; // multiple tenors: triangular shifts
    sensiData->indexShiftType() = "Absolute";
    sensiData->indexShiftSize() = 0.0001;
    sensiData->indexLabel() = "YIELD_INDEX";

    // sensiData->fxCurrencyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxShiftType() = "Relative";
    sensiData->fxShiftSize() = 0.01;
    sensiData->fxLabel() = "FX";

    // sensiData->fxVolCurrencyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxVolShiftType() = "Relative";
    sensiData->fxVolShiftSize() = 1.0;               // 0.01;
    sensiData->fxVolShiftExpiries() = { 5 * Years }; // parallel shift only { 6*Months, 1*Years, 2*Years, 3*Years };
    sensiData->fxVolLabel() = "VOL_FX";

    // sensiData->swaptionVolCurrencies() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    sensiData->swaptionVolShiftType() = "Relative";
    sensiData->swaptionVolShiftSize() = 0.01;
    sensiData->swaptionVolShiftExpiries() = {
        2 * Years, 5 * Years, 10 * Years
    }; // parallel shift only //{1*Years, 2*Years, 3*Years, 5*Years};
    sensiData->swaptionVolShiftTerms() = { 5 * Years,
                                           10 * Years }; // parallel shifts only //{1*Years, 2*Years, 3*Years, 5*Years};
    sensiData->swaptionVolLabel() = "VOL_SWAPTION";

    sensiData->capFloorVolShiftType() = "Relative";
    sensiData->capFloorVolShiftSize() = 0.01;
    sensiData->capFloorVolShiftExpiries() = { 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years };
    sensiData->capFloorVolShiftStrikes() = { 0.05 }; // parallel shifts only
    sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";

    // build scenario generator
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator(
        new SensitivityScenarioGenerator(scenarioFactory, sensiData, simMarketData, today, initMarket));
    boost::shared_ptr<ScenarioGenerator> sgen(scenarioGenerator);

    // build scenario sim market
    Conventions conventions = *conv();
    boost::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        boost::make_shared<analytics::ScenarioSimMarket>(sgen, initMarket, simMarketData, conventions);

    // build porfolio
    boost::shared_ptr<EngineData> data = boost::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("CrossCurrencySwap") = "DiscountedCashflows";
    data->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    data->model("FxForward") = "DiscountedCashflows";
    data->engine("FxForward") = "DiscountingFxForwardEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";
    data->model("CapFloor") = "IborCapModel";
    data->engine("CapFloor") = "IborCapEngine";
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(data, simMarket);
    factory->registerBuilder(boost::make_shared<SwapEngineBuilder>());
    factory->registerBuilder(boost::make_shared<EuropeanSwaptionEngineBuilder>());
    factory->registerBuilder(boost::make_shared<FxOptionEngineBuilder>());
    factory->registerBuilder(boost::make_shared<FxForwardEngineBuilder>());
    factory->registerBuilder(boost::make_shared<CapFloorEngineBuilder>());

    // boost::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    boost::shared_ptr<Portfolio> portfolio(new Portfolio());
    portfolio->add(buildSwap("1_Swap_EUR", "EUR", true, 10000000.0, 0, 10, 0.03, 0.00, "1Y", "30/360", "6M", "A360",
                             "EUR-EURIBOR-6M"));
    portfolio->add(buildSwap("2_Swap_USD", "USD", true, 10000000.0, 0, 15, 0.02, 0.00, "6M", "30/360", "3M", "A360",
                             "USD-LIBOR-3M"));
    portfolio->add(buildSwap("3_Swap_GBP", "GBP", true, 10000000.0, 0, 20, 0.04, 0.00, "6M", "30/360", "3M", "A360",
                             "GBP-LIBOR-6M"));
    portfolio->add(buildSwap("4_Swap_JPY", "JPY", true, 1000000000.0, 0, 5, 0.01, 0.00, "6M", "30/360", "3M", "A360",
                             "JPY-LIBOR-6M"));
    portfolio->add(buildEuropeanSwaption("5_Swaption_EUR", "Long", "EUR", true, 1000000.0, 10, 10, 0.03, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildEuropeanSwaption("6_Swaption_EUR", "Long", "EUR", true, 1000000.0, 2, 5, 0.03, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildFxOption("7_FxOption_EUR_USD", "Long", "Call", 3, "EUR", 10000000.0, "USD", 11000000.0));
    portfolio->add(buildFxOption("8_FxOption_EUR_GBP", "Long", "Call", 7, "EUR", 10000000.0, "GBP", 11000000.0));
    portfolio->build(factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // build the scenario engine
    ScenarioEngine engine(today, simMarket, simMarketData->baseCcy());

    // run scenarios and fill the cube
    boost::timer t;
    boost::shared_ptr<NPVCube> cube = boost::make_shared<DoublePrecisionInMemoryCube>(
        today, portfolio->ids(), vector<Date>(1, today), scenarioGenerator->samples());
    engine.buildCube(portfolio, cube);
    double elapsed = t.elapsed();

    struct Results {
        string id;
        string label;
        Real npv;
        Real sensi;
    };

    std::vector<Results> cachedResults = {
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/1Y/UP", -928826, 12.1683 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/2Y/UP", -928826, 19.0081 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/3Y/UP", -928826, 46.1186 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/5Y/UP", -928826, 85.1033 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/7Y/UP", -928826, 149.43 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/10Y/UP", -928826, 205.064 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/1Y/DOWN", -928826, -12.1699 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/2Y/DOWN", -928826, -19.0137 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/3Y/DOWN", -928826, -46.1338 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/5Y/DOWN", -928826, -85.1406 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/7Y/DOWN", -928826, -149.515 },
        { "1_Swap_EUR", "YIELD_DISCOUNT/EUR/10Y/DOWN", -928826, -205.239 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/1Y/UP", -928826, -480.331 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/2Y/UP", -928826, 38.7816 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/3Y/UP", -928826, 94.186 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/5Y/UP", -928826, 173.125 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/7Y/UP", -928826, 304.648 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/10Y/UP", -928826, 8479.55 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/1Y/DOWN", -928826, 480.402 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/2Y/DOWN", -928826, -38.4045 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/3Y/DOWN", -928826, -93.532 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/5Y/DOWN", -928826, -171.969 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/7Y/DOWN", -928826, -302.864 },
        { "1_Swap_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/10Y/DOWN", -928826, -8478.14 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/1Y/UP", 980404, -7.11721 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/2Y/UP", 980404, -15.8605 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/3Y/UP", 980404, -38.0708 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/5Y/UP", 980404, -68.7288 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/7Y/UP", 980404, -118.405 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/10Y/UP", 980404, -244.946 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/15Y/UP", 980404, -202.226 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/20Y/UP", 980404, 0.0148314 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/1Y/DOWN", 980404, 7.11764 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/2Y/DOWN", 980404, 15.8623 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/3Y/DOWN", 980404, 38.0784 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/5Y/DOWN", 980404, 68.7502 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/7Y/DOWN", 980404, 118.458 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/10Y/DOWN", 980404, 245.108 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/15Y/DOWN", 980404, 202.42 },
        { "2_Swap_USD", "YIELD_DISCOUNT/USD/20Y/DOWN", 980404, -0.0148314 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/1Y/UP", 980404, -182.901 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/2Y/UP", 980404, 47.3066 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/3Y/UP", 980404, 113.4 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/5Y/UP", 980404, 205.068 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/7Y/UP", 980404, 352.859 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/10Y/UP", 980404, 730.076 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/15Y/UP", 980404, 8626.78 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/20Y/UP", 980404, 5.86437 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/1Y/DOWN", 980404, 182.935 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/2Y/DOWN", 980404, -47.1526 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/3Y/DOWN", 980404, -113.136 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/5Y/DOWN", 980404, -204.611 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/7Y/DOWN", 980404, -352.166 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/10Y/DOWN", 980404, -729.248 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/15Y/DOWN", 980404, -8626.13 },
        { "2_Swap_USD", "YIELD_INDEX/USD-LIBOR-3M/20Y/DOWN", 980404, -5.86436 },
        { "2_Swap_USD", "FX/USDEUR/UP", 980404, 9804.04 },
        { "2_Swap_USD", "FX/USDEUR/DOWN", 980404, -9804.04 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/1Y/UP", 69795.3, 1.47798 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/2Y/UP", 69795.3, -1.75066 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/3Y/UP", 69795.3, -4.24827 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/5Y/UP", 69795.3, -7.2252 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/7Y/UP", 69795.3, -12.5287 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/10Y/UP", 69795.3, -24.7828 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/15Y/UP", 69795.3, -39.2456 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/20Y/UP", 69795.3, 31.2081 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/1Y/DOWN", 69795.3, -1.47827 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/2Y/DOWN", 69795.3, 1.74981 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/3Y/DOWN", 69795.3, 4.2473 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/5Y/DOWN", 69795.3, 7.22426 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/7Y/DOWN", 69795.3, 12.5298 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/10Y/DOWN", 69795.3, 24.7939 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/15Y/DOWN", 69795.3, 39.2773 },
        { "3_Swap_GBP", "YIELD_DISCOUNT/GBP/20Y/DOWN", 69795.3, -31.2925 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/1Y/UP", 69795.3, -239.702 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/2Y/UP", 69795.3, 81.3735 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/3Y/UP", 69795.3, 239.034 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/5Y/UP", 69795.3, 372.209 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/7Y/UP", 69795.3, 654.949 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/10Y/UP", 69795.3, 1343.01 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/15Y/UP", 69795.3, 2139.68 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/20Y/UP", 69795.3, 12633.8 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/1Y/DOWN", 69795.3, 239.754 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/2Y/DOWN", 69795.3, -81.1438 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/3Y/DOWN", 69795.3, -238.649 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/5Y/DOWN", 69795.3, -371.553 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/7Y/DOWN", 69795.3, -653.972 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/10Y/DOWN", 69795.3, -1341.88 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/15Y/DOWN", 69795.3, -2138.11 },
        { "3_Swap_GBP", "YIELD_INDEX/GBP-LIBOR-6M/20Y/DOWN", 69795.3, -12632.5 },
        { "3_Swap_GBP", "FX/GBPEUR/UP", 69795.3, 697.953 },
        { "3_Swap_GBP", "FX/GBPEUR/DOWN", 69795.3, -697.953 },
        { "4_Swap_JPY", "YIELD_DISCOUNT/JPY/1Y/UP", 871.03, -0.00895744 },
        { "4_Swap_JPY", "YIELD_DISCOUNT/JPY/2Y/UP", 871.03, -0.020079 },
        { "4_Swap_JPY", "YIELD_DISCOUNT/JPY/3Y/UP", 871.03, -0.0667249 },
        { "4_Swap_JPY", "YIELD_DISCOUNT/JPY/5Y/UP", 871.03, 4.75708 },
        { "4_Swap_JPY", "YIELD_DISCOUNT/JPY/1Y/DOWN", 871.03, 0.00891103 },
        { "4_Swap_JPY", "YIELD_DISCOUNT/JPY/2Y/DOWN", 871.03, 0.0199001 },
        { "4_Swap_JPY", "YIELD_DISCOUNT/JPY/3Y/DOWN", 871.03, 0.0664106 },
        { "4_Swap_JPY", "YIELD_DISCOUNT/JPY/5Y/DOWN", 871.03, -4.75978 },
        { "4_Swap_JPY", "YIELD_INDEX/JPY-LIBOR-6M/1Y/UP", 871.03, -190.575 },
        { "4_Swap_JPY", "YIELD_INDEX/JPY-LIBOR-6M/2Y/UP", 871.03, 7.81453 },
        { "4_Swap_JPY", "YIELD_INDEX/JPY-LIBOR-6M/3Y/UP", 871.03, 19.3576 },
        { "4_Swap_JPY", "YIELD_INDEX/JPY-LIBOR-6M/5Y/UP", 871.03, 3832.83 },
        { "4_Swap_JPY", "YIELD_INDEX/JPY-LIBOR-6M/1Y/DOWN", 871.03, 190.608 },
        { "4_Swap_JPY", "YIELD_INDEX/JPY-LIBOR-6M/2Y/DOWN", 871.03, -7.6631 },
        { "4_Swap_JPY", "YIELD_INDEX/JPY-LIBOR-6M/3Y/DOWN", 871.03, -19.0907 },
        { "4_Swap_JPY", "YIELD_INDEX/JPY-LIBOR-6M/5Y/DOWN", 871.03, -3832.59 },
        { "4_Swap_JPY", "FX/JPYEUR/UP", 871.03, 8.7103 },
        { "4_Swap_JPY", "FX/JPYEUR/DOWN", 871.03, -8.7103 },
        { "5_Swaption_EUR", "YIELD_DISCOUNT/EUR/10Y/UP", 18027.1, -1.33793 },
        { "5_Swaption_EUR", "YIELD_DISCOUNT/EUR/15Y/UP", 18027.1, 0.197251 },
        { "5_Swaption_EUR", "YIELD_DISCOUNT/EUR/20Y/UP", 18027.1, 2.41317 },
        { "5_Swaption_EUR", "YIELD_DISCOUNT/EUR/10Y/DOWN", 18027.1, 1.33862 },
        { "5_Swaption_EUR", "YIELD_DISCOUNT/EUR/15Y/DOWN", 18027.1, -0.197718 },
        { "5_Swaption_EUR", "YIELD_DISCOUNT/EUR/20Y/DOWN", 18027.1, -2.41555 },
        { "5_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/10Y/UP", 18027.1, -266.802 },
        { "5_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/15Y/UP", 18027.1, 38.2821 },
        { "5_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/20Y/UP", 18027.1, 487.065 },
        { "5_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/10Y/DOWN", 18027.1, 268.413 },
        { "5_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/15Y/DOWN", 18027.1, -38.1352 },
        { "5_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/20Y/DOWN", 18027.1, -481.778 },
        { "5_Swaption_EUR", "VOL_SWAPTION/EUR/10Y/10Y/UP", 18027.1, 357.634 },
        { "5_Swaption_EUR", "VOL_SWAPTION/EUR/10Y/10Y/DOWN", 18027.1, -356.596 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/2Y/UP", 1156.84, -0.0976825 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/3Y/UP", 1156.84, 0.00251532 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/5Y/UP", 1156.84, 0.00988176 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/7Y/UP", 1156.84, 0.320953 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/10Y/UP", 1156.84, 0.00247921 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/2Y/DOWN", 1156.84, 0.0976982 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/3Y/DOWN", 1156.84, -0.00254931 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/5Y/DOWN", 1156.84, -0.00993997 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/7Y/DOWN", 1156.84, -0.321034 },
        { "6_Swaption_EUR", "YIELD_DISCOUNT/EUR/10Y/DOWN", 1156.84, -0.00247921 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/2Y/UP", 1156.84, -19.525 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/3Y/UP", 1156.84, 0.809505 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/5Y/UP", 1156.84, 1.79161 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/7Y/UP", 1156.84, 65.6802 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/10Y/UP", 1156.84, 0.248765 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/2Y/DOWN", 1156.84, 19.7717 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/3Y/DOWN", 1156.84, -0.80235 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/5Y/DOWN", 1156.84, -1.77773 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/7Y/DOWN", 1156.84, -63.0435 },
        { "6_Swaption_EUR", "YIELD_INDEX/EUR-EURIBOR-6M/10Y/DOWN", 1156.84, -0.248725 },
        { "6_Swaption_EUR", "VOL_SWAPTION/EUR/2Y/5Y/UP", 1156.84, 47.3455 },
        { "6_Swaption_EUR", "VOL_SWAPTION/EUR/5Y/5Y/UP", 1156.84, 0.0858115 },
        { "6_Swaption_EUR", "VOL_SWAPTION/EUR/2Y/5Y/DOWN", 1156.84, -46.4415 },
        { "6_Swaption_EUR", "VOL_SWAPTION/EUR/5Y/5Y/DOWN", 1156.84, -0.0858084 },
        { "7_FxOption_EUR_USD", "YIELD_DISCOUNT/EUR/3Y/UP", 1.36968e+06, -2107.81 },
        { "7_FxOption_EUR_USD", "YIELD_DISCOUNT/EUR/5Y/UP", 1.36968e+06, -3.85768 },
        { "7_FxOption_EUR_USD", "YIELD_DISCOUNT/USD/3Y/UP", 1.36968e+06, 1698.91 },
        { "7_FxOption_EUR_USD", "YIELD_DISCOUNT/USD/5Y/UP", 1.36968e+06, 3.10717 },
        { "7_FxOption_EUR_USD", "YIELD_DISCOUNT/EUR/3Y/DOWN", 1.36968e+06, 2109.74 },
        { "7_FxOption_EUR_USD", "YIELD_DISCOUNT/EUR/5Y/DOWN", 1.36968e+06, 3.85768 },
        { "7_FxOption_EUR_USD", "YIELD_DISCOUNT/USD/3Y/DOWN", 1.36968e+06, -1698.12 },
        { "7_FxOption_EUR_USD", "YIELD_DISCOUNT/USD/5Y/DOWN", 1.36968e+06, -3.10717 },
        { "7_FxOption_EUR_USD", "FX/USDEUR/UP", 1.36968e+06, -55979.6 },
        { "7_FxOption_EUR_USD", "FX/USDEUR/DOWN", 1.36968e+06, 57426.4 },
        { "7_FxOption_EUR_USD", "VOL_FX/EURUSD/5Y/UP", 1.36968e+06, 672236 },
        { "7_FxOption_EUR_USD", "VOL_FX/EURUSD/5Y/DOWN", 1.36968e+06, -329688 },
        { "8_FxOption_EUR_GBP", "YIELD_DISCOUNT/EUR/7Y/UP", 798336, -2435.22 },
        { "8_FxOption_EUR_GBP", "YIELD_DISCOUNT/GBP/7Y/UP", 798336, 1880.89 },
        { "8_FxOption_EUR_GBP", "YIELD_DISCOUNT/EUR/7Y/DOWN", 798336, 2441.08 },
        { "8_FxOption_EUR_GBP", "YIELD_DISCOUNT/GBP/7Y/DOWN", 798336, -1878.05 },
        { "8_FxOption_EUR_GBP", "FX/GBPEUR/UP", 798336, -26437.4 },
        { "8_FxOption_EUR_GBP", "FX/GBPEUR/DOWN", 798336, 27284.3 },
        { "8_FxOption_EUR_GBP", "VOL_FX/EURGBP/5Y/UP", 798336, 1.36635e+06 },
        { "8_FxOption_EUR_GBP", "VOL_FX/EURGBP/5Y/DOWN", 798336, -798336 }
    };

    std::map<pair<string, string>, Real> npvMap, sensiMap;
    for (Size i = 0; i < cachedResults.size(); ++i) {
        pair<string, string> p(cachedResults[i].id, cachedResults[i].label);
        npvMap[p] = cachedResults[i].npv;
        sensiMap[p] = cachedResults[i].sensi;
    }

    Real tiny = 1.0e-10;
    Real tolerance = 0.01;
    Size count = 0;
    for (Size i = 0; i < portfolio->size(); ++i) {
        Real npv0 = cube->getT0(i, 0);
        string id = portfolio->trades()[i]->id();
        for (Size j = 1; j < scenarioGenerator->samples(); ++j) { // skip j = 0, this is the base scenario
            Real npv = cube->get(i, 0, j, 0);
            Real sensi = npv - npv0;
            string label = scenarioGenerator->scenarios()[j]->label();
            if (fabs(sensi) > tiny) {
                count++;
                //BOOST_TEST_MESSAGE("{ \"" << id << "\", \"" << label << "\", " << npv0 << ", " << sensi << " },");
                pair<string, string> p(id, label);
                QL_REQUIRE(npvMap.find(p) != npvMap.end(), "pair (" << p.first << ", " << p.second
                                                                    << ") not found in npv map");
                QL_REQUIRE(sensiMap.find(p) != sensiMap.end(), "pair (" << p.first << ", " << p.second
                                                                        << ") not found in sensi map");
                BOOST_CHECK_MESSAGE(fabs(npv0 - npvMap[p]) < tolerance || fabs((npv0 - npvMap[p]) / npv0) <
                tolerance,
                                    "npv regression failed for pair (" << p.first << ", " << p.second << "): " <<
                                    npv0
                                                                       << " vs " << npvMap[p]);
                BOOST_CHECK_MESSAGE(fabs(sensi - sensiMap[p]) < tolerance ||
                                        fabs((sensi - sensiMap[p]) / sensi) < tolerance,
                                    "sensitivity regression failed for pair ("
                                        << p.first << ", " << p.second << "): " << sensi << " vs " << sensiMap[p]);
            }
        }
    }
    BOOST_CHECK_MESSAGE(count == cachedResults.size(), "number of non-zero sensitivities do not match regression data");

    BOOST_TEST_MESSAGE("Cube generated in " << elapsed << " seconds");
}

void SensitivityAnalysisTest::test1dShifts() {
    BOOST_TEST_MESSAGE("Testing 1d shifts");

    SavedSettings backup;

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");

    // Init market
    boost::shared_ptr<Market> initMarket = boost::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = { "EUR", "GBP" };
    simMarketData->yieldCurveTenors() = { 1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years, 4 * Years,
                                          5 * Years,  6 * Years,  7 * Years,  8 * Years,  9 * Years, 10 * Years,
                                          12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years };
    simMarketData->indices() = { "EUR-EURIBOR-6M", "GBP-LIBOR-6M" };
    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = { 1 * Years, 2 * Years, 3 * Years,  4 * Years,
                                      5 * Years, 7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolExpiries() = { 6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                         5 * Years,  7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolCcys() = ccys;
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true;

    simMarketData->fxVolExpiries() = { 1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years };
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true;
    simMarketData->fxVolCcyPairs() = { "EURGBP" };

    simMarketData->fxCcyPairs() = { "EURGBP" };

    simMarketData->simulateCapFloorVols() = false;

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();
    sensiData->discountDomain() = "ZERO";
    sensiData->discountShiftTenors() = {
        1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years
    }; // multiple tenors: triangular shifts
    sensiData->discountShiftType() = "Absolute";
    sensiData->discountShiftSize() = 0.0001;
    sensiData->discountLabel() = "YIELD_DISCOUNT";

    // sensiData->indexNames() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    sensiData->indexDomain() = "ZERO";
    sensiData->indexShiftTenors() = {
        1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years
    }; // multiple tenors: triangular shifts
    sensiData->indexShiftType() = "Absolute";
    sensiData->indexShiftSize() = 0.0001;
    sensiData->indexLabel() = "YIELD_INDEX";

    // sensiData->fxCurrencyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxShiftType() = "Relative";
    sensiData->fxShiftSize() = 0.01;
    sensiData->fxLabel() = "FX";

    // sensiData->fxVolCurrencyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxVolShiftType() = "Relative";
    sensiData->fxVolShiftSize() = 1.0; // 0.01;
    sensiData->fxVolShiftExpiries() = { 1 * Years, 5 * Years };
    sensiData->fxVolLabel() = "VOL_FX";

    // sensiData->swaptionVolCurrencies() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    sensiData->swaptionVolShiftType() = "Relative";
    sensiData->swaptionVolShiftSize() = 0.01;
    sensiData->swaptionVolShiftExpiries() = { 5 * Years };
    sensiData->swaptionVolShiftTerms() = { 5 * Years };
    sensiData->swaptionVolLabel() = "VOL_SWAPTION";

    sensiData->capFloorVolShiftType() = "Relative";
    sensiData->capFloorVolShiftSize() = 0.01;
    sensiData->capFloorVolShiftExpiries() = { 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years };
    sensiData->capFloorVolShiftStrikes() = { 0.05 }; // parallel shifts only
    sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";

    // build scenario generator
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator(
        new SensitivityScenarioGenerator(scenarioFactory, sensiData, simMarketData, today, initMarket));

    // cache initial zero rates
    vector<Period> tenors = simMarketData->yieldCurveTenors();
    vector<Real> initialZeros(tenors.size());
    vector<Real> times(tenors.size());
    string ccy = simMarketData->ccys()[0];
    Handle<YieldTermStructure> ts = initMarket->discountCurve(ccy);
    DayCounter dc = ts->dayCounter();
    for (Size j = 0; j < tenors.size(); ++j) {
        Date d = today + simMarketData->yieldCurveTenors()[j];
        initialZeros[j] = ts->zeroRate(d, dc, Continuous);
        times[j] = dc.yearFraction(today, d);
    }

    // apply zero shifts for tenors on the shift curve
    // collect shifted data at tenors of the underlying curve
    // aggregate "observed" shifts
    // compare to expected total shifts
    vector<Period> shiftTenors = sensiData->discountShiftTenors();
    vector<Time> shiftTimes(shiftTenors.size());
    for (Size i = 0; i < shiftTenors.size(); ++i)
        shiftTimes[i] = dc.yearFraction(today, today + shiftTenors[i]);

    vector<Real> shiftedZeros(tenors.size());
    vector<Real> diffAbsolute(tenors.size(), 0.0);
    vector<Real> diffRelative(tenors.size(), 0.0);
    Real shiftSize = 0.01;
    SensitivityScenarioGenerator::ShiftType shiftTypeAbsolute = SensitivityScenarioGenerator::ShiftType::Absolute;
    SensitivityScenarioGenerator::ShiftType shiftTypeRelative = SensitivityScenarioGenerator::ShiftType::Relative;
    for (Size i = 0; i < shiftTenors.size(); ++i) {
        scenarioGenerator->applyShift(i, shiftSize, true, shiftTypeAbsolute, shiftTimes, initialZeros, times,
                                      shiftedZeros);
        for (Size j = 0; j < tenors.size(); ++j)
            diffAbsolute[j] += shiftedZeros[j] - initialZeros[j];
        scenarioGenerator->applyShift(i, shiftSize, true, shiftTypeRelative, shiftTimes, initialZeros, times,
                                      shiftedZeros);
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
}

void SensitivityAnalysisTest::test2dShifts() {
    BOOST_TEST_MESSAGE("Testing 2d shifts");

    SavedSettings backup;

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");

    // Init market
    boost::shared_ptr<Market> initMarket = boost::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = { "EUR", "GBP" };
    simMarketData->yieldCurveTenors() = { 1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years, 4 * Years,
                                          5 * Years,  6 * Years,  7 * Years,  8 * Years,  9 * Years, 10 * Years,
                                          12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years };
    simMarketData->indices() = { "EUR-EURIBOR-6M", "GBP-LIBOR-6M" };
    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = { 1 * Years, 2 * Years, 3 * Years,  4 * Years,
                                      5 * Years, 7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolExpiries() = { 6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                         5 * Years,  7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolCcys() = ccys;
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true;

    simMarketData->fxVolExpiries() = { 1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years };
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true;
    simMarketData->fxVolCcyPairs() = { "EURGBP" };

    simMarketData->fxCcyPairs() = { "EURGBP" };

    simMarketData->simulateCapFloorVols() = false;

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();
    sensiData->discountDomain() = "ZERO";
    sensiData->discountShiftTenors() = {
        1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years
    }; // multiple tenors: triangular shifts
    sensiData->discountShiftType() = "Absolute";
    sensiData->discountShiftSize() = 0.0001;
    sensiData->discountLabel() = "YIELD_DISCOUNT";

    // sensiData->indexNames() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    sensiData->indexDomain() = "ZERO";
    sensiData->indexShiftTenors() = {
        1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years
    }; // multiple tenors: triangular shifts
    sensiData->indexShiftType() = "Absolute";
    sensiData->indexShiftSize() = 0.0001;
    sensiData->indexLabel() = "YIELD_INDEX";

    // sensiData->fxCurrencyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxShiftType() = "Relative";
    sensiData->fxShiftSize() = 0.01;
    sensiData->fxLabel() = "FX";

    // sensiData->fxVolCurrencyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxVolShiftType() = "Relative";
    sensiData->fxVolShiftSize() = 1.0; // 0.01;
    sensiData->fxVolShiftExpiries() = { 1 * Years, 5 * Years };
    sensiData->fxVolLabel() = "VOL_FX";

    // sensiData->swaptionVolCurrencies() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    sensiData->swaptionVolShiftType() = "Relative";
    sensiData->swaptionVolShiftSize() = 0.01;
    sensiData->swaptionVolShiftExpiries() = { 3 * Years, 5 * Years, 10 * Years };
    sensiData->swaptionVolShiftTerms() = { 2 * Years, 5 * Years, 10 * Years };
    sensiData->swaptionVolLabel() = "VOL_SWAPTION";

    sensiData->capFloorVolShiftType() = "Relative";
    sensiData->capFloorVolShiftSize() = 0.01;
    sensiData->capFloorVolShiftExpiries() = { 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years };
    sensiData->capFloorVolShiftStrikes() = { 0.05 }; // parallel shifts only
    sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";

    // build scenario generator
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator(
        new SensitivityScenarioGenerator(scenarioFactory, sensiData, simMarketData, today, initMarket));

    // cache initial zero rates
    vector<Period> expiries = simMarketData->swapVolExpiries();
    vector<Period> terms = simMarketData->swapVolTerms();
    vector<vector<Real> > initialData(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<Real> expiryTimes(expiries.size());
    vector<Real> termTimes(terms.size());
    string ccy = simMarketData->ccys()[0];
    Handle<SwaptionVolatilityStructure> ts = initMarket->swaptionVol(ccy);
    DayCounter dc = ts->dayCounter();
    Real strike = 0.0; // FIXME
    for (Size i = 0; i < expiries.size(); ++i)
        expiryTimes[i] = dc.yearFraction(today, today + expiries[i]);
    for (Size j = 0; j < terms.size(); ++j)
        termTimes[j] = dc.yearFraction(today, today + terms[j]);
    for (Size i = 0; i < expiries.size(); ++i) {
        for (Size j = 0; j < terms.size(); ++j)
            initialData[i][j] = ts->volatility(expiries[i], terms[j], strike);
    }

    // apply shifts for tenors on the 2d shift grid
    // collect shifted data at tenors of the underlying 2d grid (different from the grid above)
    // aggregate "observed" shifts
    // compare to expected total shifts
    vector<Period> expiryShiftTenors = sensiData->swaptionVolShiftExpiries();
    vector<Period> termShiftTenors = sensiData->swaptionVolShiftTerms();
    vector<Real> shiftExpiryTimes(expiryShiftTenors.size());
    vector<Real> shiftTermTimes(termShiftTenors.size());
    for (Size i = 0; i < expiryShiftTenors.size(); ++i)
        shiftExpiryTimes[i] = dc.yearFraction(today, today + expiryShiftTenors[i]);
    for (Size j = 0; j < termShiftTenors.size(); ++j)
        shiftTermTimes[j] = dc.yearFraction(today, today + termShiftTenors[j]);

    vector<vector<Real> > shiftedData(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<vector<Real> > diffAbsolute(expiries.size(), vector<Real>(terms.size(), 0.0));
    vector<vector<Real> > diffRelative(expiries.size(), vector<Real>(terms.size(), 0.0));
    Real shiftSize = 0.01; // arbitrary
    SensitivityScenarioGenerator::ShiftType shiftTypeAbsolute = SensitivityScenarioGenerator::ShiftType::Absolute;
    SensitivityScenarioGenerator::ShiftType shiftTypeRelative = SensitivityScenarioGenerator::ShiftType::Relative;
    for (Size i = 0; i < expiryShiftTenors.size(); ++i) {
        for (Size j = 0; j < termShiftTenors.size(); ++j) {
            scenarioGenerator->applyShift(i, j, shiftSize, true, shiftTypeAbsolute, shiftExpiryTimes, shiftTermTimes,
                                          expiryTimes, termTimes, initialData, shiftedData);
            for (Size k = 0; k < expiries.size(); ++k) {
                for (Size l = 0; l < terms.size(); ++l)
                    diffAbsolute[k][l] += shiftedData[k][l] - initialData[k][l];
            }
            scenarioGenerator->applyShift(i, j, shiftSize, true, shiftTypeRelative, shiftExpiryTimes, shiftTermTimes,
                                          expiryTimes, termTimes, initialData, shiftedData);
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
}

test_suite* SensitivityAnalysisTest::suite() {
    // Uncomment the below to get detailed output TODO: custom logger that uses BOOST_MESSAGE

    boost::shared_ptr<ore::data::FileLogger> logger = boost::make_shared<ore::data::FileLogger>("sensitivity.log");
    ore::data::Log::instance().removeAllLoggers();
    ore::data::Log::instance().registerLogger(logger);
    ore::data::Log::instance().switchOn();
    ore::data::Log::instance().setMask(255);

    test_suite* suite = BOOST_TEST_SUITE("SensitivityAnalysisTest");
    // Set the Observation mode here
    ObservationMode::instance().setMode(ObservationMode::Mode::None);
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::test1dShifts));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::test2dShifts));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::testPortfolioSensitivity));
    return suite;
}
}
