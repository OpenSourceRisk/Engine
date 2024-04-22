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

#include "testmarket.hpp"
#include "testportfolio.hpp"

#include <boost/test/unit_test.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/cube/npvcube.hpp>
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
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>
#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>

#include <ored/model/lgmdata.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>

#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounters/actualactual.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

using testsuite::buildEuropeanSwaption;
using testsuite::buildFxOption;
using testsuite::buildSwap;
using testsuite::TestMarket;

namespace {
QuantLib::ext::shared_ptr<data::Conventions> conv() {
    QuantLib::ext::shared_ptr<data::Conventions> conventions(new data::Conventions());

    QuantLib::ext::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    // QuantLib::ext::shared_ptr<data::Convention> swapConv(
    //     new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("EUR-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "EUR-EURIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("USD-3M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-3M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("USD-6M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("GBP-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "GBP-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("JPY-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "JPY-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<data::IRSwapConvention>("CHF-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "CHF-LIBOR-6M"));

    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("EUR-DEP-CONVENTIONS", "EUR-EURIBOR"));
    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("USD-DEP-CONVENTIONS", "USD-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("GBP-DEP-CONVENTIONS", "GBP-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("JPY-DEP-CONVENTIONS", "JPY-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<data::DepositConvention>("CHF-DEP-CONVENTIONS", "CHF-LIBOR"));

    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-USD-FX", "0", "EUR", "USD", "10000", "EUR,USD"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-GBP-FX", "0", "EUR", "GBP", "10000", "EUR,GBP"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-CHF-FX", "0", "EUR", "CHF", "10000", "EUR,CHF"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-JPY-FX", "0", "EUR", "JPY", "10000", "EUR,JPY"));

    InstrumentConventions::instance().setConventions(conventions);

    return conventions;
}

QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData5() {
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setIndices(
        {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "USD-LIBOR-6M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});
    simMarketData->interpolation() = "LogLinear";

    simMarketData->setSwapVolTerms("", {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setSwapVolKeys({"EUR", "GBP", "USD", "CHF", "JPY"});
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->setSimulateSwapVols(true); // false;
    simMarketData->setFxVolExpiries("",
        vector<Period>{6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years});
    simMarketData->setFxVolDecayMode(string("ConstantVariance"));
    simMarketData->setSimulateFXVols(true); // false;
    simMarketData->setFxVolCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"});
    simMarketData->setFxVolIsSurface(false);
    simMarketData->setFxVolMoneyness(vector<Real>{0});

    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});

    simMarketData->setSimulateCapFloorVols(true);
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->setCapFloorVolKeys({"EUR", "USD"});
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setCapFloorVolStrikes("", {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06});

    return simMarketData;
}

QuantLib::ext::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5() {
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = QuantLib::ext::make_shared<SensitivityScenarioData>();

    SensitivityScenarioData::CurveShiftData cvsData;

    // identical to sim market tenor structure, we can only check this case, because the analytic engine
    // assumes either linear in zero or linear in log discount interpolation, while the sensitivity analysis
    // assumes a linear in zero interpolation for rebucketing, but uses the linear in log discount interpolation
    // of the sim market yield curves for the scenario calculation
    cvsData.shiftTenors = {1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,  4 * Years,
                           5 * Years,  7 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 1E-5;

    SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = ShiftType::Absolute;
    fxsData.shiftSize = 1E-5;

    SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = ShiftType::Absolute;
    fxvsData.shiftSize = 1E-5;
    fxvsData.shiftExpiries = {5 * Years};

    SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = ShiftType::Absolute;
    cfvsData.shiftSize = 1E-5;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05};

    SensitivityScenarioData::GenericYieldVolShiftData swvsData;
    swvsData.shiftType = ShiftType::Absolute;
    swvsData.shiftSize = 1E-5;
    swvsData.shiftExpiries = {6 * Months, 1 * Years, 2 * Years,  3 * Years,
                              5 * Years,  7 * Years, 10 * Years, 20 * Years};
    swvsData.shiftTerms = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years};

    sensiData->discountCurveShiftData()["EUR"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->discountCurveShiftData()["USD"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->discountCurveShiftData()["GBP"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->discountCurveShiftData()["JPY"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->discountCurveShiftData()["CHF"] = QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;
    sensiData->fxVolShiftData()["GBPCHF"] = fxvsData;

    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;
    sensiData->swaptionVolShiftData()["USD"] = swvsData;
    sensiData->swaptionVolShiftData()["JPY"] = swvsData;
    sensiData->swaptionVolShiftData()["CHF"] = swvsData;

    sensiData->capFloorVolShiftData()["EUR"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["EUR"]->indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["USD"]->indexName = "USD-LIBOR-3M";

    sensiData->crossGammaFilter() = {{"DiscountCurve/EUR", "DiscountCurve/EUR"},
                                     {"DiscountCurve/USD", "DiscountCurve/USD"},
                                     {"DiscountCurve/EUR", "IndexCurve/EUR"},
                                     {"IndexCurve/EUR", "IndexCurve/EUR"},
                                     {"DiscountCurve/EUR", "DiscountCurve/USD"}};

    return sensiData;
}

bool check(const Real reference, const Real value) {
    if (std::fabs(reference) >= 1E-2) {
        return std::fabs((reference - value) / reference) < 5E-3;
    } else {
        return std::fabs(reference - value) < 1E-3;
    }
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SensitivityAnalysisAnalyticTest)

BOOST_AUTO_TEST_CASE(testSensitivities) {

    BOOST_TEST_MESSAGE("Checking sensitivity analysis results vs analytic sensi engine results...");

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5();

    // build scenario sim market
    conv();

    // build portfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("CrossCurrencySwap") = "DiscountedCashflows";
    data->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";

    // QuantLib::ext::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    portfolio->add(
        buildSwap("1_Swap_EUR", "EUR", true, 10.0, 0, 10, 0.03, 0.00, "1Y", "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildFxOption("7_FxOption_EUR_USD", "Long", "Call", 3, "EUR", 10.0, "USD", 11.0));

    // analytic results
    map<string, Real> analyticalResultsDelta = {{"1_Swap_EUR DiscountCurve/EUR/0/1M", 0},
                                                {"1_Swap_EUR DiscountCurve/EUR/1/6M", -0.0251638},
                                                {"1_Swap_EUR DiscountCurve/EUR/10/20Y", 0},
                                                {"1_Swap_EUR DiscountCurve/EUR/11/30Y", 0},
                                                {"1_Swap_EUR DiscountCurve/EUR/2/1Y", 0.146855},
                                                {"1_Swap_EUR DiscountCurve/EUR/3/2Y", 0.190109},
                                                {"1_Swap_EUR DiscountCurve/EUR/4/3Y", 0.279228},
                                                {"1_Swap_EUR DiscountCurve/EUR/5/4Y", 0.364784},
                                                {"1_Swap_EUR DiscountCurve/EUR/6/5Y", 0.66847},
                                                {"1_Swap_EUR DiscountCurve/EUR/7/7Y", 1.49473},
                                                {"1_Swap_EUR DiscountCurve/EUR/8/10Y", 2.05151},
                                                {"1_Swap_EUR DiscountCurve/EUR/9/15Y", 0},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M", -4.95025},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y", 0.146584},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y", 0.385931},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y", 0.567839},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y", 0.74296},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y", 1.35326},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y", 3.03756},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y", 84.7885},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y", -21.0493},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y", -0.0770026},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/10/20Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/11/30Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y", 16.9542},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/5/4Y", 0.0620218},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/6/5Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/7/7Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/8/10Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/9/15Y", 0},
                                                {"7_FxOption_EUR_USD FXSpot/EURUSD/0/spot", 4.72549},
                                                {"7_FxOption_EUR_USD FXVolatility/EURUSD/0/5Y/ATM", 5.21067}};

    map<string, Real> analyticalResultsGamma = {{"1_Swap_EUR DiscountCurve/EUR/0/1M", 0},
                                                {"1_Swap_EUR DiscountCurve/EUR/1/6M", 0.0125819},
                                                {"1_Swap_EUR DiscountCurve/EUR/10/20Y", 0},
                                                {"1_Swap_EUR DiscountCurve/EUR/11/30Y", 0},
                                                {"1_Swap_EUR DiscountCurve/EUR/2/1Y", -0.16852},
                                                {"1_Swap_EUR DiscountCurve/EUR/3/2Y", -0.558829},
                                                {"1_Swap_EUR DiscountCurve/EUR/4/3Y", -1.24741},
                                                {"1_Swap_EUR DiscountCurve/EUR/5/4Y", -2.19217},
                                                {"1_Swap_EUR DiscountCurve/EUR/6/5Y", -3.64545},
                                                {"1_Swap_EUR DiscountCurve/EUR/7/7Y", -8.45766},
                                                {"1_Swap_EUR DiscountCurve/EUR/8/10Y", -17.5009},
                                                {"1_Swap_EUR DiscountCurve/EUR/9/15Y", 0},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M", 2.47512},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y", 14.3979},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y", 37.7122},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y", 84.1478},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y", 148.04},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y", 170.402},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y", 178.37},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y", 141.3},
                                                {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y", 192.286},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y", 0.00257327},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/10/20Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/11/30Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y", 78.6621},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/5/4Y", 0.00105269},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/6/5Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/7/7Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/8/10Y", 0},
                                                {"7_FxOption_EUR_USD DiscountCurve/USD/9/15Y", 0},
                                                {"7_FxOption_EUR_USD FXSpot/EURUSD/0/spot", 2.17301}};

    map<string, Real> analyticalResultsCrossGamma = {
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/3/2Y", 0.0439491},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/1/6M", 4.8864},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/2/1Y", -7.2595},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/3/2Y", -4.99316},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/4/3Y", 0.136543},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0.108392},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 14.1881},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/3/2Y", -19.1426},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/4/3Y", -14.5467},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/5/4Y", 0.274041},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0.0784567},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 42.4881},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/4/3Y", -42.7095},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/5/4Y", -28.3908},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/6/5Y", 0.459076},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0.10308},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 83.8339},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/5/4Y", -75.1334},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/6/5Y", -46.1375},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/7/7Y", -0.376937},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 137.497},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/6/5Y", -87.5996},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/7/7Y", -117.899},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y DiscountCurve/EUR/8/10Y", -2.10692},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 193.901},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/7/7Y", -96.4279},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/8/10Y", -250.112},
        {"1_Swap_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y DiscountCurve/EUR/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 343.241},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/8/10Y", -490.385},
        {"1_Swap_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y DiscountCurve/EUR/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y DiscountCurve/EUR/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/2/1Y", -4.8864},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/3/2Y", -0.108392},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/10/20Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/3/2Y", -9.24531},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/4/3Y", -0.0784567},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/4/3Y", -28.0873},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/5/4Y", -0.10308},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/5/4Y", -55.7263},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/6/5Y", -91.8185},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/7/7Y", -77.9517},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y IndexCurve/EUR-EURIBOR-6M/8/10Y", -98.9016},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/9/15Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"1_Swap_EUR IndexCurve/EUR-EURIBOR-6M/9/15Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/0/1M DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/1/6M DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/10/20Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/11/30Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/2/1Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/EUR/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/EUR/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/EUR/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/EUR/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/EUR/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/3/2Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/EUR/5/4Y", 0.703423},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/EUR/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/EUR/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/EUR/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/4/3Y", -129.352},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/5/4Y", -0.473197},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/4/3Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/EUR/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/EUR/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/EUR/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/4/3Y", -0.473197},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/5/4Y", -0.00173105},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/5/4Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/EUR/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/EUR/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/6/5Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/EUR/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/7/7Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/EUR/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/8/10Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/EUR/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/EUR/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/0/1M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/EUR/9/15Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/1/6M", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/0/1M DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/2/1Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/1/6M DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/10/20Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/3/2Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/2/1Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y DiscountCurve/USD/4/3Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y DiscountCurve/USD/5/4Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/3/2Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y DiscountCurve/USD/5/4Y", 0.287762},
        {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/4/3Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/5/4Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/5/4Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/5/4Y DiscountCurve/USD/6/5Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/5/4Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/5/4Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/5/4Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/6/5Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/6/5Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/6/5Y DiscountCurve/USD/7/7Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/6/5Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/6/5Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/7/7Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/7/7Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/7/7Y DiscountCurve/USD/8/10Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/7/7Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/8/10Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/8/10Y DiscountCurve/USD/11/30Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/8/10Y DiscountCurve/USD/9/15Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/9/15Y DiscountCurve/USD/10/20Y", 0},
        {"7_FxOption_EUR_USD DiscountCurve/USD/9/15Y DiscountCurve/USD/11/30Y", 0}};
    // sensitivity analysis
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, false);
    sa->generateSensitivities();
    map<pair<string, string>, Real> deltaMap;
    map<pair<string, string>, Real> gammaMap;
    for (auto [pid,p] : portfolio->trades()) {
        for (const auto& f : sa->sensiCube()->factors()) {
            auto des = sa->sensiCube()->factorDescription(f);
            deltaMap[make_pair(pid, des)] = sa->sensiCube()->delta(pid, f);
            gammaMap[make_pair(pid, des)] = sa->sensiCube()->gamma(pid, f);
        }
    }
    std::vector<ore::analytics::SensitivityScenarioGenerator::ScenarioDescription> scenDesc =
        sa->scenarioGenerator()->scenarioDescriptions();

    Real shiftSize = 1E-5; // shift size

    // check deltas
    BOOST_TEST_MESSAGE("Checking deltas...");
    Size foundDeltas = 0, zeroDeltas = 0;
    for (auto const& x : deltaMap) {
        string key = x.first.first + " " + x.first.second;
        Real scaledResult = x.second / shiftSize;
        if (analyticalResultsDelta.count(key) > 0) {
            if (!check(analyticalResultsDelta.at(key), scaledResult))
                BOOST_ERROR("Sensitivity analysis result " << key << " (" << scaledResult
                                                           << ") could not be verified against analytic result ("
                                                           << analyticalResultsDelta.at(key) << ")");
            ++foundDeltas;
        } else {
            if (!close_enough(x.second, 0.0))
                BOOST_ERROR("Sensitivity analysis result " << key << " (" << scaledResult << ") expected to be zero");
            ++zeroDeltas;
        }
    }
    if (foundDeltas != analyticalResultsDelta.size())
        BOOST_ERROR("Mismatch between number of analytical results for delta ("
                    << analyticalResultsDelta.size() << ") and sensitivity results (" << foundDeltas << ")");
    BOOST_TEST_MESSAGE("Checked " << foundDeltas << " deltas against analytical values (and " << zeroDeltas
                                  << " deal-unrelated deltas for zero).");

    // check gammas
    BOOST_TEST_MESSAGE("Checking gammas...");
    Size foundGammas = 0, zeroGammas = 0;
    for (auto const& x : gammaMap) {
        string key = x.first.first + " " + x.first.second;
        Real scaledResult = x.second / (shiftSize * shiftSize);
        if (analyticalResultsGamma.count(key) > 0) {
            if (!check(analyticalResultsGamma.at(key), scaledResult))
                BOOST_ERROR("Sensitivity analysis result " << key << " (" << scaledResult
                                                           << ") could not be verified against analytic result ("
                                                           << analyticalResultsGamma.at(key) << ")");
            ++foundGammas;
        } else {
            // the sensi framework produces a Vomma, which we don't check (it isn't produced by the analytic sensi
            // engine)
            if (!close_enough(x.second, 0.0) && key != "5_Swaption_EUR SwaptionVolatility/EUR/47/10Y/10Y/ATM" &&
                key != "7_FxOption_EUR_USD FXVolatility/EURUSD/0/5Y/ATM")
                BOOST_ERROR("Sensitivity analysis result " << key << " (" << scaledResult << ") expected to be zero");
            ++zeroGammas;
        }
    }
    if (foundGammas != analyticalResultsGamma.size())
        BOOST_ERROR("Mismatch between number of analytical results for gamma ("
                    << analyticalResultsGamma.size() << ") and sensitivity results (" << foundGammas << ")");
    BOOST_TEST_MESSAGE("Checked " << foundGammas << " gammas against analytical values (and " << zeroGammas
                                  << " deal-unrelated gammas for zero).");

    // check cross gammas
    BOOST_TEST_MESSAGE("Checking cross-gammas...");
    Size foundCrossGammas = 0, zeroCrossGammas = 0;
    for (const auto& [tradeId, _] : portfolio->trades()) {
        for (auto const& s : scenDesc) {
            if (s.type() == ShiftScenarioGenerator::ScenarioDescription::Type::Cross) {
                string key = tradeId + " " + s.factor1() + " " + s.factor2();
                Real crossGamma = sa->sensiCube()->crossGamma(tradeId, make_pair(s.key1(), s.key2()));
                Real scaledResult = crossGamma / (shiftSize * shiftSize);
                // BOOST_TEST_MESSAGE(key << " " << scaledResult); // debug
                if (analyticalResultsCrossGamma.count(key) > 0) {
                    if (!check(analyticalResultsCrossGamma.at(key), scaledResult))
                        BOOST_ERROR("Sensitivity analysis result "
                                    << key << " (" << scaledResult
                                    << ") could not be verified against analytic result ("
                                    << analyticalResultsCrossGamma.at(key) << ")");
                    ++foundCrossGammas;
                } else {
                    if (!check(crossGamma, 0.0))
                        BOOST_ERROR("Sensitivity analysis result " << key << " (" << crossGamma
                                                                   << ") expected to be zero");
                    ++zeroCrossGammas;
                }
            }
        }
    }
    if (foundCrossGammas != analyticalResultsCrossGamma.size())
        BOOST_ERROR("Mismatch between number of analytical results for gamma ("
                    << analyticalResultsCrossGamma.size() << ") and sensitivity results (" << foundCrossGammas << ")");
    BOOST_TEST_MESSAGE("Checked " << foundCrossGammas << " cross gammas against analytical values (and "
                                  << zeroCrossGammas << " deal-unrelated cross gammas for zero).");

    // debug: dump analytical cross gamma results
    // for(auto const& x: analyticalResultsCrossGamma) {
    //     BOOST_TEST_MESSAGE(x.first << " " << x.second);
    // }

    ObservationMode::instance().setMode(backupMode);
    IndexManager::instance().clearHistories();

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
