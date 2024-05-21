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

#include <oret/toplevelfixture.hpp>
#include <test/oreatoplevelfixture.hpp>
#include "testmarket.hpp"
#include "testportfolio.hpp"

#include <orea/scenario/deltascenariofactory.hpp>
#include <qle/pricingengines/analyticeuropeanenginedeltagamma.hpp>
#include <qle/pricingengines/blackswaptionenginedeltagamma.hpp>
#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>

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
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
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

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

using ore::analytics::DeltaScenarioFactory;

namespace {
QuantLib::ext::shared_ptr<ore::data::Conventions> conv() {
    QuantLib::ext::shared_ptr<ore::data::Conventions> conventions(new ore::data::Conventions());

    QuantLib::ext::shared_ptr<ore::data::Convention> swapIndexConv(
        new ore::data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    // QuantLib::ext::shared_ptr<ore::data::Convention> swapConv(
    //     new ore::data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("EUR-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "EUR-EURIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("USD-3M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-3M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("USD-6M-SWAP-CONVENTIONS", "TARGET", "Q", "MF",
                                                                "30/360", "USD-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("GBP-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "GBP-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("JPY-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "JPY-LIBOR-6M"));
    conventions->add(QuantLib::ext::make_shared<ore::data::IRSwapConvention>("CHF-6M-SWAP-CONVENTIONS", "TARGET", "A", "MF",
                                                                "30/360", "CHF-LIBOR-6M"));

    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("EUR-DEP-CONVENTIONS", "EUR-EURIBOR"));
    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("USD-DEP-CONVENTIONS", "USD-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("GBP-DEP-CONVENTIONS", "GBP-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("JPY-DEP-CONVENTIONS", "JPY-LIBOR"));
    conventions->add(QuantLib::ext::make_shared<ore::data::DepositConvention>("CHF-DEP-CONVENTIONS", "CHF-LIBOR"));

    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-USD-FX", "0", "EUR", "USD", "10000", "EUR,USD"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-GBP-FX", "0", "EUR", "GBP", "10000", "EUR,GBP"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-CHF-FX", "0", "EUR", "CHF", "10000", "EUR,CHF"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-JPY-FX", "0", "EUR", "JPY", "10000", "EUR,JPY"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-SEK-FX", "0", "EUR", "SEK", "10000", "EUR,SEK"));
    conventions->add(QuantLib::ext::make_shared<FXConvention>("EUR-CAD-FX", "0", "EUR", "CAD", "10000", "EUR,CAD"));

    return conventions;
}

// QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> setupSimMarketData2() {
//     QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(
//         new analytics::ScenarioSimMarketParameters());
//     simMarketData->baseCcy() = "EUR";
//     simMarketData->ccys() = {"EUR", "GBP"};
//     simMarketData->yieldCurveTenors() = {1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years, 4 * Years,
//                                          5 * Years,  6 * Years,  7 * Years,  8 * Years,  9 * Years, 10 * Years,
//                                          12 * Years, 15 * Years, 20 * Years, 25 * Years, 30 * Years};
//     simMarketData->indices() = {"EUR-EURIBOR-6M", "GBP-LIBOR-6M"};
//     simMarketData->interpolation() = "LogLinear";
//     simMarketData->extrapolate() = true;

//     simMarketData->swapVolTerms() = {1 * Years, 2 * Years, 3 * Years,  4 * Years,
//                                      5 * Years, 7 * Years, 10 * Years, 20 * Years};
//     simMarketData->swapVolExpiries() = {6 * Months, 1 * Years, 2 * Years,  3 * Years,
//                                         5 * Years,  7 * Years, 10 * Years, 20 * Years};
//     simMarketData->swapVolCcys() = {"EUR", "GBP"};
//     simMarketData->swapVolDecayMode() = "ForwardVariance";
//     simMarketData->simulateSwapVols() = true;

//     simMarketData->fxVolExpiries() = {1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 *
//     Years};
//     simMarketData->fxVolDecayMode() = "ConstantVariance";
//     simMarketData->simulateFXVols() = true;
//     simMarketData->fxVolCcyPairs() = {"EURGBP"};

//     simMarketData->fxCcyPairs() = {"EURGBP"};

//     simMarketData->simulateCapFloorVols() = false;

