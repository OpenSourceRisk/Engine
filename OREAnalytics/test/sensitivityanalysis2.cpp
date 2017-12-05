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

#include "sensitivityanalysis2.hpp"
#include "testmarket.hpp"
#include "testportfolio.hpp"

#include <orea/cube/inmemorycube.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/engine/all.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/sensitivityanalysis.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariogenerator.hpp>

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

#include <boost/timer.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

namespace testsuite {

namespace {
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

boost::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData5() {
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
        new analytics::ScenarioSimMarketParameters());

    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->setYieldCurveTenors("", {1 * Months, 6 * Months, 1 * Years, 2 * Years, 3 * Years, 4 * Years,
                                            5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years, 30 * Years});
    simMarketData->setYieldCurveDayCounters("", "ACT/ACT");
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
    simMarketData->setSwapVolDayCounters("", "ACT/ACT");
    simMarketData->fxVolExpiries() = {6 * Months, 1 * Years, 2 * Years,  3 * Years,
                                      5 * Years,  7 * Years, 10 * Years, 20 * Years};
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true; // false;
    simMarketData->fxVolCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"};
    simMarketData->fxVolIsSurface() = false;
    simMarketData->fxVolMoneyness() = {0};
    simMarketData->setFxVolDayCounters("", "ACT/ACT");
    
    simMarketData->fxCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};

    simMarketData->simulateCapFloorVols() = true;
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->capFloorVolCcys() = {"EUR", "USD"};
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->capFloorVolStrikes() = {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06};
    simMarketData->setCapFloorVolDayCounters("", "A365");

    return simMarketData;
}

