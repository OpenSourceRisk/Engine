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
#include <orea/engine/observationmode.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/engine/all.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/fxoption.hpp>
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

boost::shared_ptr<data::Conventions> conv() {
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

boost::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData2() {
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = { "EUR", "GBP" };
    simMarketData->yieldCurveNames() = { "BondCurve1" };
    simMarketData->yieldCurveTenors() = { 1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years, 4 * Years,
                                          5 * Years,  6 * Years,  7 * Years,  8 * Years,  9 * Years, 10 * Years,
                                          12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years };
    simMarketData->indices() = { "EUR-EURIBOR-6M", "GBP-LIBOR-6M" };
    simMarketData->defaultNames() = { "BondIssuer1" };
    simMarketData->defaultTenors() = { 1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years, 4 * Years,
                                       5 * Years,  7 * Years,  10 * Years, 20 * Years, 30 * Years };
    simMarketData->securities() = { "Bond1" };

    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = { 1 * Years, 2 * Years, 3 * Years,  4 * Years,
                                      5 * Years, 7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolExpiries() = { 6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                         5 * Years,  7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolCcys() = { "EUR", "GBP" };
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true;

    simMarketData->fxVolExpiries() = { 1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years };
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true;
    simMarketData->fxVolCcyPairs() = { "EURGBP" };

    simMarketData->fxCcyPairs() = { "EURGBP" };

    simMarketData->simulateCapFloorVols() = false;

    return simMarketData;
}

boost::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData5() {
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = { "EUR", "GBP", "USD", "CHF", "JPY" };
    simMarketData->yieldCurveTenors() = { 1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,  4 * Years,
                                          5 * Years,  7 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years };
    simMarketData->indices() = { "EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M",
                                 "GBP-LIBOR-6M",   "CHF-LIBOR-6M", "JPY-LIBOR-6M" };
    simMarketData->yieldCurveNames() = { "BondCurve1" };
    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = { 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolExpiries() = { 6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                         5 * Years,  7 * Years, 10 * Years, 20 * Years };
    simMarketData->swapVolCcys() = { "EUR", "GBP", "USD", "CHF", "JPY" };
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true; // false;

    simMarketData->fxVolExpiries() = { 1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years };
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true; // false;
    simMarketData->fxVolCcyPairs() = { "EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF" };

    simMarketData->fxCcyPairs() = { "EURUSD", "EURGBP", "EURCHF", "EURJPY" };

    simMarketData->simulateCapFloorVols() = true;
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->capFloorVolCcys() = { "EUR", "USD" };
    simMarketData->capFloorVolExpiries() = { 6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                                             7 * Years,  10 * Years, 15 * Years, 20 * Years };
    simMarketData->capFloorVolStrikes() = { 0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06 };

    simMarketData->defaultNames() = { "BondIssuer1" };
    simMarketData->defaultTenors() = { 1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years, 4 * Years,
                                       5 * Years,  7 * Years,  10 * Years, 20 * Years, 30 * Years };
    simMarketData->securities() = { "Bond1" };

    return simMarketData;
}

boost::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData2() {
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();

    SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {
        1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years
    }; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    SensitivityScenarioData::FxShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    SensitivityScenarioData::FxVolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = { 2 * Years, 5 * Years };

    SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = { 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years };
    cfvsData.shiftStrikes = { 0.05 };

    SensitivityScenarioData::SwaptionVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = { 3 * Years, 5 * Years, 10 * Years };
    swvsData.shiftTerms = { 2 * Years, 5 * Years, 10 * Years };

    sensiData->discountCurrencies() = { "EUR", "GBP" };
    sensiData->discountCurveShiftData()["EUR"] = cvsData;

    sensiData->discountCurveShiftData()["GBP"] = cvsData;

    sensiData->indexNames() = { "EUR-EURIBOR-6M", "GBP-LIBOR-6M" };
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;

    sensiData->yieldCurveNames() = { "BondCurve1" };
    sensiData->yieldCurveShiftData()["BondCurve1"] = cvsData;

    sensiData->fxCcyPairs() = { "EURGBP" };
    sensiData->fxShiftData()["EURGBP"] = fxsData;

    sensiData->fxVolCcyPairs() = { "EURGBP" };
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;

    sensiData->swaptionVolCurrencies() = { "EUR", "GBP" };
    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;

    // sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";
    // sensiData->capFloorVolCurrencies() = { "EUR", "GBP" };
    // sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    // sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    // sensiData->capFloorVolShiftData()["GBP"] = cfvsData;
    // sensiData->capFloorVolShiftData()["GBP"].indexName = "GBP-LIBOR-6M";

    return sensiData;
}

boost::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5() {
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();

    SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = { 6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                            7 * Years,  10 * Years, 15 * Years, 20 * Years }; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;

    SensitivityScenarioData::FxShiftData fxsData;
    fxsData.shiftType = "Relative";
    fxsData.shiftSize = 0.01;

    SensitivityScenarioData::FxVolShiftData fxvsData;
    fxvsData.shiftType = "Relative";
    fxvsData.shiftSize = 1.0;
    fxvsData.shiftExpiries = { 5 * Years };

    SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 0.0001;
    cfvsData.shiftExpiries = { 1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years };
    cfvsData.shiftStrikes = { 0.01, 0.02, 0.03, 0.04, 0.05 };

    SensitivityScenarioData::SwaptionVolShiftData swvsData;
    swvsData.shiftType = "Relative";
    swvsData.shiftSize = 0.01;
    swvsData.shiftExpiries = { 2 * Years, 5 * Years, 10 * Years };
    swvsData.shiftTerms = { 5 * Years, 10 * Years };

    sensiData->discountCurrencies() = { "EUR", "USD", "GBP", "CHF", "JPY" };
    sensiData->discountCurveShiftData()["EUR"] = cvsData;

    sensiData->discountCurveShiftData()["USD"] = cvsData;

    sensiData->discountCurveShiftData()["GBP"] = cvsData;

    sensiData->discountCurveShiftData()["JPY"] = cvsData;

    sensiData->discountCurveShiftData()["CHF"] = cvsData;

    sensiData->indexNames() = { "EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M" };
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] = cvsData;

    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] = cvsData;

    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] = cvsData;

    sensiData->yieldCurveNames() = { "BondCurve1" };
    sensiData->yieldCurveShiftData()["BondCurve1"] = cvsData;

    sensiData->fxCcyPairs() = { "EURUSD", "EURGBP", "EURCHF", "EURJPY" };
    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolCcyPairs() = { "EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF" };
    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;
    sensiData->fxVolShiftData()["GBPCHF"] = fxvsData;

    sensiData->swaptionVolCurrencies() = { "EUR", "USD", "GBP", "CHF", "JPY" };
    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;
    sensiData->swaptionVolShiftData()["USD"] = swvsData;
    sensiData->swaptionVolShiftData()["JPY"] = swvsData;
    sensiData->swaptionVolShiftData()["CHF"] = swvsData;

    sensiData->capFloorVolCurrencies() = { "EUR", "USD" };
    sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] = cfvsData;
    sensiData->capFloorVolShiftData()["USD"].indexName = "USD-LIBOR-3M";

    return sensiData;
}