//     return simMarketData;
// }

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
    simMarketData->extrapolation() = "FlatFwd";

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
    simMarketData->setFxVolIsSurface(true);
    simMarketData->setFxVolMoneyness(vector<Real>{0.1, 0.5, 1, 1.5, 2, 2.5, 2});

    simMarketData->setFxCcyPairs({"EURUSD", "EURGBP", "EURCHF", "EURJPY"});

    simMarketData->setSimulateCapFloorVols(true);
    simMarketData->capFloorVolDecayMode() = "ForwardVariance";
    simMarketData->setCapFloorVolKeys({"EUR", "USD"});
    simMarketData->setCapFloorVolExpiries(
        "", {6 * Months, 1 * Years, 2 * Years, 3 * Years, 5 * Years, 7 * Years, 10 * Years, 15 * Years, 20 * Years});
    simMarketData->setCapFloorVolStrikes("", {0.00, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06});

    return simMarketData;
}

// QuantLib::ext::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData2() {
//     QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = QuantLib::ext::make_shared<SensitivityScenarioData>();

//     sensiData->parConversion() = false;

//     SensitivityScenarioData::CurveShiftData cvsData;
//     cvsData.shiftTenors = {1 * Years, 2 * Years,  3 * Years,  5 * Years,
//                            7 * Years, 10 * Years, 15 * Years, 20 * Years}; // multiple tenors: triangular shifts
//     cvsData.shiftType = ShiftType::Absolute;
//     cvsData.shiftSize = 0.0001;
//     cvsData.parInstruments = {"DEP", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS"};

//     SensitivityScenarioData::FxShiftData fxsData;
//     fxsData.shiftType = ShiftType::Relative;
//     fxsData.shiftSize = 0.01;

//     SensitivityScenarioData::FxVolShiftData fxvsData;
//     fxvsData.shiftType = ShiftType::Relative;
//     fxvsData.shiftSize = 1.0;
//     fxvsData.shiftExpiries = {2 * Years, 5 * Years};

//     SensitivityScenarioData::CapFloorVolShiftData cfvsData;
//     cfvsData.shiftType = ShiftType::Absolute;
//     cfvsData.shiftSize = 0.0001;
//     cfvsData.shiftExpiries = {1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};
//     cfvsData.shiftStrikes = {0.05};

//     SensitivityScenarioData::SwaptionVolShiftData swvsData;
//     swvsData.shiftType = ShiftType::Relative;
//     swvsData.shiftSize = 0.01;
//     swvsData.shiftExpiries = {3 * Years, 5 * Years, 10 * Years};
//     swvsData.shiftTerms = {2 * Years, 5 * Years, 10 * Years};

//     sensiData->discountCurveShiftData()["EUR"] = cvsData;
//     sensiData->discountCurveShiftData()["EUR"].parInstrumentSingleCurve = true;
//     sensiData->discountCurveShiftData()["EUR"].parInstrumentConventions = {{"DEP", "EUR-DEP-CONVENTIONS"},
//                                                                            {"IRS", "EUR-6M-SWAP-CONVENTIONS"}};
//     sensiData->discountCurveShiftData()["GBP"] = cvsData;
//     sensiData->discountCurveShiftData()["GBP"].parInstrumentSingleCurve = true;
//     sensiData->discountCurveShiftData()["GBP"].parInstrumentConventions = {{"DEP", "GBP-DEP-CONVENTIONS"},
//                                                                            {"IRS", "GBP-6M-SWAP-CONVENTIONS"}};

//     sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;
//     sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"].parInstrumentSingleCurve = false;
//     sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"].parInstrumentConventions = {{"DEP", "EUR-DEP-CONVENTIONS"},
//                                                                                    {"IRS",
//                                                                                    "EUR-6M-SWAP-CONVENTIONS"}};
//     sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;
//     sensiData->indexCurveShiftData()["GBP-LIBOR-6M"].parInstrumentSingleCurve = false;
//     sensiData->indexCurveShiftData()["GBP-LIBOR-6M"].parInstrumentConventions = {{"DEP", "GBP-DEP-CONVENTIONS"},
//                                                                                  {"IRS", "GBP-6M-SWAP-CONVENTIONS"}};

//     sensiData->fxShiftData()["EURGBP"] = fxsData;

//     sensiData->fxVolShiftData()["EURGBP"] = fxvsData;

//     sensiData->swaptionVolShiftData()["EUR"] = swvsData;
//     sensiData->swaptionVolShiftData()["GBP"] = swvsData;

//     // sensiData->capFloorVolLabel() = "VOL_CAPFLOOR";
//     // sensiData->capFloorVolShiftData()["EUR"] = cfvsData;
//     // sensiData->capFloorVolShiftData()["EUR"].indexName = "EUR-EURIBOR-6M";
//     // sensiData->capFloorVolShiftData()["GBP"] = cfvsData;
//     // sensiData->capFloorVolShiftData()["GBP"].indexName = "GBP-LIBOR-6M";

