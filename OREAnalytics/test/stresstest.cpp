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

#include <test/stresstest.hpp>
#include <test/testmarket.hpp>
#include <test/testportfolio.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/log.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <ored/model/lgmdata.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/all.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ql/time/date.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <boost/timer.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

namespace testsuite {

boost::shared_ptr<data::Conventions> stressConv() {
    boost::shared_ptr<data::Conventions> conventions(new data::Conventions());

    boost::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    // boost::shared_ptr<data::Convention> swapConv(
    //     new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("EUR-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "EUR-EURIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("USD-3M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-3M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("USD-6M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("GBP-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "GBP-LIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("JPY-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "JPY-LIBOR-6M"));
    conventions->add(boost::make_shared<data::IRSwapConvention>("CHF-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "CHF-LIBOR-6M"));

    conventions->add(boost::make_shared<data::DepositConvention>("EUR-DEP-CONVENTIONS", "EUR-EURIBOR"));
    conventions->add(boost::make_shared<data::DepositConvention>("USD-DEP-CONVENTIONS", "USD-LIBOR"));
    conventions->add(boost::make_shared<data::DepositConvention>("GBP-DEP-CONVENTIONS", "GBP-LIBOR"));
    conventions->add(boost::make_shared<data::DepositConvention>("JPY-DEP-CONVENTIONS", "JPY-LIBOR"));
    conventions->add(boost::make_shared<data::DepositConvention>("CHF-DEP-CONVENTIONS", "CHF-LIBOR"));

    return conventions;
}

boost::shared_ptr<analytics::ScenarioSimMarketParameters> setupStressSimMarketData() {
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->yieldCurveTenors() = {1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,  4 * Years,
                                         5 * Years,  7 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};
    simMarketData->indices() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M",
                                "GBP-LIBOR-6M",   "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years};
    simMarketData->swapVolExpiries() = {6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                        5 * Years,  7 * Years, 10 * Years, 20 * Years};
    simMarketData->swapVolCcys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true; // false;

    simMarketData->fxVolExpiries() = {1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years};
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true; // false;
    simMarketData->fxVolCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};

    simMarketData->fxCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};

    simMarketData->simulateCapFloorVols() = true;
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->capFloorVolCcys() = {"EUR", "USD"};
    simMarketData->capFloorVolExpiries() = {6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                                            7 * Years,  10 * Years, 15 * Years, 20 * Years};
    simMarketData->capFloorVolStrikes() = {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06};

    return simMarketData;
}

boost::shared_ptr<StressTestScenarioData> setupStressScenarioData() {
    boost::shared_ptr<StressTestScenarioData> stressData = boost::make_shared<StressTestScenarioData>();

    StressTestScenarioData::StressTestData data;
    data.label = "stresstest_1";
    data.discountCurveShifts["EUR"] = StressTestScenarioData::CurveShiftData();
    data.discountCurveShifts["EUR"].shiftType = "Absolute";
    data.discountCurveShifts["EUR"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                   5 * Years,  7 * Years, 10 * Years};
    data.discountCurveShifts["EUR"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.discountCurveShifts["USD"] = StressTestScenarioData::CurveShiftData();
    data.discountCurveShifts["USD"].shiftType = "Absolute";
    data.discountCurveShifts["USD"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                   5 * Years,  7 * Years, 10 * Years};
    data.discountCurveShifts["USD"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.discountCurveShifts["GBP"] = StressTestScenarioData::CurveShiftData();
    data.discountCurveShifts["GBP"].shiftType = "Absolute";
    data.discountCurveShifts["GBP"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                   5 * Years,  7 * Years, 10 * Years};
    data.discountCurveShifts["GBP"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.discountCurveShifts["JPY"] = StressTestScenarioData::CurveShiftData();
    data.discountCurveShifts["JPY"].shiftType = "Absolute";
    data.discountCurveShifts["JPY"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                   5 * Years,  7 * Years, 10 * Years};
    data.discountCurveShifts["JPY"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.discountCurveShifts["CHF"] = StressTestScenarioData::CurveShiftData();
    data.discountCurveShifts["CHF"].shiftType = "Absolute";
    data.discountCurveShifts["CHF"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                   5 * Years,  7 * Years, 10 * Years};
    data.discountCurveShifts["CHF"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.indexCurveShifts["EUR-EURIBOR-6M"] = StressTestScenarioData::CurveShiftData();
    data.indexCurveShifts["EUR-EURIBOR-6M"].shiftType = "Absolute";
    data.indexCurveShifts["EUR-EURIBOR-6M"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                           5 * Years,  7 * Years, 10 * Years};
    data.indexCurveShifts["EUR-EURIBOR-6M"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.indexCurveShifts["USD-LIBOR-3M"] = StressTestScenarioData::CurveShiftData();
    data.indexCurveShifts["USD-LIBOR-3M"].shiftType = "Absolute";
    data.indexCurveShifts["USD-LIBOR-3M"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                         5 * Years,  7 * Years, 10 * Years};
    data.indexCurveShifts["USD-LIBOR-3M"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.indexCurveShifts["USD-LIBOR-6M"] = StressTestScenarioData::CurveShiftData();
    data.indexCurveShifts["USD-LIBOR-6M"].shiftType = "Absolute";
    data.indexCurveShifts["USD-LIBOR-6M"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                         5 * Years,  7 * Years, 10 * Years};
    data.indexCurveShifts["USD-LIBOR-6M"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.indexCurveShifts["GBP-LIBOR-6M"] = StressTestScenarioData::CurveShiftData();
    data.indexCurveShifts["GBP-LIBOR-6M"].shiftType = "Absolute";
    data.indexCurveShifts["GBP-LIBOR-6M"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                         5 * Years,  7 * Years, 10 * Years};
    data.indexCurveShifts["GBP-LIBOR-6M"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.indexCurveShifts["CHF-LIBOR-6M"] = StressTestScenarioData::CurveShiftData();
    data.indexCurveShifts["CHF-LIBOR-6M"].shiftType = "Absolute";
    data.indexCurveShifts["CHF-LIBOR-6M"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                         5 * Years,  7 * Years, 10 * Years};
    data.indexCurveShifts["CHF-LIBOR-6M"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.indexCurveShifts["JPY-LIBOR-6M"] = StressTestScenarioData::CurveShiftData();
    data.indexCurveShifts["JPY-LIBOR-6M"].shiftType = "Absolute";
    data.indexCurveShifts["JPY-LIBOR-6M"].shiftTenors = {6 * Months, 1 * Years, 2 * Years, 3 * Years,
                                                         5 * Years,  7 * Years, 10 * Years};
    data.indexCurveShifts["JPY-LIBOR-6M"].shifts = {0.001, 0.002, 0.003, 0.004, 0.005, 0.006, 0.007};
    data.fxShifts["EURUSD"] = StressTestScenarioData::FxShiftData();
    data.fxShifts["EURUSD"].shiftType = "Relative";
    data.fxShifts["EURUSD"].shiftSize = 0.01;
    data.fxShifts["EURGBP"] = StressTestScenarioData::FxShiftData();
    data.fxShifts["EURGBP"].shiftType = "Relative";
    data.fxShifts["EURGBP"].shiftSize = 0.01;
    data.fxShifts["EURJPY"] = StressTestScenarioData::FxShiftData();
    data.fxShifts["EURJPY"].shiftType = "Relative";
    data.fxShifts["EURJPY"].shiftSize = 0.01;
    data.fxShifts["EURCHF"] = StressTestScenarioData::FxShiftData();
    data.fxShifts["EURCHF"].shiftType = "Relative";
    data.fxShifts["EURCHF"].shiftSize = 0.01;
    data.fxVolShifts["EURUSD"] = StressTestScenarioData::FxVolShiftData();
    data.fxVolShifts["EURUSD"].shiftType = "Absolute";
    data.fxVolShifts["EURUSD"].shiftExpiries = {6 * Months, 2 * Years, 3 * Years, 5 * Years};
    data.fxVolShifts["EURUSD"].shifts = {0.10, 0.11, 0.13, 0.14};
    data.fxVolShifts["EURGBP"] = StressTestScenarioData::FxVolShiftData();
    data.fxVolShifts["EURGBP"].shiftType = "Absolute";
    data.fxVolShifts["EURGBP"].shiftExpiries = {6 * Months, 2 * Years, 3 * Years, 5 * Years};
    data.fxVolShifts["EURGBP"].shifts = {0.10, 0.11, 0.13, 0.14};
    data.fxVolShifts["EURJPY"] = StressTestScenarioData::FxVolShiftData();
    data.fxVolShifts["EURJPY"].shiftType = "Absolute";
    data.fxVolShifts["EURJPY"].shiftExpiries = {6 * Months, 2 * Years, 3 * Years, 5 * Years};
    data.fxVolShifts["EURJPY"].shifts = {0.10, 0.11, 0.13, 0.14};
    data.fxVolShifts["EURCHF"] = StressTestScenarioData::FxVolShiftData();
    data.fxVolShifts["EURCHF"].shiftType = "Absolute";
    data.fxVolShifts["EURCHF"].shiftExpiries = {6 * Months, 2 * Years, 3 * Years, 5 * Years};
    data.fxVolShifts["EURCHF"].shifts = {0.10, 0.11, 0.13, 0.14};

    stressData->data() = vector<StressTestScenarioData::StressTestData>(1, data);

    return stressData;
}

void StressTestingTest::regression() {
    BOOST_TEST_MESSAGE("Testing Sensitivity Par Conversion");

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
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupStressSimMarketData();

    // sensitivity config
    boost::shared_ptr<StressTestScenarioData> stressData = setupStressScenarioData();

    // build scenario generator
    boost::shared_ptr<StressScenarioGenerator> scenarioGenerator(
        new StressScenarioGenerator(stressData, simMarketData, today, initMarket));
    boost::shared_ptr<Scenario> baseScen = scenarioGenerator->baseScenario();
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new CloneScenarioFactory(baseScen));
    scenarioGenerator->generateScenarios(scenarioFactory);
    boost::shared_ptr<ScenarioGenerator> sgen(scenarioGenerator);

    // build scenario sim market
    Conventions conventions = *stressConv();
    boost::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        boost::make_shared<analytics::ScenarioSimMarket>(sgen, initMarket, simMarketData, conventions);

    // build porfolio
    boost::shared_ptr<EngineData> engineData = boost::make_shared<EngineData>();
    engineData->model("Swap") = "DiscountedCashflows";
    engineData->engine("Swap") = "DiscountingSwapEngine";
    engineData->model("CrossCurrencySwap") = "DiscountedCashflows";
    engineData->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    engineData->model("EuropeanSwaption") = "BlackBachelier";
    engineData->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    engineData->model("FxForward") = "DiscountedCashflows";
    engineData->engine("FxForward") = "DiscountingFxForwardEngine";
    engineData->model("FxOption") = "GarmanKohlhagen";
    engineData->engine("FxOption") = "AnalyticEuropeanEngine";
    engineData->model("CapFloor") = "IborCapModel";
    engineData->engine("CapFloor") = "IborCapEngine";
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(engineData, simMarket);
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
    portfolio->add(buildCap("9_Cap_EUR", "EUR", "Long", 0.05, 1000000.0, 0, 10, "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildFloor("10_Floor_USD", "USD", "Long", 0.01, 1000000.0, 0, 10, "3M", "A360", "USD-LIBOR-3M"));
    portfolio->build(factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // build the sensitivity analysis object
    ore::analytics::StressTest analysis(portfolio, initMarket, "default", engineData, simMarketData, stressData,
                                        conventions);

    std::map<std::string, Real> baseNPV = analysis.baseNPV();
    std::map<std::pair<std::string, std::string>, Real> shiftedNPV = analysis.shiftedNPV();

    QL_REQUIRE(shiftedNPV.size() > 0, "no shifted results");

    struct Results {
        string id;
        string label;
        Real shift;
    };

    std::vector<Results> cachedResults = {{"10_Floor_USD", "stresstest_1", -2487.75},
                                          {"1_Swap_EUR", "stresstest_1", 629406},
                                          {"2_Swap_USD", "stresstest_1", 599846},
                                          {"3_Swap_GBP", "stresstest_1", 1.11005e+06},
                                          {"4_Swap_JPY", "stresstest_1", 186736},
                                          {"5_Swaption_EUR", "stresstest_1", 20768.2},
                                          {"6_Swaption_EUR", "stresstest_1", 8247.32},
                                          {"7_FxOption_EUR_USD", "stresstest_1", 748160},
                                          {"8_FxOption_EUR_GBP", "stresstest_1", 1.21724e+06},
                                          {"9_Cap_EUR", "stresstest_1", 1175.5}};

    std::map<pair<string, string>, Real> stressMap;
    for (Size i = 0; i < cachedResults.size(); ++i) {
        pair<string, string> p(cachedResults[i].id, cachedResults[i].label);
        stressMap[p] = cachedResults[i].shift;
    }

    Real tolerance = 0.01;
    Size count = 0;
    for (auto data : shiftedNPV) {
        pair<string, string> p = data.first;
        string id = data.first.first;
        string label = data.first.second;
        Real npv = data.second;
        QL_REQUIRE(baseNPV.find(id) != baseNPV.end(), "base npv not found for trade " << id);
        Real base = baseNPV[id];
        Real delta = npv - base;
        if (fabs(delta) > 0.0) {
            count++;
            // BOOST_TEST_MESSAGE("{ \"" << id << "\", \"" << label << "\", " << delta << " },");
            QL_REQUIRE(stressMap.find(p) != stressMap.end(), "pair (" << p.first << ", " << p.second
                                                                      << ") not found in sensi map");
            BOOST_CHECK_MESSAGE(fabs(delta - stressMap[p]) < tolerance ||
                                    fabs((delta - stressMap[p]) / delta) < tolerance,
                                "stress test regression failed for pair (" << p.first << ", " << p.second
                                                                           << "): " << delta << " vs " << stressMap[p]);
        }
    }
    BOOST_CHECK_MESSAGE(count == cachedResults.size(), "number of non-zero stress impacts ("
                                                           << count << ") do not match regression data ("
                                                           << cachedResults.size() << ")");
    IndexManager::instance().clearHistories();
}

test_suite* StressTestingTest::suite() {
    // Uncomment the below to get detailed output TODO: custom logger that uses BOOST_MESSAGE
    /*
    boost::shared_ptr<ore::data::FileLogger> logger = boost::make_shared<ore::data::FileLogger>("stresstest.log");
    ore::data::Log::instance().removeAllLoggers();
    ore::data::Log::instance().registerLogger(logger);
    ore::data::Log::instance().switchOn();
    ore::data::Log::instance().setMask(255);
    */
    test_suite* suite = BOOST_TEST_SUITE("StressTestingTest");
    // Set the Observation mode here
    ObservationMode::instance().setMode(ObservationMode::Mode::None);
    suite->add(BOOST_TEST_CASE(&StressTestingTest::regression));
    return suite;
}
}