boost::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5() {
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();

    SensitivityScenarioData::CurveShiftData cvsData;

    // identical to sim market tenor structure, we can only check this case, because the analytic engine
    // assumes either linear in zero or linear in log discount interpolation, while the sensitivity analysis
    // assumes a lienar in zero interpolation for rebucketing, but uses the linear in log discount interpolation
    // of the sim market yield curves for the scenario calculation
    cvsData.shiftTenors = {1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,  4 * Years,
                           5 * Years,  7 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};
    cvsData.shiftType = "Absolute";
    cvsData.shiftSize = 1E-5;

    SensitivityScenarioData::SpotShiftData fxsData;
    fxsData.shiftType = "Absolute";
    fxsData.shiftSize = 1E-5;

    SensitivityScenarioData::VolShiftData fxvsData;
    fxvsData.shiftType = "Absolute";
    fxvsData.shiftSize = 1E-5;
    fxvsData.shiftExpiries = {5 * Years};

    SensitivityScenarioData::CapFloorVolShiftData cfvsData;
    cfvsData.shiftType = "Absolute";
    cfvsData.shiftSize = 1E-5;
    cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
    cfvsData.shiftStrikes = {0.01, 0.02, 0.03, 0.04, 0.05};

    SensitivityScenarioData::SwaptionVolShiftData swvsData;
    swvsData.shiftType = "Absolute";
    swvsData.shiftSize = 1E-5;
    swvsData.shiftExpiries = {6 * Months, 1 * Years, 2 * Years,  3 * Years,
                              5 * Years,  7 * Years, 10 * Years, 20 * Years};
    swvsData.shiftTerms = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 20 * Years};

    sensiData->discountCurrencies() = {"EUR", "USD", "GBP", "CHF", "JPY"};
    sensiData->discountCurveShiftData()["EUR"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->discountCurveShiftData()["USD"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->discountCurveShiftData()["GBP"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->discountCurveShiftData()["JPY"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->discountCurveShiftData()["CHF"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->indexNames() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);
    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] = boost::make_shared<SensitivityScenarioData::CurveShiftData>(cvsData);

    sensiData->fxCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxShiftData()["EURUSD"] = fxsData;
    sensiData->fxShiftData()["EURGBP"] = fxsData;
    sensiData->fxShiftData()["EURJPY"] = fxsData;
    sensiData->fxShiftData()["EURCHF"] = fxsData;

    sensiData->fxVolCcyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY", "GBPCHF"};
    sensiData->fxVolShiftData()["EURUSD"] = fxvsData;
    sensiData->fxVolShiftData()["EURGBP"] = fxvsData;
    sensiData->fxVolShiftData()["EURJPY"] = fxvsData;
    sensiData->fxVolShiftData()["EURCHF"] = fxvsData;
    sensiData->fxVolShiftData()["GBPCHF"] = fxvsData;

    sensiData->swaptionVolCurrencies() = {"EUR", "USD", "GBP", "CHF", "JPY"};
    sensiData->swaptionVolShiftData()["EUR"] = swvsData;
    sensiData->swaptionVolShiftData()["GBP"] = swvsData;
    sensiData->swaptionVolShiftData()["USD"] = swvsData;
    sensiData->swaptionVolShiftData()["JPY"] = swvsData;
    sensiData->swaptionVolShiftData()["CHF"] = swvsData;

    sensiData->capFloorVolCurrencies() = {"EUR", "USD"};
    sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
    sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] = cfvsData;
    sensiData->capFloorVolShiftData()["USD"].indexName = "USD-LIBOR-3M";

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

void SensitivityAnalysis2Test::testSensitivities() {

    BOOST_TEST_MESSAGE("Checking sensitivity analysis results vs analytic sensi engine results...");

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

    // build scenario sim market
    Conventions conventions = *conv();

    // build porfolio
    boost::shared_ptr<EngineData> data = boost::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("CrossCurrencySwap") = "DiscountedCashflows";
    data->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    data->model("EuropeanSwaption") = "BlackBachelier";
    data->engine("EuropeanSwaption") = "BlackBachelierSwaptionEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";

    // boost::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    boost::shared_ptr<Portfolio> portfolio(new Portfolio());
    portfolio->add(
        buildSwap("1_Swap_EUR", "EUR", true, 10.0, 0, 10, 0.03, 0.00, "1Y", "30/360", "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(buildEuropeanSwaption("5_Swaption_EUR", "Long", "EUR", true, 10.0, 10, 10, 0.03, 0.00, "1Y",
                                         "30/360", "6M", "A360", "EUR-EURIBOR-6M", "Physical"));
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
                                                {"5_Swaption_EUR DiscountCurve/EUR/0/1M", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/1/6M", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/10/20Y", -0.747105},
                                                {"5_Swaption_EUR DiscountCurve/EUR/11/30Y", 7.54828e-05},
                                                {"5_Swaption_EUR DiscountCurve/EUR/2/1Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/3/2Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/4/3Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/5/4Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/6/5Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/7/7Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/8/10Y", -0.53418},
                                                {"5_Swaption_EUR DiscountCurve/EUR/9/15Y", -1.3424},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/10/20Y", 53.6536},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/11/30Y", 0.0210198},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y", -29.6507},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/9/15Y", 4.23344},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/0/6M/1Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/1/6M/2Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/10/1Y/5Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/11/1Y/7Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/12/1Y/10Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/13/1Y/20Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/14/2Y/1Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/15/2Y/2Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/16/2Y/3Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/17/2Y/5Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/18/2Y/7Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/19/2Y/10Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/2/6M/3Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/20/2Y/20Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/21/3Y/1Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/22/3Y/2Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/23/3Y/3Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/24/3Y/5Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/25/3Y/7Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/26/3Y/10Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/27/3Y/20Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/28/5Y/1Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/29/5Y/2Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/3/6M/5Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/30/5Y/3Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/31/5Y/5Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/32/5Y/7Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/33/5Y/10Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/34/5Y/20Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/35/7Y/1Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/36/7Y/2Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/37/7Y/3Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/38/7Y/5Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/39/7Y/7Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/4/6M/7Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/40/7Y/10Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/41/7Y/20Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/42/10Y/1Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/43/10Y/2Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/44/10Y/3Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/45/10Y/5Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/46/10Y/7Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/47/10Y/10Y/ATM", 1.78576},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/48/10Y/20Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/49/20Y/1Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/5/6M/10Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/50/20Y/2Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/51/20Y/3Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/52/20Y/5Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/53/20Y/7Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/54/20Y/10Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/55/20Y/20Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/6/6M/20Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/7/1Y/1Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/8/1Y/2Y/ATM", 0},
                                                {"5_Swaption_EUR SwaptionVolatility/EUR/9/1Y/3Y/ATM", 0},
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
                                                {"5_Swaption_EUR DiscountCurve/EUR/0/1M", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/1/6M", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/10/20Y", 9.16378},
                                                {"5_Swaption_EUR DiscountCurve/EUR/11/30Y", -4.94345e-07},
                                                {"5_Swaption_EUR DiscountCurve/EUR/2/1Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/3/2Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/4/3Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/5/4Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/6/5Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/7/7Y", 0},
                                                {"5_Swaption_EUR DiscountCurve/EUR/8/10Y", 3.7521},
                                                {"5_Swaption_EUR DiscountCurve/EUR/9/15Y", 13.0565},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/10/20Y", 8237.22},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/11/30Y", 0.00142014},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y", 2512.58},
                                                {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/9/15Y", 177.559},
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
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M DiscountCurve/EUR/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/0/1M IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M DiscountCurve/EUR/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/1/6M IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y DiscountCurve/EUR/11/30Y", -0.00135078},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/10/20Y", -540.615},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/11/30Y", -0.404414},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/8/10Y", -22.3523},
        {"5_Swaption_EUR DiscountCurve/EUR/10/20Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 439.141},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/10/20Y", -0.00991954},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/11/30Y", -0.000159855},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/8/10Y", -0.0175782},
        {"5_Swaption_EUR DiscountCurve/EUR/11/30Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0.0338876},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y DiscountCurve/EUR/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/2/1Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y DiscountCurve/EUR/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/3/2Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y DiscountCurve/EUR/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/4/3Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y DiscountCurve/EUR/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/5/4Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y DiscountCurve/EUR/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/6/5Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y DiscountCurve/EUR/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y DiscountCurve/EUR/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y DiscountCurve/EUR/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y DiscountCurve/EUR/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/7/7Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y DiscountCurve/EUR/10/20Y", -0.111935},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y DiscountCurve/EUR/11/30Y", -8.79058e-05},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y DiscountCurve/EUR/9/15Y", 2.46712},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/10/20Y", -22.4684},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/11/30Y", -0.00880242},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 148.263},
        {"5_Swaption_EUR DiscountCurve/EUR/8/10Y IndexCurve/EUR-EURIBOR-6M/9/15Y", -205.555},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y DiscountCurve/EUR/10/20Y", 4.50176},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y DiscountCurve/EUR/11/30Y", 1.29719e-05},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/0/1M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/10/20Y", -365.581},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0.00129894},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 239.064},
        {"5_Swaption_EUR DiscountCurve/EUR/9/15Y IndexCurve/EUR-EURIBOR-6M/9/15Y", -84.4702},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/1/6M", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/0/1M IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/2/1Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/1/6M IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/10/20Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 3.22683},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/3/2Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/2/1Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/4/3Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/3/2Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/5/4Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/4/3Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/6/5Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/5/4Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/7/7Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/6/5Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y IndexCurve/EUR-EURIBOR-6M/8/10Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/7/7Y IndexCurve/EUR-EURIBOR-6M/9/15Y", 0},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y IndexCurve/EUR-EURIBOR-6M/10/20Y", -4492.95},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y IndexCurve/EUR-EURIBOR-6M/11/30Y", -1.76019},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/8/10Y IndexCurve/EUR-EURIBOR-6M/9/15Y", -398.959},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/9/15Y IndexCurve/EUR-EURIBOR-6M/10/20Y", 561.152},
        {"5_Swaption_EUR IndexCurve/EUR-EURIBOR-6M/9/15Y IndexCurve/EUR-EURIBOR-6M/11/30Y", 0.219937},
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
    boost::shared_ptr<SensitivityAnalysis> sa = boost::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, conventions, false);
    sa->generateSensitivities();
    map<pair<string, string>, Real> deltaMap;
    map<pair<string, string>, Real> gammaMap;
    for (auto p : portfolio->trades()) { 
        for (auto f : sa->sensiCube()->upFactors()) { 
            deltaMap[make_pair(p->id(), f.first)] = sa->delta(p->id(), f.first); 
            gammaMap[make_pair(p->id(), f.first)] = sa->gamma(p->id(), f.first); 
        } 
    }
    std::vector<ore::analytics::SensitivityScenarioGenerator::ScenarioDescription> scenDesc = sa->scenarioGenerator()->scenarioDescriptions();

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
    for (Size i=0; i<portfolio->size(); i++) {
        string id = portfolio->trades()[i]->id();
        for (auto const& s : scenDesc) {
            if (s.type() == ShiftScenarioGenerator::ScenarioDescription::Type::Cross) {
                string key =  id + " " + s.factor1() + " " + s.factor2();
                Real scaledResult = sa->crossGamma(id, s.factor1(), s.factor2()) / (shiftSize * shiftSize);
                // BOOST_TEST_MESSAGE(key << " " << scaledResult); // debug
                if (analyticalResultsCrossGamma.count(key) > 0) {
                    if (!check(analyticalResultsCrossGamma.at(key), scaledResult))
                        BOOST_ERROR("Sensitivity analysis result " << key << " (" << scaledResult
                                                            << ") could not be verified against analytic result ("
                                                            << analyticalResultsCrossGamma.at(key) << ")");
                    ++foundCrossGammas;
                } else {
                    if (!check(sa->crossGamma(id, s.factor1(), s.factor2()), 0.0))
                        BOOST_ERROR("Sensitivity analysis result " << key << " (" << sa->crossGamma(id, s.factor1(), s.factor2()) << ") expected to be zero");
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

test_suite* SensitivityAnalysis2Test::suite() {

    test_suite* suite = BOOST_TEST_SUITE("SensitivityAnalysis2Test");
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysis2Test::testSensitivities));
    return suite;
}
} // namespace testsuite