//     return sensiData;
// }

QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData::CurveShiftParData> createCurveData() {
    ore::analytics::SensitivityScenarioData::CurveShiftParData cvsData;

    // identical to sim market tenor structure, we can only check this case, because the analytic engine
    // assumes either linear in zero or linear in log discount interpolation, while the sensitivity analysis
    // assumes a lienar in zero interpolation for rebucketing, but uses the linear in log discount interpolation
    // of the sim market yield curves for the scenario calculation
    cvsData.shiftTenors = {1 * Months, 6 * Months, 1 * Years,  2 * Years,  3 * Years,  4 * Years,
                           5 * Years,  7 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};
    cvsData.shiftType = ShiftType::Absolute;
    cvsData.shiftSize = 1E-5;
    cvsData.parInstruments = {"DEP", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS", "IRS"};

    return QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData::CurveShiftParData>(cvsData);
}
QuantLib::ext::shared_ptr<SensitivityScenarioData> setupSensitivityScenarioData5(bool parConversion) {
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData =
        QuantLib::ext::make_shared<ore::analytics::SensitivityScenarioData>(parConversion);

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

    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData::CurveShiftParData> cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = true;
    cvsData->parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["EUR"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = true;
    cvsData->parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "USD-3M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["USD"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = true;
    cvsData->parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["GBP"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = true;
    cvsData->parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["JPY"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = true;
    cvsData->parInstrumentConventions["DEP"] = "CHF-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "CHF-6M-SWAP-CONVENTIONS";
    sensiData->discountCurveShiftData()["CHF"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = false;
    cvsData->parInstrumentConventions["DEP"] = "EUR-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "EUR-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["EUR-EURIBOR-6M"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = false;
    cvsData->parInstrumentConventions["DEP"] = "USD-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "USD-3M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["USD-LIBOR-3M"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = false;
    cvsData->parInstrumentConventions["DEP"] = "GBP-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "GBP-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["GBP-LIBOR-6M"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = false;
    cvsData->parInstrumentConventions["DEP"] = "JPY-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "JPY-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["JPY-LIBOR-6M"] = cvsData;

    cvsData = createCurveData();
    cvsData->parInstrumentSingleCurve = true;
    cvsData->parInstrumentConventions["DEP"] = "CHF-DEP-CONVENTIONS";
    cvsData->parInstrumentConventions["IRS"] = "CHF-6M-SWAP-CONVENTIONS";
    sensiData->indexCurveShiftData()["CHF-LIBOR-6M"] = cvsData;

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
        QuantLib::ext::make_shared<SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
    sensiData->capFloorVolShiftData()["EUR"]->indexName = "EUR-EURIBOR-6M";
    sensiData->capFloorVolShiftData()["USD"] =
        QuantLib::ext::make_shared<SensitivityScenarioData::CapFloorVolShiftData>(cfvsData);
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

BOOST_AUTO_TEST_SUITE(SensitivityVsAnalyticSensiEnginesTest)
  
BOOST_AUTO_TEST_CASE(testSensitivities) {

    BOOST_TEST_MESSAGE("Checking sensitivity analysis results vs analytic sensi engine results...");

    SavedSettings backup;

    ObservationMode::Mode backupMode = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(ObservationMode::Mode::None);

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<testsuite::TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData = setupSimMarketData5();

    // sensitivity config
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiData = setupSensitivityScenarioData5(false);

    // build scenario sim market
    InstrumentConventions::instance().setConventions(conv());
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, simMarketData);

    // build scenario factory
    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = QuantLib::ext::make_shared<DeltaScenarioFactory>(baseScenario);

    // build scenario generator
    QuantLib::ext::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator =
        QuantLib::ext::make_shared<SensitivityScenarioGenerator>(sensiData, baseScenario, simMarketData, simMarket,
                                                         scenarioFactory, false);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // build porfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    data->model("CrossCurrencySwap") = "DiscountedCashflows";
    data->engine("CrossCurrencySwap") = "DiscountingCrossCurrencySwapEngine";
    data->model("FxOption") = "GarmanKohlhagen";
    data->engine("FxOption") = "AnalyticEuropeanEngine";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, simMarket);

    // QuantLib::ext::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());
    portfolio->add(testsuite::buildSwap("1_Swap_EUR", "EUR", true, 10.0, 0, 10, 0.03, 0.00, "1Y", "30/360",
                                                "6M", "A360", "EUR-EURIBOR-6M"));
    portfolio->add(testsuite::buildFxOption("7_FxOption_EUR_USD", "Long", "Call", 3, "EUR", 10.0, "USD", 11.0));
    portfolio->build(factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // analytic results

    std::map<string, Real> analyticalResultsDelta, analyticalResultsGamma, analyticalResultsCrossGamma;
    std::vector<Real> bucketTimes, bucketTimesVegaOpt, bucketTimesVegaUnd, bucketTimesFxOpt;
    std::vector<string> bucketStr, numStr, bucketStrVega, numStrVega, bucketStrFxVega, numStrFxVega;
    ActualActual dc(ActualActual::ISDA); // this is the dc used for the init / sim market curves
    Size i = 0;
    for (auto const& p : sensiData->discountCurveShiftData()["EUR"]->shiftTenors) {
        bucketTimes.push_back(dc.yearFraction(today, today + p));
        ostringstream bs;
        bs << p;
        bucketStr.push_back(bs.str());
        ostringstream num;
        num << i++;
        numStr.push_back(num.str());
    }
    for (auto const& p : sensiData->swaptionVolShiftData()["EUR"].shiftExpiries) {
        bucketTimesVegaOpt.push_back(dc.yearFraction(today, today + p));
    }
    for (auto const& p : sensiData->swaptionVolShiftData()["EUR"].shiftTerms) {
        bucketTimesVegaUnd.push_back(dc.yearFraction(today, today + p));
    }
    i = 0;
    for (auto const& p1 : sensiData->swaptionVolShiftData()["EUR"].shiftExpiries) {
        for (auto const& p2 : sensiData->swaptionVolShiftData()["EUR"].shiftTerms) {
            ostringstream bs, num;
            bs << p1 << "/" << p2;
            num << i++;
            bucketStrVega.push_back(bs.str());
            numStrVega.push_back(num.str());
        }
    }
    i = 0;
    for (auto const& p : sensiData->fxVolShiftData()["EURUSD"].shiftExpiries) {
        bucketTimesFxOpt.push_back(dc.yearFraction(today, today + p));
        ostringstream bs, num;
        bs << p;
        num << i++;
        bucketStrFxVega.push_back(bs.str());
        numStrFxVega.push_back(num.str());
    }

    Size n = bucketTimes.size();
    auto analyticSwapEngine = QuantLib::ext::make_shared<DiscountingSwapEngineDeltaGamma>(simMarket->discountCurve("EUR"),
                                                                                  bucketTimes, true, true, true, false);
    auto swap0 = portfolio->trades().begin()->second->instrument()->qlInstrument();
    swap0->setPricingEngine(analyticSwapEngine);
    auto deltaDsc0 = swap0->result<std::vector<Real>>("deltaDiscount");
    auto deltaFwd0 = swap0->result<std::vector<Real>>("deltaForward");
    auto gamma0 = swap0->result<Matrix>("gamma");

    auto process =
        QuantLib::ext::make_shared<GarmanKohlagenProcess>(simMarket->fxRate("EURUSD"), simMarket->discountCurve("EUR"),
                                                  simMarket->discountCurve("USD"), simMarket->fxVol("EURUSD"));
    auto analyticFxEngine =
        QuantLib::ext::make_shared<AnalyticEuropeanEngineDeltaGamma>(process, bucketTimes, bucketTimesFxOpt, true, true, false);
    auto fxoption2 = (++portfolio->trades().begin())->second->instrument()->qlInstrument();
    fxoption2->setPricingEngine(analyticFxEngine);
    Real deltaSpot2 = fxoption2->result<Real>("deltaSpot");
    Real gammaSpot2 = fxoption2->result<Real>("gammaSpot");
    auto vega2 = fxoption2->result<std::vector<Real>>("vega");
    auto deltaRate2 = fxoption2->result<std::vector<Real>>("deltaRate");
    auto deltaDividend2 = fxoption2->result<std::vector<Real>>("deltaDividend");
    auto gamma2 = fxoption2->result<Matrix>("gamma");
    Real fxnpv = fxoption2->NPV();
    Real fxspot = simMarket->fxRate("EURUSD")->value();

    std::vector<string> dscKey, dscKey2, fwdKey;
    for (Size i = 0; i < n; ++i) {
        dscKey.push_back("DiscountCurve/EUR/" + numStr[i] + "/" + bucketStr[i]);
        dscKey2.push_back("DiscountCurve/USD/" + numStr[i] + "/" + bucketStr[i]);
        fwdKey.push_back("IndexCurve/EUR-EURIBOR-6M/" + numStr[i] + "/" + bucketStr[i]);
    }

    for (Size i = 0; i < n; ++i) {
        analyticalResultsDelta["1_Swap_EUR " + dscKey[i]] = deltaDsc0[i];
        analyticalResultsDelta["1_Swap_EUR " + fwdKey[i]] = deltaFwd0[i];
        analyticalResultsDelta["7_FxOption_EUR_USD " + dscKey[i]] = deltaDividend2[i] * 10.0 / fxspot; // convert to EUR
        analyticalResultsDelta["7_FxOption_EUR_USD " + dscKey2[i]] = deltaRate2[i] * 10.0 / fxspot;    // convert to EUR
    }

    for (Size i = 0; i < n; ++i) {
        analyticalResultsGamma["1_Swap_EUR " + dscKey[i]] = gamma0[i][i];
        analyticalResultsGamma["1_Swap_EUR " + fwdKey[i]] = gamma0[n + i][n + i];
        analyticalResultsGamma["7_FxOption_EUR_USD " + dscKey[i]] =
            gamma2[n + i][n + i] * 10.0 / fxspot;                                                  // convert to EUR
        analyticalResultsGamma["7_FxOption_EUR_USD " + dscKey2[i]] = gamma2[i][i] * 10.0 / fxspot; // convert to EUR
        for (Size j = 0; j < n; ++j) {
            if (i < j) {
                analyticalResultsCrossGamma["1_Swap_EUR " + dscKey[i] + " " + dscKey[j]] = gamma0[i][j];
                analyticalResultsCrossGamma["1_Swap_EUR " + fwdKey[i] + " " + fwdKey[j]] = gamma0[n + i][n + j];
                analyticalResultsCrossGamma["7_FxOption_EUR_USD " + dscKey[i] + " " + dscKey[j]] =
                    gamma2[n + i][n + j] * 10.0 / fxspot; // convert to EUR
                analyticalResultsCrossGamma["7_FxOption_EUR_USD " + dscKey2[i] + " " + dscKey2[j]] =
                    gamma2[i][j] * 10.0 / fxspot; // convert to EUR
            }
            analyticalResultsCrossGamma["1_Swap_EUR " + dscKey[i] + " " + fwdKey[j]] = gamma0[i][n + j];
            analyticalResultsCrossGamma["7_FxOption_EUR_USD " + dscKey[i] + " " + dscKey2[j]] =
                gamma2[n + i][j] * 10.0 / fxspot; // convert to EUR
        }
    }

    // the sensitivity framework computes d/dS (npv/S), with S = EURUSD fx rate, npv = NPV in USD
    // the analytical engine computes d/dS npv, the first expression is
    // -npv/S^2+ d/dS npv / S
    // furthermore the analytical engine produces results for an EUR notional of 1 instead of 10
    analyticalResultsDelta["7_FxOption_EUR_USD FXSpot/EURUSD/0/spot"] =
        10.0 * (deltaSpot2 / fxspot - fxnpv / (fxspot * fxspot));
    // ... differentiate the above expression by S again gives
    // 2npv/S^3-2 d/dS npv / S^2 + d^2/dS^2 npv / S
    analyticalResultsGamma["7_FxOption_EUR_USD FXSpot/EURUSD/0/spot"] =
        10.0 * (2.0 * fxnpv / (fxspot * fxspot * fxspot) - 2.0 * deltaSpot2 / (fxspot * fxspot) + gammaSpot2 / fxspot);

    analyticalResultsDelta["7_FxOption_EUR_USD FXVolatility/EURUSD/0/5Y/ATM"] =
        vega2.front() * 10.0 / fxspot; // we only have one vega bucket

    // sensitivity analysis
    QuantLib::ext::shared_ptr<SensitivityAnalysis> sa = QuantLib::ext::make_shared<SensitivityAnalysis>(
        portfolio, initMarket, Market::defaultConfiguration, data, simMarketData, sensiData, false);
    sa->generateSensitivities();
    map<pair<string, string>, Real> deltaMap;
    map<pair<string, string>, Real> gammaMap;

    auto sensiCube = sa->sensiCube();
    for (const auto& tradeId : portfolio->ids()) {
        for (const auto& f : sensiCube->factors()) {
            string des = sensiCube->factorDescription(f);
            deltaMap[make_pair(tradeId, des)] = sensiCube->delta(tradeId, f);
            gammaMap[make_pair(tradeId, des)] = sensiCube->gamma(tradeId, f);
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
    for (const auto& [tradeId, trade] : portfolio->trades()) {
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
                        BOOST_ERROR("Sensitivity analysis result " << key << " (" << scaledResult
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