void testPortfolioSensitivity(ObservationMode::Mode om) {

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(om);

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    boost::shared_ptr<Market> initMarket = boost::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5();

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
    data->model("Bond") = "DiscountedCashflows";
    data->engine("Bond") = "DiscountingRiskyBondEngine";
    data->engineParameters("Bond")["TimestepPeriod"] = "6M";
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
    portfolio->add(buildCap("9_Cap_EUR", "EUR", "Long", 0.05, 1000000.0, 0, 10, "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildFloor("10_Floor_USD", "USD", "Long", 0.01, 1000000.0, 0, 10, "3M", "A360", "USD-LIBOR-3M"));
    portfolio->add(buildZeroBond("11_ZeroBond_EUR", "EUR", 1.0, 10));
    portfolio->add(buildZeroBond("12_ZeroBond_USD", "USD", 1.0, 10));
    portfolio->build(factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // build the scenario valuation engine
    boost::shared_ptr<DateGrid> dg = boost::make_shared<DateGrid>(
        "1,0W"); // TODO - extend the DateGrid interface so that it can actually take a vector of dates as input
    vector<boost::shared_ptr<ValuationCalculator> > calculators;
    calculators.push_back(boost::make_shared<NPVCalculator>(simMarketData->baseCcy()));
    ValuationEngine engine(today, dg, simMarket);
    // run scenarios and fill the cube
    boost::timer t;
    boost::shared_ptr<NPVCube> cube = boost::make_shared<DoublePrecisionInMemoryCube>(
        today, portfolio->ids(), vector<Date>(1, today), scenarioGenerator->samples());
    engine.buildCube(portfolio, cube, calculators);
    double elapsed = t.elapsed();

    struct Results {
        string id;
        string label;
        Real npv;
        Real sensi;
    };

    std::vector<Results> cachedResults = {
        { "1_Swap_EUR", "Up:DiscountCurve/EUR/0/6M", -928826, -2.51631 },
        { "1_Swap_EUR", "Up:DiscountCurve/EUR/1/1Y", -928826, 14.6846 },
        { "1_Swap_EUR", "Up:DiscountCurve/EUR/2/2Y", -928826, 19.0081 },
        { "1_Swap_EUR", "Up:DiscountCurve/EUR/3/3Y", -928826, 46.1186 },
        { "1_Swap_EUR", "Up:DiscountCurve/EUR/4/5Y", -928826, 85.1033 },
        { "1_Swap_EUR", "Up:DiscountCurve/EUR/5/7Y", -928826, 149.43 },
        { "1_Swap_EUR", "Up:DiscountCurve/EUR/6/10Y", -928826, 205.064 },
        { "1_Swap_EUR", "Down:DiscountCurve/EUR/0/6M", -928826, 2.51644 },
        { "1_Swap_EUR", "Down:DiscountCurve/EUR/1/1Y", -928826, -14.6863 },
        { "1_Swap_EUR", "Down:DiscountCurve/EUR/2/2Y", -928826, -19.0137 },
        { "1_Swap_EUR", "Down:DiscountCurve/EUR/3/3Y", -928826, -46.1338 },
        { "1_Swap_EUR", "Down:DiscountCurve/EUR/4/5Y", -928826, -85.1406 },
        { "1_Swap_EUR", "Down:DiscountCurve/EUR/5/7Y", -928826, -149.515 },
        { "1_Swap_EUR", "Down:DiscountCurve/EUR/6/10Y", -928826, -205.239 },
        { "1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/0/6M", -928826, -495.013 },
        { "1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/1/1Y", -928826, 14.7304 },
        { "1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", -928826, 38.7816 },
        { "1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", -928826, 94.186 },
        { "1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", -928826, 173.125 },
        { "1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", -928826, 304.648 },
        { "1_Swap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", -928826, 8479.55 },
        { "1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/0/6M", -928826, 495.037 },
        { "1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/1/1Y", -928826, -14.5864 },
        { "1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", -928826, -38.4045 },
        { "1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", -928826, -93.532 },
        { "1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", -928826, -171.969 },
        { "1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", -928826, -302.864 },
        { "1_Swap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", -928826, -8478.14 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/0/6M", 980404, -1.04797 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/1/1Y", 980404, -6.06931 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/2/2Y", 980404, -15.8605 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/3/3Y", 980404, -38.0708 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/4/5Y", 980404, -68.7288 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/5/7Y", 980404, -118.405 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/6/10Y", 980404, -244.946 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/7/15Y", 980404, -202.226 },
        { "2_Swap_USD", "Up:DiscountCurve/USD/8/20Y", 980404, 0.0148314 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/0/6M", 980404, 1.04797 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/1/1Y", 980404, 6.06959 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/2/2Y", 980404, 15.8623 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/3/3Y", 980404, 38.0784 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/4/5Y", 980404, 68.7502 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/5/7Y", 980404, 118.458 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/6/10Y", 980404, 245.108 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/7/15Y", 980404, 202.42 },
        { "2_Swap_USD", "Down:DiscountCurve/USD/8/20Y", 980404, -0.0148314 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/0/6M", 980404, -201.015 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/1/1Y", 980404, 18.134 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/2/2Y", 980404, 47.3066 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/3/3Y", 980404, 113.4 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/4/5Y", 980404, 205.068 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/5/7Y", 980404, 352.859 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/6/10Y", 980404, 730.076 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/7/15Y", 980404, 8626.78 },
        { "2_Swap_USD", "Up:IndexCurve/USD-LIBOR-3M/8/20Y", 980404, 5.86437 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/0/6M", 980404, 201.03 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/1/1Y", 980404, -18.0746 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/2/2Y", 980404, -47.1526 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/3/3Y", 980404, -113.136 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/4/5Y", 980404, -204.611 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/5/7Y", 980404, -352.166 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/6/10Y", 980404, -729.248 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/7/15Y", 980404, -8626.13 },
        { "2_Swap_USD", "Down:IndexCurve/USD-LIBOR-3M/8/20Y", 980404, -5.86436 },
        { "2_Swap_USD", "Up:FXSpot/EURUSD/0/spot", 980404, -9706.97 },
        { "2_Swap_USD", "Down:FXSpot/EURUSD/0/spot", 980404, 9903.07 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/0/6M", 69795.3, 2.12392 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/1/1Y", 69795.3, -0.646097 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/2/2Y", 69795.3, -1.75066 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/3/3Y", 69795.3, -4.24827 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/4/5Y", 69795.3, -7.2252 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/5/7Y", 69795.3, -12.5287 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/6/10Y", 69795.3, -24.7828 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/7/15Y", 69795.3, -39.2456 },
        { "3_Swap_GBP", "Up:DiscountCurve/GBP/8/20Y", 69795.3, 31.2081 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/0/6M", 69795.3, -2.12413 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/1/1Y", 69795.3, 0.645698 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/2/2Y", 69795.3, 1.74981 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/3/3Y", 69795.3, 4.2473 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/4/5Y", 69795.3, 7.22426 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/5/7Y", 69795.3, 12.5298 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/6/10Y", 69795.3, 24.7939 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/7/15Y", 69795.3, 39.2773 },
        { "3_Swap_GBP", "Down:DiscountCurve/GBP/8/20Y", 69795.3, -31.2925 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/0/6M", 69795.3, -308.49 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/1/1Y", 69795.3, 68.819 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/2/2Y", 69795.3, 81.3735 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/3/3Y", 69795.3, 239.034 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/4/5Y", 69795.3, 372.209 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/5/7Y", 69795.3, 654.949 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/6/10Y", 69795.3, 1343.01 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/7/15Y", 69795.3, 2139.68 },
        { "3_Swap_GBP", "Up:IndexCurve/GBP-LIBOR-6M/8/20Y", 69795.3, 12633.8 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/0/6M", 69795.3, 308.513 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/1/1Y", 69795.3, -68.7287 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/2/2Y", 69795.3, -81.1438 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/3/3Y", 69795.3, -238.649 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/4/5Y", 69795.3, -371.553 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/5/7Y", 69795.3, -653.972 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/6/10Y", 69795.3, -1341.88 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/7/15Y", 69795.3, -2138.11 },
        { "3_Swap_GBP", "Down:IndexCurve/GBP-LIBOR-6M/8/20Y", 69795.3, -12632.5 },
        { "3_Swap_GBP", "Up:FXSpot/EURGBP/0/spot", 69795.3, -691.043 },
        { "3_Swap_GBP", "Down:FXSpot/EURGBP/0/spot", 69795.3, 705.003 },
        { "4_Swap_JPY", "Up:DiscountCurve/JPY/0/6M", 871.03, -0.00750246 },
        { "4_Swap_JPY", "Up:DiscountCurve/JPY/1/1Y", 871.03, -0.00147994 },
        { "4_Swap_JPY", "Up:DiscountCurve/JPY/2/2Y", 871.03, -0.020079 },
        { "4_Swap_JPY", "Up:DiscountCurve/JPY/3/3Y", 871.03, -0.0667249 },
        { "4_Swap_JPY", "Up:DiscountCurve/JPY/4/5Y", 871.03, 4.75708 },
        { "4_Swap_JPY", "Down:DiscountCurve/JPY/0/6M", 871.03, 0.00747801 },
        { "4_Swap_JPY", "Down:DiscountCurve/JPY/1/1Y", 871.03, 0.00140807 },
        { "4_Swap_JPY", "Down:DiscountCurve/JPY/2/2Y", 871.03, 0.0199001 },
        { "4_Swap_JPY", "Down:DiscountCurve/JPY/3/3Y", 871.03, 0.0664106 },
        { "4_Swap_JPY", "Down:DiscountCurve/JPY/4/5Y", 871.03, -4.75978 },
        { "4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/0/6M", 871.03, -193.514 },
        { "4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/1/1Y", 871.03, 2.95767 },
        { "4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/2/2Y", 871.03, 7.81453 },
        { "4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/3/3Y", 871.03, 19.3576 },
        { "4_Swap_JPY", "Up:IndexCurve/JPY-LIBOR-6M/4/5Y", 871.03, 3832.83 },
        { "4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/0/6M", 871.03, 193.528 },
        { "4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/1/1Y", 871.03, -2.90067 },
        { "4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/2/2Y", 871.03, -7.6631 },
        { "4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/3/3Y", 871.03, -19.0907 },
        { "4_Swap_JPY", "Down:IndexCurve/JPY-LIBOR-6M/4/5Y", 871.03, -3832.59 },
        { "4_Swap_JPY", "Up:FXSpot/EURJPY/0/spot", 871.03, -8.62406 },
        { "4_Swap_JPY", "Down:FXSpot/EURJPY/0/spot", 871.03, 8.79829 },
        { "5_Swaption_EUR", "Up:DiscountCurve/EUR/6/10Y", 18027.1, -1.33793 },
        { "5_Swaption_EUR", "Up:DiscountCurve/EUR/7/15Y", 18027.1, 0.197251 },
        { "5_Swaption_EUR", "Up:DiscountCurve/EUR/8/20Y", 18027.1, 2.41317 },
        { "5_Swaption_EUR", "Down:DiscountCurve/EUR/6/10Y", 18027.1, 1.33862 },
        { "5_Swaption_EUR", "Down:DiscountCurve/EUR/7/15Y", 18027.1, -0.197718 },
        { "5_Swaption_EUR", "Down:DiscountCurve/EUR/8/20Y", 18027.1, -2.41555 },
        { "5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 18027.1, -266.802 },
        { "5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/7/15Y", 18027.1, 38.2821 },
        { "5_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/8/20Y", 18027.1, 487.065 },
        { "5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 18027.1, 268.413 },
        { "5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/7/15Y", 18027.1, -38.1352 },
        { "5_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/8/20Y", 18027.1, -481.778 },
        { "5_Swaption_EUR", "Up:SwaptionVolatility/EUR/5/10Y/10Y/ATM", 18027.1, 357.634 },
        { "5_Swaption_EUR", "Down:SwaptionVolatility/EUR/5/10Y/10Y/ATM", 18027.1, -356.596 },
        { "6_Swaption_EUR", "Up:DiscountCurve/EUR/2/2Y", 1156.84, -0.0976825 },
        { "6_Swaption_EUR", "Up:DiscountCurve/EUR/3/3Y", 1156.84, 0.00251532 },
        { "6_Swaption_EUR", "Up:DiscountCurve/EUR/4/5Y", 1156.84, 0.00988176 },
        { "6_Swaption_EUR", "Up:DiscountCurve/EUR/5/7Y", 1156.84, 0.320953 },
        { "6_Swaption_EUR", "Up:DiscountCurve/EUR/6/10Y", 1156.84, 0.00247921 },
        { "6_Swaption_EUR", "Down:DiscountCurve/EUR/2/2Y", 1156.84, 0.0976982 },
        { "6_Swaption_EUR", "Down:DiscountCurve/EUR/3/3Y", 1156.84, -0.00254931 },
        { "6_Swaption_EUR", "Down:DiscountCurve/EUR/4/5Y", 1156.84, -0.00993997 },
        { "6_Swaption_EUR", "Down:DiscountCurve/EUR/5/7Y", 1156.84, -0.321034 },
        { "6_Swaption_EUR", "Down:DiscountCurve/EUR/6/10Y", 1156.84, -0.00247921 },
        { "6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", 1156.84, -19.525 },
        { "6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", 1156.84, 0.809505 },
        { "6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", 1156.84, 1.79161 },
        { "6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", 1156.84, 65.6802 },
        { "6_Swaption_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 1156.84, 0.248765 },
        { "6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", 1156.84, 19.7717 },
        { "6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", 1156.84, -0.80235 },
        { "6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", 1156.84, -1.77773 },
        { "6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", 1156.84, -63.0435 },
        { "6_Swaption_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 1156.84, -0.248725 },
        { "6_Swaption_EUR", "Up:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 1156.84, 47.3455 },
        { "6_Swaption_EUR", "Up:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 1156.84, 0.0858115 },
        { "6_Swaption_EUR", "Down:SwaptionVolatility/EUR/0/2Y/5Y/ATM", 1156.84, -46.4415 },
        { "6_Swaption_EUR", "Down:SwaptionVolatility/EUR/2/5Y/5Y/ATM", 1156.84, -0.0858084 },
        { "7_FxOption_EUR_USD", "Up:DiscountCurve/EUR/3/3Y", 1.36968e+06, -2107.81 },
        { "7_FxOption_EUR_USD", "Up:DiscountCurve/EUR/4/5Y", 1.36968e+06, -3.85768 },
        { "7_FxOption_EUR_USD", "Up:DiscountCurve/USD/3/3Y", 1.36968e+06, 1698.91 },
        { "7_FxOption_EUR_USD", "Up:DiscountCurve/USD/4/5Y", 1.36968e+06, 3.10717 },
        { "7_FxOption_EUR_USD", "Down:DiscountCurve/EUR/3/3Y", 1.36968e+06, 2109.74 },
        { "7_FxOption_EUR_USD", "Down:DiscountCurve/EUR/4/5Y", 1.36968e+06, 3.85768 },
        { "7_FxOption_EUR_USD", "Down:DiscountCurve/USD/3/3Y", 1.36968e+06, -1698.12 },
        { "7_FxOption_EUR_USD", "Down:DiscountCurve/USD/4/5Y", 1.36968e+06, -3.10717 },
        { "7_FxOption_EUR_USD", "Up:FXSpot/EURUSD/0/spot", 1.36968e+06, 56850.7 },
        { "7_FxOption_EUR_USD", "Down:FXSpot/EURUSD/0/spot", 1.36968e+06, -56537.6 },
        { "7_FxOption_EUR_USD", "Up:FXVolatility/EURUSD/0/5Y/ATM", 1.36968e+06, 672236 },
        { "7_FxOption_EUR_USD", "Down:FXVolatility/EURUSD/0/5Y/ATM", 1.36968e+06, -329688 },
        { "8_FxOption_EUR_GBP", "Up:DiscountCurve/EUR/5/7Y", 798336, -2435.22 },
        { "8_FxOption_EUR_GBP", "Up:DiscountCurve/GBP/5/7Y", 798336, 1880.89 },
        { "8_FxOption_EUR_GBP", "Down:DiscountCurve/EUR/5/7Y", 798336, 2441.08 },
        { "8_FxOption_EUR_GBP", "Down:DiscountCurve/GBP/5/7Y", 798336, -1878.05 },
        { "8_FxOption_EUR_GBP", "Up:FXSpot/EURGBP/0/spot", 798336, 27009.9 },
        { "8_FxOption_EUR_GBP", "Down:FXSpot/EURGBP/0/spot", 798336, -26700.2 },
        { "8_FxOption_EUR_GBP", "Up:FXVolatility/EURGBP/0/5Y/ATM", 798336, 1.36635e+06 },
        { "8_FxOption_EUR_GBP", "Down:FXVolatility/EURGBP/0/5Y/ATM", 798336, -798336 },
        { "9_Cap_EUR", "Up:DiscountCurve/EUR/2/2Y", 289.105, -7.28588e-07 },
        { "9_Cap_EUR", "Up:DiscountCurve/EUR/3/3Y", 289.105, -0.000381869 },
        { "9_Cap_EUR", "Up:DiscountCurve/EUR/4/5Y", 289.105, -0.00790528 },
        { "9_Cap_EUR", "Up:DiscountCurve/EUR/5/7Y", 289.105, -0.0764893 },
        { "9_Cap_EUR", "Up:DiscountCurve/EUR/6/10Y", 289.105, -0.162697 },
        { "9_Cap_EUR", "Down:DiscountCurve/EUR/2/2Y", 289.105, 7.28664e-07 },
        { "9_Cap_EUR", "Down:DiscountCurve/EUR/3/3Y", 289.105, 0.000381934 },
        { "9_Cap_EUR", "Down:DiscountCurve/EUR/4/5Y", 289.105, 0.00790776 },
        { "9_Cap_EUR", "Down:DiscountCurve/EUR/5/7Y", 289.105, 0.0765231 },
        { "9_Cap_EUR", "Down:DiscountCurve/EUR/6/10Y", 289.105, 0.162824 },
        { "9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/1/1Y", 289.105, -1.81582e-05 },
        { "9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/2/2Y", 289.105, -0.00670729 },
        { "9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/3/3Y", 289.105, -0.330895 },
        { "9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/4/5Y", 289.105, -2.03937 },
        { "9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/5/7Y", 289.105, -6.42991 },
        { "9_Cap_EUR", "Up:IndexCurve/EUR-EURIBOR-6M/6/10Y", 289.105, 15.5182 },
        { "9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/1/1Y", 289.105, 1.97218e-05 },
        { "9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/2/2Y", 289.105, 0.00746096 },
        { "9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/3/3Y", 289.105, 0.353405 },
        { "9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/4/5Y", 289.105, 2.24481 },
        { "9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/5/7Y", 289.105, 7.1522 },
        { "9_Cap_EUR", "Down:IndexCurve/EUR-EURIBOR-6M/6/10Y", 289.105, -14.6675 },
        { "9_Cap_EUR", "Up:OptionletVolatility/EUR/4/1Y/0.05", 289.105, 8.49293e-05 },
        { "9_Cap_EUR", "Up:OptionletVolatility/EUR/9/2Y/0.05", 289.105, 0.0150901 },
        { "9_Cap_EUR", "Up:OptionletVolatility/EUR/14/3Y/0.05", 289.105, 0.620393 },
        { "9_Cap_EUR", "Up:OptionletVolatility/EUR/19/5Y/0.05", 289.105, 17.2057 },
        { "9_Cap_EUR", "Up:OptionletVolatility/EUR/24/10Y/0.05", 289.105, 24.4267 },
        { "9_Cap_EUR", "Down:OptionletVolatility/EUR/4/1Y/0.05", 289.105, -6.97789e-05 },
        { "9_Cap_EUR", "Down:OptionletVolatility/EUR/9/2Y/0.05", 289.105, -0.0125099 },
        { "9_Cap_EUR", "Down:OptionletVolatility/EUR/14/3Y/0.05", 289.105, -0.554344 },
        { "9_Cap_EUR", "Down:OptionletVolatility/EUR/19/5Y/0.05", 289.105, -16.1212 },
        { "9_Cap_EUR", "Down:OptionletVolatility/EUR/24/10Y/0.05", 289.105, -23.0264 },
        { "10_Floor_USD", "Up:DiscountCurve/USD/0/6M", 3406.46, -7.03494e-09 },
        { "10_Floor_USD", "Up:DiscountCurve/USD/1/1Y", 3406.46, -8.41429e-05 },
        { "10_Floor_USD", "Up:DiscountCurve/USD/2/2Y", 3406.46, -0.00329744 },
        { "10_Floor_USD", "Up:DiscountCurve/USD/3/3Y", 3406.46, -0.053884 },
        { "10_Floor_USD", "Up:DiscountCurve/USD/4/5Y", 3406.46, -0.269714 },
        { "10_Floor_USD", "Up:DiscountCurve/USD/5/7Y", 3406.46, -0.989583 },
        { "10_Floor_USD", "Up:DiscountCurve/USD/6/10Y", 3406.46, -1.26544 },
        { "10_Floor_USD", "Down:DiscountCurve/USD/0/6M", 3406.46, 7.0354e-09 },
        { "10_Floor_USD", "Down:DiscountCurve/USD/1/1Y", 3406.46, 8.41464e-05 },
        { "10_Floor_USD", "Down:DiscountCurve/USD/2/2Y", 3406.46, 0.00329786 },
        { "10_Floor_USD", "Down:DiscountCurve/USD/3/3Y", 3406.46, 0.0538949 },
        { "10_Floor_USD", "Down:DiscountCurve/USD/4/5Y", 3406.46, 0.269802 },
        { "10_Floor_USD", "Down:DiscountCurve/USD/5/7Y", 3406.46, 0.990038 },
        { "10_Floor_USD", "Down:DiscountCurve/USD/6/10Y", 3406.46, 1.26635 },
        { "10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/0/6M", 3406.46, 0.00150733 },
        { "10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/1/1Y", 3406.46, 0.240284 },
        { "10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/2/2Y", 3406.46, 2.17175 },
        { "10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/3/3Y", 3406.46, 7.77249 },
        { "10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/4/5Y", 3406.46, 12.9642 },
        { "10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/5/7Y", 3406.46, 16.8269 },
        { "10_Floor_USD", "Up:IndexCurve/USD-LIBOR-3M/6/10Y", 3406.46, -81.4363 },
        { "10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/0/6M", 3406.46, -0.00139804 },
        { "10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/1/1Y", 3406.46, -0.230558 },
        { "10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/2/2Y", 3406.46, -2.00123 },
        { "10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/3/3Y", 3406.46, -7.14862 },
        { "10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/4/5Y", 3406.46, -11.2003 },
        { "10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/5/7Y", 3406.46, -13.7183 },
        { "10_Floor_USD", "Down:IndexCurve/USD-LIBOR-3M/6/10Y", 3406.46, 84.0113 },
        { "10_Floor_USD", "Up:FXSpot/EURUSD/0/spot", 3406.46, -33.7273 },
        { "10_Floor_USD", "Down:FXSpot/EURUSD/0/spot", 3406.46, 34.4087 },
        { "10_Floor_USD", "Up:OptionletVolatility/USD/0/1Y/0.01", 3406.46, 0.402913 },
        { "10_Floor_USD", "Up:OptionletVolatility/USD/5/2Y/0.01", 3406.46, 3.32861 },
        { "10_Floor_USD", "Up:OptionletVolatility/USD/10/3Y/0.01", 3406.46, 16.8798 },
        { "10_Floor_USD", "Up:OptionletVolatility/USD/15/5Y/0.01", 3406.46, 96.415 },
        { "10_Floor_USD", "Up:OptionletVolatility/USD/20/10Y/0.01", 3406.46, 92.2212 },
        { "10_Floor_USD", "Down:OptionletVolatility/USD/0/1Y/0.01", 3406.46, -0.37428 },
        { "10_Floor_USD", "Down:OptionletVolatility/USD/5/2Y/0.01", 3406.46, -3.14445 },
        { "10_Floor_USD", "Down:OptionletVolatility/USD/10/3Y/0.01", 3406.46, -16.3074 },
        { "10_Floor_USD", "Down:OptionletVolatility/USD/15/5Y/0.01", 3406.46, -94.5309 },
        { "10_Floor_USD", "Down:OptionletVolatility/USD/20/10Y/0.01", 3406.46, -90.9303 },
        // Excel calculation with z=5% flat rate, term structure day counter ActAct,
        // time to maturity T = YEARFRAC(14/4/16, 14/4/26, 1) = 9.99800896, yields
        // sensi to up shift d=1bp: exp(-(z+d)*T)-exp(z*T)
        // = -0.00060616719559925
        { "11_ZeroBond_EUR", "Up:YieldCurve/BondCurve1/6/10Y", 0.60659, -0.000606168 }, // OK, diff 1e-9
        // sensi to down shift d=-1bp: 0.00060677354516836
        { "11_ZeroBond_EUR", "Down:YieldCurve/BondCurve1/6/10Y", 0.60659, 0.000606774 }, // OK, diff < 1e-9
        // sensi to up shift d=+1bp: exp(-(z+d)*T)*USDEUR - exp(-z*T)*USDEUR
        // = -0.000505139329666004
        { "12_ZeroBond_USD", "Up:YieldCurve/BondCurve1/6/10Y", 0.505492, -0.00050514 }, // OK, diff < 1e-8
        // sensi to down shit d=-1bp: 0.000505644620973689
        { "12_ZeroBond_USD", "Down:YieldCurve/BondCurve1/6/10Y", 0.505492, 0.000505645 }, // OK, diff < 1e-9
        // sensi to EURUSD upshift d=+1%: exp(-z*T)*USDEUR/(1+d) - exp(-z*T)*USDEUR
        // = -0.00500487660122262
        { "12_ZeroBond_USD", "Up:FXSpot/EURUSD/0/spot", 0.505492, -0.00500487 }, // OK, diff < 1e-8
        // sensi to EURUSD down shift d=-1%: 0.00510598521942907
        { "12_ZeroBond_USD", "Down:FXSpot/EURUSD/0/spot", 0.505492, 0.00510598 } // OK, diff < 1e-8
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
    vector<ShiftScenarioGenerator::ScenarioDescription> desc = scenarioGenerator->scenarioDescriptions();
    for (Size i = 0; i < portfolio->size(); ++i) {
        Real npv0 = cube->getT0(i, 0);
        string id = portfolio->trades()[i]->id();
        for (Size j = 1; j < scenarioGenerator->samples(); ++j) { // skip j = 0, this is the base scenario
            Real npv = cube->get(i, 0, j, 0);
            Real sensi = npv - npv0;
            string label = desc[j].text();
            if (fabs(sensi) > tiny) {
                count++;
                // BOOST_TEST_MESSAGE("{ \"" << id << "\", \"" << label << "\", " << npv0 << ", " << sensi
                //                        << " },");
                pair<string, string> p(id, label);
                QL_REQUIRE(npvMap.find(p) != npvMap.end(), "pair (" << p.first << ", " << p.second
                                                                    << ") not found in npv map");
                QL_REQUIRE(sensiMap.find(p) != sensiMap.end(), "pair (" << p.first << ", " << p.second
                                                                        << ") not found in sensi map");
                BOOST_CHECK_MESSAGE(fabs(npv0 - npvMap[p]) < tolerance || fabs((npv0 - npvMap[p]) / npv0) < tolerance,
                                    "npv regression failed for pair (" << p.first << ", " << p.second << "): " << npv0
                                                                       << " vs " << npvMap[p]);
                BOOST_CHECK_MESSAGE(fabs(sensi - sensiMap[p]) < tolerance ||
                                        fabs((sensi - sensiMap[p]) / sensi) < tolerance,
                                    "sensitivity regression failed for pair ("
                                        << p.first << ", " << p.second << "): " << sensi << " vs " << sensiMap[p]);
            }
        }
    }
    BOOST_CHECK_MESSAGE(count == cachedResults.size(), "number of non-zero sensitivities ("
                                                           << count << ") do not match regression data ("
                                                           << cachedResults.size() << ")");

    // Repeat analysis using the SensitivityAnalysis class and spot check a few deltas and gammas
    boost::shared_ptr<SensitivityAnalysis> sa = boost::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, conventions);
    map<pair<string, string>, Real> deltaMap = sa->delta();
    map<pair<string, string>, Real> gammaMap = sa->gamma();

    std::vector<Results> cachedResults2 = {
        // trade, factor, delta, gamma
        { "11_ZeroBond_EUR", "YieldCurve/BondCurve1/6/10Y", -0.000606168, 6.06352e-07 }, // gamma OK see case 1 below
        { "12_ZeroBond_USD", "YieldCurve/BondCurve1/6/10Y", -0.00050514, 5.05294e-07 },  // gamma OK, see case 2 below
        { "12_ZeroBond_USD", "FXSpot/EURUSD/0/spot", -0.00500487, 0.000101108 }          // gamma OK, see case 3
    };

    // Validation of cached gammas:
    // gamma * (dx)^2 = \partial^2_x NPV(x) * (dx)^2
    //               \approx (NPV(x_up) - 2 NPV(x) + NPV(x_down)) = sensi(up) + sensi(down)
    //
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
        BOOST_CHECK_MESSAGE(fabs((delta - deltaMap[p]) / delta) < tolerance, "delta regression failed for trade "
                                                                                 << p.first << " factor " << p.second);
        BOOST_CHECK_MESSAGE(fabs((gamma - gammaMap[p]) / gamma) < tolerance, "gamma regression failed for trade "
                                                                                 << p.first << " factor " << p.second);
    }

    BOOST_TEST_MESSAGE("Cube generated in " << elapsed << " seconds");
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

void SensitivityAnalysisTest::testPortfolioSensitivityNoneObs() {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity (None observation mode)");
    testPortfolioSensitivity(ObservationMode::Mode::None);
}

void SensitivityAnalysisTest::testPortfolioSensitivityDisableObs() {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity (Disable observation mode)");
    testPortfolioSensitivity(ObservationMode::Mode::Disable);
}

void SensitivityAnalysisTest::testPortfolioSensitivityDeferObs() {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity (Defer observation mode)");
    testPortfolioSensitivity(ObservationMode::Mode::Defer);
}

void SensitivityAnalysisTest::testPortfolioSensitivityUnregisterObs() {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity (Unregister observation mode)");
    testPortfolioSensitivity(ObservationMode::Mode::Unregister);
}

void SensitivityAnalysisTest::test1dShifts() {
    BOOST_TEST_MESSAGE("Testing 1d shifts");

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
    boost::shared_ptr<Market> initMarket = boost::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData2();

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData2();

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
    vector<Period> shiftTenors = sensiData->discountCurveShiftData()["EUR"].shiftTenors;
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

void SensitivityAnalysisTest::test2dShifts() {
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
    boost::shared_ptr<Market> initMarket = boost::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData2();

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData2();

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
    vector<Period> expiryShiftTenors = sensiData->swaptionVolShiftData()["EUR"].shiftExpiries;
    vector<Period> termShiftTenors = sensiData->swaptionVolShiftData()["EUR"].shiftTerms;
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

void SensitivityAnalysisTest::testFxOptionDeltaGamma() {

    BOOST_TEST_MESSAGE("Testing FX option sensitivities against QL analytic greeks");

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    boost::shared_ptr<Market> initMarket = boost::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5();

    map<string, SensitivityScenarioData::FxVolShiftData>& fxvs = sensiData->fxVolShiftData();
    for (auto& it : fxvs) {
        it.second.shiftSize = 0.0001; // want a smaller shift size than 1.0 to test the analytic sensitivities
    }
    map<string, SensitivityScenarioData::FxShiftData>& fxs = sensiData->fxShiftData();
    for (auto& it : fxs) {
        it.second.shiftSize = 0.0001; // want a smaller shift size to test the analytic sensitivities
    }
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
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(data, simMarket);
    factory->registerBuilder(boost::make_shared<FxOptionEngineBuilder>());

    boost::shared_ptr<Portfolio> portfolio(new Portfolio());
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
    for (Size i = 0; i < portfolio->size(); ++i) {
        AnalyticInfo info;
        boost::shared_ptr<Trade> trn = portfolio->trades()[i];
        boost::shared_ptr<ore::data::FxOption> fxoTrn = boost::dynamic_pointer_cast<ore::data::FxOption>(trn);
        BOOST_CHECK(fxoTrn);
        info.id = trn->id();
        info.npvCcy = trn->npvCurrency();
        info.forCcy = fxoTrn->boughtCurrency();
        info.domCcy = fxoTrn->soldCurrency();
        BOOST_CHECK_EQUAL(info.npvCcy, info.domCcy);
        string pair = info.npvCcy + simMarketData->baseCcy();
        info.fx = initMarket->fxSpot(pair)->value();
        string trnPair = info.forCcy + info.domCcy;
        info.trnFx = initMarket->fxSpot(trnPair)->value();
        string forPair = info.forCcy + simMarketData->baseCcy();
        info.fxForBase = initMarket->fxSpot(forPair)->value();
        info.baseNpv = trn->instrument()->NPV() * info.fx;
        boost::shared_ptr<QuantLib::VanillaOption> qlOpt =
            boost::dynamic_pointer_cast<QuantLib::VanillaOption>(trn->instrument()->qlInstrument());
        BOOST_CHECK(qlOpt);
        info.qlNpv = qlOpt->NPV() * fxoTrn->boughtAmount();
        info.delta = qlOpt->delta() * fxoTrn->boughtAmount();
        info.gamma = qlOpt->gamma() * fxoTrn->boughtAmount();
        info.vega = qlOpt->vega() * fxoTrn->boughtAmount();
        info.rho = qlOpt->rho() * fxoTrn->boughtAmount();
        info.divRho = qlOpt->dividendRho() * fxoTrn->boughtAmount();
        BOOST_CHECK_CLOSE(info.fx, info.baseNpv / info.qlNpv, 0.01);
        qlInfoMap[info.id] = info;
    }

    bool useOriginalFxForBaseCcyConv = true; // convert sensi to EUR using original FX rate (not the shifted rate)
    boost::shared_ptr<SensitivityAnalysis> sa =
        boost::make_shared<SensitivityAnalysis>(portfolio, initMarket, Market::defaultConfiguration, data,
                                                simMarketData, sensiData, conventions, useOriginalFxForBaseCcyConv);
    map<pair<string, string>, Real> deltaMap = sa->delta();
    map<pair<string, string>, Real> gammaMap = sa->gamma();
    map<std::string, Real> baseNpvMap = sa->baseNPV();
    std::set<string> sensiTrades = sa->trades();

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
        SensiResults res = { string(""), 0.0, 0.0, 0.0, 0.0, 0.0,   0.0,   0.0,        0.0,       0.0,
                             0.0,        0.0, 0.0, 0.0, 0.0, false, false, string(""), string("") };
        for (auto it2 : deltaMap) {
            pair<string, string> sensiKey = it2.first;
            string sensiTrnId = it2.first.first;
            if (sensiTrnId != id)
                continue;
            res.id = sensiTrnId;
            res.baseNpv = baseNpvMap[sensiTrnId];
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
                Real fxSensi = initMarket->fxSpot(pair)->value();
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
        Real pc = 0.01;
        Real unit = 1.0;
        Real tol = 0.5; // % relative tolerance
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
                Real s = qlInfo.trnFx;           // TrnFor/TrnDom
                qlGamma = ((2.0 * y) / pow(z, 3)) * qlInfo.delta + (y / pow(z, 4)) * qlInfo.gamma;
            }
            BOOST_CHECK_CLOSE(res.fxSpotDeltaFor, qlInfo.delta * qlInfo.fx * (-bp * qlInfo.trnFx), tol);
            BOOST_CHECK_CLOSE(res.fxSpotGammaFor, qlGamma * qlInfo.fx * (pow(-bp * res.fxRateSensiFor, 2)), tol);
        }
    }
    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();
}

void SensitivityAnalysisTest::testCrossGamma() {

    BOOST_TEST_MESSAGE("Testing cross-gamma sensitivities against cached results");

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016);
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    boost::shared_ptr<Market> initMarket = boost::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5();
    map<string, SensitivityScenarioData::FxVolShiftData>& fxvs = sensiData->fxVolShiftData();
    vector<pair<string, string> >& cgFilter = sensiData->crossGammaFilter();
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
    portfolio->add(buildEuropeanSwaption("5_Swaption_EUR", "Long", "EUR", true, 1000000.0, 10, 10, 0.03, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
    trnCount++;
    portfolio->add(buildEuropeanSwaption("6_Swaption_EUR", "Long", "EUR", true, 1000000.0, 2, 5, 0.03, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
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
    boost::shared_ptr<SensitivityAnalysis> sa =
        boost::make_shared<SensitivityAnalysis>(portfolio, initMarket, Market::defaultConfiguration, data,
                                                simMarketData, sensiData, conventions, useOriginalFxForBaseCcyConv);
    map<pair<string, string>, Real> deltaMap = sa->delta();
    map<pair<string, string>, Real> gammaMap = sa->gamma();
    map<tuple<string, string, string>, Real> cgMap = sa->crossGamma();
    map<std::string, Real> baseNpvMap = sa->baseNPV();
    std::set<string> sensiTrades = sa->trades();

    struct GammaResult {
        string id;
        string factor1;
        string factor2;
        Real crossgamma;
    };

    vector<GammaResult> cachedResults = {
        { "10_Floor_USD", "DiscountCurve/USD/1/1Y", "OptionletVolatility/USD/0/1Y/0.01", -1.14292006e-05 },
        { "10_Floor_USD", "DiscountCurve/USD/1/1Y", "OptionletVolatility/USD/5/2Y/0.01", -4.75325714e-06 },
        { "10_Floor_USD", "DiscountCurve/USD/2/2Y", "OptionletVolatility/USD/0/1Y/0.01", -5.72627955e-05 },
        { "10_Floor_USD", "DiscountCurve/USD/2/2Y", "OptionletVolatility/USD/10/3Y/0.01", -6.0423848e-05 },
        { "10_Floor_USD", "DiscountCurve/USD/2/2Y", "OptionletVolatility/USD/5/2Y/0.01", -0.0003282313 },
        { "10_Floor_USD", "DiscountCurve/USD/3/3Y", "DiscountCurve/USD/4/5Y", 2.38844859e-06 },
        { "10_Floor_USD", "DiscountCurve/USD/3/3Y", "OptionletVolatility/USD/10/3Y/0.01", -0.0032767365 },
        { "10_Floor_USD", "DiscountCurve/USD/3/3Y", "OptionletVolatility/USD/15/5Y/0.01", -0.00124021334 },
        { "10_Floor_USD", "DiscountCurve/USD/3/3Y", "OptionletVolatility/USD/5/2Y/0.01", -0.000490482465 },
        { "10_Floor_USD", "DiscountCurve/USD/4/5Y", "DiscountCurve/USD/5/7Y", 4.56303869e-05 },
        { "10_Floor_USD", "DiscountCurve/USD/4/5Y", "OptionletVolatility/USD/10/3Y/0.01", -0.00309734116 },
        { "10_Floor_USD", "DiscountCurve/USD/4/5Y", "OptionletVolatility/USD/15/5Y/0.01", -0.0154732663 },
        { "10_Floor_USD", "DiscountCurve/USD/4/5Y", "OptionletVolatility/USD/20/10Y/0.01", -0.0011774169 },
        { "10_Floor_USD", "DiscountCurve/USD/4/5Y", "OptionletVolatility/USD/5/2Y/0.01", -1.15352532e-06 },
        { "10_Floor_USD", "DiscountCurve/USD/5/7Y", "DiscountCurve/USD/6/10Y", 0.00024726356 },
        { "10_Floor_USD", "DiscountCurve/USD/5/7Y", "OptionletVolatility/USD/10/3Y/0.01", -1.30253466e-06 },
        { "10_Floor_USD", "DiscountCurve/USD/5/7Y", "OptionletVolatility/USD/15/5Y/0.01", -0.0325565983 },
        { "10_Floor_USD", "DiscountCurve/USD/5/7Y", "OptionletVolatility/USD/20/10Y/0.01", -0.026175823 },
        { "10_Floor_USD", "DiscountCurve/USD/6/10Y", "OptionletVolatility/USD/15/5Y/0.01", -0.0151532607 },
        { "10_Floor_USD", "DiscountCurve/USD/6/10Y", "OptionletVolatility/USD/20/10Y/0.01", -0.0524224726 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/0/6M", "IndexCurve/USD-LIBOR-3M/1/1Y", -0.000206363102 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/0/6M", "IndexCurve/USD-LIBOR-3M/2/2Y", -9.83482187e-06 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", "IndexCurve/USD-LIBOR-3M/2/2Y", -0.0181056744 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.000292001105 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.197980608 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.000472459871 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/3/3Y", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.506924993 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/4/5Y", "IndexCurve/USD-LIBOR-3M/5/7Y", -1.31308851 },
        { "10_Floor_USD", "IndexCurve/USD-LIBOR-3M/5/7Y", "IndexCurve/USD-LIBOR-3M/6/10Y", -1.79643202 },
        { "10_Floor_USD", "OptionletVolatility/USD/0/1Y/0.01", "OptionletVolatility/USD/5/2Y/0.01", 0.0214845769 },
        { "10_Floor_USD", "OptionletVolatility/USD/10/3Y/0.01", "OptionletVolatility/USD/15/5Y/0.01", 0.224709734 },
        { "10_Floor_USD", "OptionletVolatility/USD/15/5Y/0.01", "OptionletVolatility/USD/20/10Y/0.01", 0.693920762 },
        { "10_Floor_USD", "OptionletVolatility/USD/5/2Y/0.01", "OptionletVolatility/USD/10/3Y/0.01", 0.0649121282 },
        { "1_Swap_EUR", "DiscountCurve/EUR/1/1Y", "DiscountCurve/EUR/2/2Y", 0.000439456664 },
        { "1_Swap_EUR", "DiscountCurve/EUR/1/1Y", "IndexCurve/EUR-EURIBOR-6M/0/6M", 0.0488603441 },
        { "1_Swap_EUR", "DiscountCurve/EUR/1/1Y", "IndexCurve/EUR-EURIBOR-6M/1/1Y", -0.0725961695 },
        { "1_Swap_EUR", "DiscountCurve/EUR/1/1Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.0499326873 },
        { "1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "DiscountCurve/EUR/3/3Y", 0.00136525929 },
        { "1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/0/6M", 0.00108389393 },
        { "1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/1/1Y", 0.141865394 },
        { "1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.191425738 },
        { "1_Swap_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.1454702 },
        { "1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "DiscountCurve/EUR/4/5Y", 0.00183080882 },
        { "1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/1/1Y", 0.000784549396 },
        { "1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 0.425320865 },
        { "1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.337527203 },
        { "1_Swap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.560276813 },
        { "1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "DiscountCurve/EUR/5/7Y", -0.00376823638 },
        { "1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 0.000516382745 },
        { "1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 0.91807051 },
        { "1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.606871969 },
        { "1_Swap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -1.1789221 },
        { "1_Swap_EUR", "DiscountCurve/EUR/5/7Y", "DiscountCurve/EUR/6/10Y", -0.0210602414 },
        { "1_Swap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 1.93838247 },
        { "1_Swap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.964284878 },
        { "1_Swap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -2.50079601 },
        { "1_Swap_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 3.43097423 },
        { "1_Swap_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -4.9024972 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/0/6M", "IndexCurve/EUR-EURIBOR-6M/1/1Y", -0.048865166 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/0/6M", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.00108389556 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/1/1Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.0924553103 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/1/1Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.000784546835 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.281394335 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.000516386237 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.359848329 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.779536431 },
        { "1_Swap_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.989040876 },
        { "2_Swap_USD", "DiscountCurve/USD/0/6M", "DiscountCurve/USD/1/1Y", 7.85577577e-05 },
        { "2_Swap_USD", "DiscountCurve/USD/1/1Y", "DiscountCurve/USD/2/2Y", 0.00034391915 },
        { "2_Swap_USD", "DiscountCurve/USD/2/2Y", "DiscountCurve/USD/3/3Y", 0.00101750751 },
        { "2_Swap_USD", "DiscountCurve/USD/3/3Y", "DiscountCurve/USD/4/5Y", 0.00129107304 },
        { "2_Swap_USD", "DiscountCurve/USD/4/5Y", "DiscountCurve/USD/5/7Y", 0.00885138742 },
        { "2_Swap_USD", "DiscountCurve/USD/5/7Y", "DiscountCurve/USD/6/10Y", 0.0236235501 },
        { "2_Swap_USD", "DiscountCurve/USD/6/10Y", "DiscountCurve/USD/7/15Y", 0.07325946 },
        { "2_Swap_USD", "DiscountCurve/USD/7/15Y", "DiscountCurve/USD/8/20Y", -2.22151866e-05 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/0/6M", "IndexCurve/USD-LIBOR-3M/1/1Y", -0.0202145245 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/0/6M", "IndexCurve/USD-LIBOR-3M/2/2Y", -0.000431735534 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", "IndexCurve/USD-LIBOR-3M/2/2Y", -0.0379707172 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/1/1Y", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.000316063757 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", "IndexCurve/USD-LIBOR-3M/3/3Y", -0.11422779 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/2/2Y", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.000207132776 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/3/3Y", "IndexCurve/USD-LIBOR-3M/4/5Y", -0.137591099 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/4/5Y", "IndexCurve/USD-LIBOR-3M/5/7Y", -0.305644142 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/5/7Y", "IndexCurve/USD-LIBOR-3M/6/10Y", -0.37816313 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/6/10Y", "IndexCurve/USD-LIBOR-3M/7/15Y", -0.431405343 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/6/10Y", "IndexCurve/USD-LIBOR-3M/8/20Y", -0.000289136427 },
        { "2_Swap_USD", "IndexCurve/USD-LIBOR-3M/7/15Y", "IndexCurve/USD-LIBOR-3M/8/20Y", 0.00042894634 },
        { "3_Swap_GBP", "DiscountCurve/GBP/0/6M", "FXSpot/EURGBP/0/spot", -0.0210289143 },
        { "3_Swap_GBP", "DiscountCurve/GBP/1/1Y", "FXSpot/EURGBP/0/spot", 0.00639700286 },
        { "3_Swap_GBP", "DiscountCurve/GBP/2/2Y", "FXSpot/EURGBP/0/spot", 0.0173332273 },
        { "3_Swap_GBP", "DiscountCurve/GBP/3/3Y", "FXSpot/EURGBP/0/spot", 0.0420620699 },
        { "3_Swap_GBP", "DiscountCurve/GBP/4/5Y", "FXSpot/EURGBP/0/spot", 0.0715365904 },
        { "3_Swap_GBP", "DiscountCurve/GBP/5/7Y", "FXSpot/EURGBP/0/spot", 0.124046364 },
        { "3_Swap_GBP", "DiscountCurve/GBP/6/10Y", "FXSpot/EURGBP/0/spot", 0.245374591 },
        { "3_Swap_GBP", "DiscountCurve/GBP/7/15Y", "FXSpot/EURGBP/0/spot", 0.388570486 },
        { "3_Swap_GBP", "DiscountCurve/GBP/8/20Y", "FXSpot/EURGBP/0/spot", -0.308991311 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "DiscountCurve/EUR/7/15Y", -0.000812667564 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "DiscountCurve/EUR/8/20Y", -0.000307592545 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.0728828434 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/7/15Y", -0.177001389 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", 0.0905885823 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", -0.0110764697 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "DiscountCurve/EUR/8/20Y", 0.00224833752 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.0136493384 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "IndexCurve/EUR-EURIBOR-6M/7/15Y", -0.0474343306 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", 0.0360955216 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/7/15Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", 0.0016329258 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/8/20Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.164000375 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/8/20Y", "IndexCurve/EUR-EURIBOR-6M/7/15Y", 0.417352238 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/8/20Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", -0.229366568 },
        { "5_Swaption_EUR", "DiscountCurve/EUR/8/20Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", 0.0199757638 },
        { "5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "IndexCurve/EUR-EURIBOR-6M/7/15Y", -0.26738619 },
        { "5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", -2.85552727 },
        { "5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", -2.22802017 },
        { "5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/7/15Y", "IndexCurve/EUR-EURIBOR-6M/8/20Y", 0.332563403 },
        { "5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/7/15Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", 0.316522049 },
        { "5_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/8/20Y", "SwaptionVolatility/EUR/5/10Y/10Y/ATM", 3.96890127 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "DiscountCurve/EUR/3/3Y", 4.65581502e-06 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "DiscountCurve/EUR/4/5Y", -1.91397435e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "DiscountCurve/EUR/5/7Y", -3.97715846e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 0.0021812065 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.00152258186 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.000110931094 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.00402972821 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -1.54062514e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.00258447059 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/2/2Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", -4.72749957e-06 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "DiscountCurve/EUR/4/5Y", 2.01546663e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "DiscountCurve/EUR/5/7Y", 3.39962985e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 0.00233445946 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.00339824401 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.00562310815 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.00698599517 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 2.59470628e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/3/3Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 6.65486464e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "DiscountCurve/EUR/5/7Y", 0.000102988368 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.00379254633 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 0.00963019417 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.00588629799 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.000396238503 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 4.87477412e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/4/5Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 0.000261444328 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -0.00793621904 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 0.000347485427 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 0.0207397881 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.00216833219 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -7.23455626e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 0.00849096973 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/5/7Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", 1.55316409e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -4.09244039e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 1.69568852e-06 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 3.42704752e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 9.33648726e-05 },
        { "6_Swaption_EUR", "DiscountCurve/EUR/6/10Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 6.55931062e-05 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.0129378053 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.0222466371 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.807803513 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.00308817849 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", -0.518759384 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", -0.000948864704 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.00277042464 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.0333716971 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.000127656373 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 0.0214136069 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", 3.91697083e-05 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.0655785376 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.000251968412 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 0.0473829647 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", 8.66729658e-05 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", 0.0103058335 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 1.71370377 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", 0.00313519435 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", 0.00658131276 },
        { "6_Swaption_EUR", "IndexCurve/EUR-EURIBOR-6M/6/10Y", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", 1.20384966e-05 },
        { "6_Swaption_EUR", "SwaptionVolatility/EUR/0/2Y/5Y/ATM", "SwaptionVolatility/EUR/2/5Y/5Y/ATM", 0.00164491931 },
        { "7_FxOption_EUR_USD", "DiscountCurve/EUR/3/3Y", "DiscountCurve/EUR/4/5Y", 0.0027612336 },
        { "7_FxOption_EUR_USD", "DiscountCurve/EUR/3/3Y", "FXSpot/EURUSD/0/spot", -42.4452352 },
        { "7_FxOption_EUR_USD", "DiscountCurve/EUR/3/3Y", "FXVolatility/EURUSD/0/5Y/ATM", 168.577072 },
        { "7_FxOption_EUR_USD", "DiscountCurve/EUR/4/5Y", "FXSpot/EURUSD/0/spot", -0.0776202832 },
        { "7_FxOption_EUR_USD", "DiscountCurve/EUR/4/5Y", "FXVolatility/EURUSD/0/5Y/ATM", 0.308961544 },
        { "7_FxOption_EUR_USD", "DiscountCurve/USD/3/3Y", "DiscountCurve/USD/4/5Y", 0.00206353236 },
        { "8_FxOption_EUR_GBP", "DiscountCurve/GBP/5/7Y", "FXSpot/EURGBP/0/spot", 40.247185 },
        { "9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", 1.89362237e-06 },
        { "9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 1.60204674e-05 },
        { "9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -3.54807444e-05 },
        { "9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "OptionletVolatility/EUR/14/3Y/0.05", -7.41440071e-05 },
        { "9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "OptionletVolatility/EUR/19/5Y/0.05", -2.8717396e-05 },
        { "9_Cap_EUR", "DiscountCurve/EUR/3/3Y", "OptionletVolatility/EUR/9/2Y/0.05", -3.95373826e-06 },
        { "9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "DiscountCurve/EUR/5/7Y", 1.87918619e-06 },
        { "9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", 0.000141954676 },
        { "9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 0.000136532169 },
        { "9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.000558091084 },
        { "9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "OptionletVolatility/EUR/14/3Y/0.05", -0.000195855626 },
        { "9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "OptionletVolatility/EUR/19/5Y/0.05", -0.0013501175 },
        { "9_Cap_EUR", "DiscountCurve/EUR/4/5Y", "OptionletVolatility/EUR/24/10Y/0.05", -9.03819837e-05 },
        { "9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "DiscountCurve/EUR/6/10Y", 2.44087892e-05 },
        { "9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", 0.00131097735 },
        { "9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.000537751659 },
        { "9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.00376190752 },
        { "9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "OptionletVolatility/EUR/19/5Y/0.05", -0.00650057233 },
        { "9_Cap_EUR", "DiscountCurve/EUR/5/7Y", "OptionletVolatility/EUR/24/10Y/0.05", -0.00529335126 },
        { "9_Cap_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", 0.00677440175 },
        { "9_Cap_EUR", "DiscountCurve/EUR/6/10Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.0101355366 },
        { "9_Cap_EUR", "DiscountCurve/EUR/6/10Y", "OptionletVolatility/EUR/19/5Y/0.05", -0.00512368197 },
        { "9_Cap_EUR", "DiscountCurve/EUR/6/10Y", "OptionletVolatility/EUR/24/10Y/0.05", -0.0166702108 },
        { "9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/1/1Y", "IndexCurve/EUR-EURIBOR-6M/2/2Y", -3.22099407e-06 },
        { "9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/3/3Y", -0.00114858136 },
        { "9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/2/2Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -3.32910605e-06 },
        { "9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/3/3Y", "IndexCurve/EUR-EURIBOR-6M/4/5Y", -0.0325351415 },
        { "9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/4/5Y", "IndexCurve/EUR-EURIBOR-6M/5/7Y", -0.22049032 },
        { "9_Cap_EUR", "IndexCurve/EUR-EURIBOR-6M/5/7Y", "IndexCurve/EUR-EURIBOR-6M/6/10Y", -0.599739496 },
        { "9_Cap_EUR", "OptionletVolatility/EUR/14/3Y/0.05", "OptionletVolatility/EUR/19/5Y/0.05", 0.0480747768 },
        { "9_Cap_EUR", "OptionletVolatility/EUR/19/5Y/0.05", "OptionletVolatility/EUR/24/10Y/0.05", 0.670249341 },
        { "9_Cap_EUR", "OptionletVolatility/EUR/4/1Y/0.05", "OptionletVolatility/EUR/9/2Y/0.05", 2.49049523e-05 },
        { "9_Cap_EUR", "OptionletVolatility/EUR/9/2Y/0.05", "OptionletVolatility/EUR/14/3Y/0.05", 0.00180372518 }
    };

    map<tuple<string, string, string>, Real> cachedMap;
    for (Size i = 0; i < cachedResults.size(); ++i) {
        tuple<string, string, string> p(cachedResults[i].id, cachedResults[i].factor1, cachedResults[i].factor2);
        cachedMap[p] = cachedResults[i].crossgamma;
    }

    Real rel_tol = 0.005;
    Real threshold = 1.e-6;
    Size count = 0;
    for (auto it : cgMap) {
        tuple<string, string, string> key = it.first;
        string id = std::get<0>(it.first);
        string factor1 = std::get<1>(it.first);
        string factor2 = std::get<2>(it.first);
        ostringstream os;
        os << id << "_" << factor1 << "_" << factor2;
        string keyStr = os.str();
        Real crossgamma = it.second;
        if (fabs(crossgamma) >= threshold) {
            // BOOST_TEST_MESSAGE("{ \"" << id << std::setprecision(9) << "\", \"" << factor1 << "\", \"" << factor2 <<
            // "\", " << crossgamma << " },");
            auto cached_it = cachedMap.find(key);
            BOOST_CHECK_MESSAGE(cached_it != cachedMap.end(), keyStr << " not found in cached results");
            if (cached_it != cachedMap.end()) {
                tuple<string, string, string> cached_key = cached_it->first;
                Real cached_cg = cached_it->second;
                BOOST_CHECK_CLOSE(crossgamma, cached_cg, rel_tol);
                count++;
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

test_suite* SensitivityAnalysisTest::suite() {
    // Uncomment the below to get detailed output TODO: custom logger that uses BOOST_MESSAGE
    /*
      boost::shared_ptr<ore::data::FileLogger> logger = boost::make_shared<ore::data::FileLogger>("sensitivity.log");
      ore::data::Log::instance().removeAllLoggers();
      ore::data::Log::instance().registerLogger(logger);
      ore::data::Log::instance().switchOn();
      ore::data::Log::instance().setMask(255);
    */
    test_suite* suite = BOOST_TEST_SUITE("SensitivityAnalysisTest");
    // Set the Observation mode here
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::test1dShifts));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::test2dShifts));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::testPortfolioSensitivityNoneObs));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::testPortfolioSensitivityDisableObs));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::testPortfolioSensitivityDeferObs));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::testPortfolioSensitivityUnregisterObs));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::testFxOptionDeltaGamma));
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::testCrossGamma));
    return suite;
}
}
