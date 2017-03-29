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

    return simMarketData;
}

boost::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData2() {
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();

    sensiData->parConversion() = false;

    SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = {
        1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years
    }; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;
    cvsData.parInstruments = { "DEP", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS" };

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
    sensiData->discountCurveShiftData()["EUR"].parInstrumentSingleCurve = true;
    sensiData->discountCurveShiftData()["EUR"].parInstrumentConventions = { { "DEP", "EUR-DEP-CONVENTIONS" },
                                                                            { "IRS", "EUR-6M-SWAP-CONVENTIONS" } };
    sensiData->discountCurveShiftData()["GBP"] = cvsData;
    sensiData->discountCurveShiftData()["GBP"].parInstrumentSingleCurve = true;
    sensiData->discountCurveShiftData()["GBP"].parInstrumentConventions = { { "DEP", "GBP-DEP-CONVENTIONS" },
                                                                            { "IRS", "GBP-6M-SWAP-CONVENTIONS" } };

    sensiData->indexNames() = { "EUR-EURIBOR-6M", "GBP-LIBOR-6M" };
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"].parInstrumentSingleCurve = false;
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"].parInstrumentConventions = {
        { "DEP", "EUR-DEP-CONVENTIONS" }, { "IRS", "EUR-6M-SWAP-CONVENTIONS" }
    };
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"].parInstrumentSingleCurve = false;
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"].parInstrumentConventions = {
        { "DEP", "GBP-DEP-CONVENTIONS" }, { "IRS", "GBP-6M-SWAP-CONVENTIONS" }
    };

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

    sensiData->parConversion() = false;

    SensitivityScenarioData::CurveShiftData cvsData;
    cvsData.shiftTenors = { 6 * Months, 1 * Years,  2 * Years,  3 * Years, 5 * Years,
                            7 * Years,  10 * Years, 15 * Years, 20 * Years }; // multiple tenors: triangular shifts
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 0.0001;
    cvsData.parInstruments = { "DEP", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS" };

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
    sensiData->discountCurveShiftData()["EUR"].parInstrumentSingleCurve = true;
    sensiData->discountCurveShiftData()["EUR"].parInstrumentConventions = { { "DEP", "EUR-DEP-CONVENTIONS" },
                                                                            { "IRS", "EUR-6M-SWAP-CONVENTIONS" } };
    sensiData->discountCurveShiftData()["USD"] = cvsData;
    sensiData->discountCurveShiftData()["USD"].parInstrumentSingleCurve = true;
    sensiData->discountCurveShiftData()["USD"].parInstrumentConventions = { { "DEP", "USD-DEP-CONVENTIONS" },
                                                                            { "IRS", "USD-3M-SWAP-CONVENTIONS" } };
    sensiData->discountCurveShiftData()["GBP"] = cvsData;
    sensiData->discountCurveShiftData()["GBP"].parInstrumentSingleCurve = true;
    sensiData->discountCurveShiftData()["GBP"].parInstrumentConventions = { { "DEP", "GBP-DEP-CONVENTIONS" },
                                                                            { "IRS", "GBP-6M-SWAP-CONVENTIONS" } };
    sensiData->discountCurveShiftData()["JPY"] = cvsData;
    sensiData->discountCurveShiftData()["JPY"].parInstrumentSingleCurve = true;
    sensiData->discountCurveShiftData()["JPY"].parInstrumentConventions = { { "DEP", "JPY-DEP-CONVENTIONS" },
                                                                            { "IRS", "JPY-6M-SWAP-CONVENTIONS" } };
    sensiData->discountCurveShiftData()["CHF"] = cvsData;
    sensiData->discountCurveShiftData()["CHF"].parInstrumentSingleCurve = true;
    sensiData->discountCurveShiftData()["CHF"].parInstrumentConventions = { { "DEP", "CHF-DEP-CONVENTIONS" },
                                                                            { "IRS", "CHF-6M-SWAP-CONVENTIONS" } };

    sensiData->indexNames() = { "EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M" };
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"].parInstrumentSingleCurve = false;
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"].parInstrumentConventions = {
        { "DEP", "EUR-DEP-CONVENTIONS" }, { "IRS", "EUR-6M-SWAP-CONVENTIONS" }
    };
    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] = cvsData;
    sensiData->indexCurveShiftData()["USD-LIBOR-3M"].parInstrumentSingleCurve = false;
    sensiData->indexCurveShiftData()["USD-LIBOR-3M"].parInstrumentConventions = {
        { "DEP", "USD-DEP-CONVENTIONS" }, { "IRS", "USD-3M-SWAP-CONVENTIONS" }
    };
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"].parInstrumentSingleCurve = false;
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"].parInstrumentConventions = {
        { "DEP", "GBP-DEP-CONVENTIONS" }, { "IRS", "GBP-6M-SWAP-CONVENTIONS" }
    };
    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] = cvsData;
    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"].parInstrumentSingleCurve = false;
    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"].parInstrumentConventions = {
        { "DEP", "JPY-DEP-CONVENTIONS" }, { "IRS", "JPY-6M-SWAP-CONVENTIONS" }
    };
    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] = cvsData;
    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"].parInstrumentSingleCurve = true;
    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"].parInstrumentConventions = {
        { "DEP", "CHF-DEP-CONVENTIONS" }, { "IRS", "CHF-6M-SWAP-CONVENTIONS" }
    };

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
    sensiData->parConversion() = false;

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
    portfolio->add(buildCap("9_Cap_EUR", "EUR", "Long", 0.05, 1000000.0, 0, 10, "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildFloor("10_Floor_USD", "USD", "Long", 0.01, 1000000.0, 0, 10, "3M", "A360", "USD-LIBOR-3M"));
    portfolio->build(factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // build the scenario valuation engine
    boost::shared_ptr<DateGrid> dg = boost::make_shared<DateGrid>("1,0W"); //TODO - extend the DateGrid interface so that it can actually take a vector of dates as input
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
        { "10_Floor_USD", "Down:OptionletVolatility/USD/20/10Y/0.01", 3406.46, -90.9303 }
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
                //BOOST_TEST_MESSAGE("{ \"" << id << "\", \"" << label << "\", " << npv0 << ", " << sensi
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
    BOOST_CHECK_MESSAGE(count == cachedResults.size(), 
        "number of non-zero sensitivities (" << count << 
        ") do not match regression data (" << cachedResults.size() << 
        ")");

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
    sensiData->parConversion() = false;
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
    };
    map<string,AnalyticInfo> qlInfoMap;
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

    boost::shared_ptr<SensitivityAnalysis> sa = 
        boost::make_shared<SensitivityAnalysis>(
            portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, conventions);
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
        SensiResults res = { string(""), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false, false };
        for (auto it2 : deltaMap) {
            pair<string,string> sensiKey = it2.first;
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
                }
                else if (isIndexCurve) {
                    if (isFgnCcySensi)
                        res.forIndexDelta += sensiVal;
                    else if (isDomCcySensi)
                        res.domIndexDelta += sensiVal;
                }
                else if (isYieldCurve) {
                    if (isFgnCcySensi)
                        res.forYcDelta += sensiVal;
                    if (isDomCcySensi)
                        res.domYcDelta += sensiVal;
                }
                continue;
            }
            else if (isFxSpot) {
                BOOST_CHECK(tokens.size() > 2);
                string pair = tokens[1];
                BOOST_CHECK_EQUAL(pair.length(), 6);
                string sensiForCcy = pair.substr(0, 3);
                string sensiDomCcy = pair.substr(3, 3);
                Real fxSensi = initMarket->fxSpot(pair)->value();
                bool isSensiForBase = (sensiForCcy == simMarketData->baseCcy());
                bool isSensiDomBase = (sensiDomCcy == simMarketData->baseCcy());
                // TO-DO this could be relaxed to handle case where market stores the currency pairs the other way around
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
                    }
                    else if (sensiDomCcy == qlInfo.domCcy) {
                        res.fxSpotDeltaDom += sensiVal;
                        res.fxRateSensiDom = fxSensi;
                        res.hasFxSpotDomSensi = true;
                        res.fxSpotGammaDom += gammaVal;
                    }
                }
                else {
                    BOOST_ERROR("This ccy pair configuration not supported yet by this test");
                }
                continue;
            }
            else if (isFxVol) {
                BOOST_CHECK(tokens.size() > 2);
                string pair = tokens[1];
                BOOST_CHECK_EQUAL(pair.length(), 6);
                string sensiForCcy = pair.substr(0, 3);
                string sensiDomCcy = pair.substr(3, 3);
                BOOST_CHECK((sensiForCcy == qlInfo.forCcy) || (sensiForCcy == qlInfo.domCcy));
                BOOST_CHECK((sensiDomCcy == qlInfo.forCcy) || (sensiDomCcy == qlInfo.domCcy));
                res.fxVolDelta += sensiVal;
                continue;
            }
            else {
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
            "SA: id=" << res.id << ", npv=" << res.baseNpv << ", forDiscountDelta=" << res.forDiscountDelta << 
            ", domDiscountDelta=" << res.domDiscountDelta << ", fxSpotDeltaFor=" << res.fxSpotDeltaFor << 
            ", fxSpotDeltaDom=" << res.fxSpotDeltaDom << ", fxVolDelta=" << res.fxVolDelta << 
            ", fxSpotGammaFor=" << res.fxSpotGammaFor << ", fxSpotGammaDom=" << res.fxSpotGammaDom);
        BOOST_TEST_MESSAGE(
            "QL: id=" << qlInfo.id << ", forCcy=" << qlInfo.forCcy << ", domCcy=" << qlInfo.domCcy << 
            ", fx=" << qlInfo.fx << ", npv=" << qlInfo.baseNpv << ", ccyNpv=" << qlInfo.qlNpv << 
            ", delta=" << qlInfo.delta << ", gamma=" << qlInfo.gamma << ", vega=" << qlInfo.vega << 
            ", rho=" << qlInfo.rho << ", divRho=" << qlInfo.divRho);
        Real bp = 1.e-4;
        Real pc = 0.01;
        Real unit = 1.0;
        Real tol = 0.5; // % relative tolerance
        // rate sensis are 1bp absolute shifts
        // fx vol sensis are 1bp relative shifts
        // fx spot sensis are 1pb relative shifts
        BOOST_CHECK_CLOSE(res.domDiscountDelta, qlInfo.rho*qlInfo.fx*bp, tol);
        BOOST_CHECK_CLOSE(res.forDiscountDelta, qlInfo.divRho*qlInfo.fx*bp, tol);
        Real fxVol = initMarket->fxVol(qlInfo.forCcy + qlInfo.domCcy)->blackVol(1.0,1.0,true); // TO-DO more appropriate vol extraction
        BOOST_CHECK_CLOSE(res.fxVolDelta, qlInfo.vega*qlInfo.fx*(bp*fxVol), tol);
        if (res.hasFxSpotDomSensi) {
            BOOST_CHECK_CLOSE(res.fxSpotDeltaDom, qlInfo.delta*qlInfo.fx*(bp*qlInfo.trnFx), tol);
            BOOST_CHECK_CLOSE(res.fxSpotGammaDom, qlInfo.gamma*qlInfo.fx*(pow(bp*qlInfo.trnFx,2)), tol);
        }
        if (res.hasFxSpotForSensi) {
            BOOST_CHECK_CLOSE(res.fxSpotDeltaFor, qlInfo.delta*qlInfo.fx*(-bp*qlInfo.trnFx), tol);
            BOOST_CHECK_CLOSE(res.fxSpotGammaFor, qlInfo.gamma*qlInfo.fx*(pow(-bp*qlInfo.trnFx,2)), tol);
        }
    }
    //TODO - turn off the report writing
    sa->writeSensitivityReport("./unitTest_fxOption_sensi.csv");
    sa->writeScenarioReport("./unitTest_fxOption_scenario.csv");
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
    return suite;
}
}
