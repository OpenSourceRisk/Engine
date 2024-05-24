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
#include <boost/test/unit_test.hpp>
#include <boost/timer/timer.hpp>
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
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <ored/model/lgmdata.hpp>
#include <ored/model/irlgmdata.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/osutils.hpp>
#include <oret/toplevelfixture.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <test/oreatoplevelfixture.hpp>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;
using boost::timer::cpu_timer;
using boost::timer::default_places;

using testsuite::TestMarket;

void setConventions() {
    QuantLib::ext::shared_ptr<data::Conventions> conventions(new data::Conventions());

    QuantLib::ext::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    QuantLib::ext::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    InstrumentConventions::instance().setConventions(conventions);
}

QuantLib::ext::shared_ptr<Portfolio> buildPortfolio(QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());

    string ccy = "EUR";
    string index = "EUR-EURIBOR-6M";
    string floatFreq = "6M";
    Real fixedRate = 0.02;
    string fixFreq = "1Y";
    Size term = 10;
    bool isPayer = true;

    Date today = Settings::instance().evaluationDate();
    Calendar cal = TARGET();
    string calStr = "TARGET";
    string conv = "MF";
    string rule = "Forward";
    Natural days = 2;
    string fixDC = "30/360";
    string floatDC = "ACT/360";

    vector<double> notional(1, 1000000);
    vector<double> spread(1, 0);

    Date startDate = cal.adjust(today + 1 * Months);
    Date endDate = cal.adjust(startDate + term * Years);

    // date 2 string
    ostringstream oss;
    oss << io::iso_date(startDate);
    string start(oss.str());
    oss.str("");
    oss.clear();
    oss << io::iso_date(endDate);
    string end(oss.str());

    // envelope
    Envelope env("CP");

    // Schedules
    ScheduleData floatSchedule(ScheduleRules(start, end, floatFreq, calStr, conv, conv, rule));
    ScheduleData fixedSchedule(ScheduleRules(start, end, fixFreq, calStr, conv, conv, rule));

    // fixed Leg - with dummy rate
    LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, fixedRate)), isPayer, ccy, fixedSchedule, fixDC,
                     notional);

    // float Leg
    vector<double> spreads(1, 0);
    LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, days, false, spread), !isPayer, ccy, floatSchedule,
                        floatDC, notional);

    QuantLib::ext::shared_ptr<Trade> swap(new data::Swap(env, floatingLeg, fixedLeg));

    swap->id() = "SWAP";

    portfolio->add(swap);

    portfolio->build(factory);

    return portfolio;
}

void simulation(string dateGridString, bool checkFixings) {
    SavedSettings backup;

    // Log::instance().registerLogger(QuantLib::ext::make_shared<StderrLogger>());
    // Log::instance().switchOn();

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    // string dateGridStr = "80,3M"; // 20 years
    QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>(dateGridString);
    Size samples = 100;

    BOOST_TEST_MESSAGE("Date Grid : " << dateGridString);

    // build model
    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("GBP");
    ccys.push_back("CHF");
    ccys.push_back("USD");
    ccys.push_back("JPY");

    // Init market
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> parameters(new analytics::ScenarioSimMarketParameters());
    parameters->baseCcy() = "EUR";
    parameters->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    parameters->setYieldCurveTenors("",
                                    {1 * Months, 6 * Months, 1 * Years, 2 * Years, 5 * Years, 10 * Years, 20 * Years});
    parameters->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});
    parameters->interpolation() = "LogLinear";

    parameters->setSwapVolTerms("", {6 * Months, 1 * Years});
    parameters->setSwapVolExpiries("", {1 * Years, 2 * Years});
    parameters->setSwapVolKeys(ccys);
    parameters->swapVolDecayMode() = "ForwardVariance";
    parameters->setSimulateSwapVols(false);

    parameters->setFxVolExpiries("",
        vector<Period>{1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    parameters->setFxVolDecayMode(string("ConstantVariance"));
    parameters->setSimulateFXVols(false);

    parameters->setFxVolCcyPairs({"USDEUR", "GBPEUR", "CHFEUR", "JPYEUR"});

    parameters->setFxCcyPairs({"USDEUR", "GBPEUR", "CHFEUR", "JPYEUR"});

    parameters->additionalScenarioDataIndices() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M",
                                                   "JPY-LIBOR-6M"};
    parameters->additionalScenarioDataCcys() = {"EUR", "GBP", "USD", "CHF", "JPY"};

    // Config

    // Build IR configurations
    CalibrationType calibrationType = CalibrationType::Bootstrap;
    LgmData::ReversionType revType = LgmData::ReversionType::HullWhite;
    LgmData::VolatilityType volType = LgmData::VolatilityType::Hagan;
    vector<string> swaptionExpiries = {"1Y", "2Y", "3Y", "5Y", "7Y", "10Y", "15Y", "20Y", "30Y"};
    vector<string> swaptionTerms = {"5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y", "5Y"};
    vector<string> swaptionStrikes(swaptionExpiries.size(), "ATM");
    vector<Time> hTimes = {};
    vector<Time> aTimes = {};

    std::vector<QuantLib::ext::shared_ptr<IrModelData>> irConfigs;

    vector<Real> hValues = {0.02};
    vector<Real> aValues = {0.008};
    irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
        "EUR", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.03};
    aValues = {0.009};
    irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
        "USD", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
        "GBP", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
        "CHF", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(QuantLib::ext::make_shared<IrLgmData>(
        "JPY", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    // Compile FX configurations
    vector<string> optionExpiries = {"1Y", "2Y", "3Y", "5Y", "7Y", "10Y"};
    vector<string> optionStrikes(optionExpiries.size(), "ATMF");
    vector<Time> sigmaTimes = {};

    std::vector<QuantLib::ext::shared_ptr<FxBsData>> fxConfigs;
    vector<Real> sigmaValues = {0.15};
    fxConfigs.push_back(QuantLib::ext::make_shared<FxBsData>("USD", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    sigmaValues = {0.20};
    fxConfigs.push_back(QuantLib::ext::make_shared<FxBsData>("GBP", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    sigmaValues = {0.20};
    fxConfigs.push_back(QuantLib::ext::make_shared<FxBsData>("CHF", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    sigmaValues = {0.20};
    fxConfigs.push_back(QuantLib::ext::make_shared<FxBsData>("JPY", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    map<CorrelationKey, Handle<Quote>> corr;
    CorrelationFactor f_1{CrossAssetModel::AssetType::IR, "EUR", 0};
    CorrelationFactor f_2{CrossAssetModel::AssetType::IR, "USD", 0};
    corr[make_pair(f_1, f_2)] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.6));

    QuantLib::ext::shared_ptr<CrossAssetModelData> config(QuantLib::ext::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, corr));

    // Model Builder & Model
    // model builder
    QuantLib::ext::shared_ptr<CrossAssetModelBuilder> modelBuilder(new CrossAssetModelBuilder(initMarket, config));
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = *modelBuilder->model();
    modelBuilder = NULL;

    // Path generator
    BigNatural seed = 5;
    bool antithetic = false;
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(model->stateProcess())) {
        tmp->resetCache(dg->timeGrid().size() - 1);
    }
    QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGen =
        QuantLib::ext::make_shared<MultiPathGeneratorMersenneTwister>(model->stateProcess(), dg->timeGrid(), seed, antithetic);

    // build scenario sim market
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarket> simMarket =
        QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, parameters);

    // build scenario generator
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory =
        QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator = QuantLib::ext::make_shared<CrossAssetModelScenarioGenerator>(
        model, pathGen, scenarioFactory, parameters, today, dg, initMarket);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // Build Portfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, simMarket);

    QuantLib::ext::shared_ptr<Portfolio> portfolio = buildPortfolio(factory);

    // Storage for selected scenario data (index fixings, FX rates, ..)
    if (checkFixings) {
        QuantLib::ext::shared_ptr<InMemoryAggregationScenarioData> inMemoryScenarioData =
            QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(dg->size(), samples);
        // Set AggregationScenarioData
        simMarket->aggregationScenarioData() = inMemoryScenarioData;
    }

    // Now calculate exposure
    ValuationEngine valEngine(today, dg, simMarket);

    // Calculate Cube
    cpu_timer t;
    QuantLib::ext::shared_ptr<NPVCube> cube =
        QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(today, portfolio->ids(), dg->dates(), samples);
    vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(baseCcy));
    valEngine.buildCube(portfolio, cube, calculators);
    t.stop();

    BOOST_TEST_MESSAGE("Cube generated in " << t.format(default_places, "%w") << " seconds");

    map<string, vector<Real>> referenceFixings;
    // First 10 EUR-EURIBOR-6M fixings at dateIndex 5, date grid 11,1Y
    referenceFixings["11,1Y"] = {0.00739033, 0.0281673, 0.0344399, 0.03362,   0.0325276, 0.030573,
                                 0.00895957, 0.0165584, 0.0194418, 0.0112834, 0.0239227};

    // First 10 EUR-EURIBOR-6M fixings at dateIndex 5, date grid 10,1Y
    referenceFixings["10,1Y"] = {0.00739033, 0.0296998, 0.0339535, 0.012449,  0.0134939, 0.0148095,
                                 0.0188541,  0.0277254, 0.035063,  0.0105228, 0.0103237};

    if (simMarket->aggregationScenarioData()) {
        QL_REQUIRE(dateGridString == "10,1Y" || dateGridString == "11,1Y",
                   "date grid string " << dateGridString << " unexpected");
        // Reference scenario data:
        Size dateIndex = 5;
        Size maxSample = 10;
        string qualifier = "EUR-EURIBOR-6M";
        Real tolerance = 1.0e-6;
        for (Size sampleIndex = 0; sampleIndex <= maxSample; ++sampleIndex) {
            Real fix = simMarket->aggregationScenarioData()->get(dateIndex, sampleIndex,
                                                                 AggregationScenarioDataType::IndexFixing, qualifier);
            Real ref = referenceFixings[dateGridString][sampleIndex];
            if (fabs(fix - ref) > tolerance)
                BOOST_FAIL("Stored fixing differs from reference value, found " << fix << ", expected " << ref);
        }
    }
}

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(ObservationModeTest)

BOOST_AUTO_TEST_CASE(testDisableShort) {
    ObservationMode::instance().setMode(ObservationMode::Mode::Disable);
    setConventions();

    BOOST_TEST_MESSAGE("Testing Observation Mode Disable, Short Grid, No Fixing Checks");
    simulation("10,1Y", false);

    BOOST_TEST_MESSAGE("Testing Observation Mode Disable, Short Grid, With Fixing Checks");
    simulation("10,1Y", true);
}

BOOST_AUTO_TEST_CASE(testDisableLong) {
    ObservationMode::instance().setMode(ObservationMode::Mode::Disable);
    setConventions();

    BOOST_TEST_MESSAGE("Testing Observation Mode Disable, Long Grid, No Fixing Checks");
    simulation("11,1Y", false);

    BOOST_TEST_MESSAGE("Testing Observation Mode Disable, Long Grid, With Fixing Checks");
    simulation("11,1Y", true);
}

BOOST_AUTO_TEST_CASE(testNone) {
    ObservationMode::instance().setMode(ObservationMode::Mode::None);
    setConventions();

    BOOST_TEST_MESSAGE("Testing Observation Mode None, Short Grid, No Fixing Checks");
    simulation("10,1Y", false);

    BOOST_TEST_MESSAGE("Testing Observation Mode None, Short Grid, With Fixing Checks");
    simulation("10,1Y", true);

    BOOST_TEST_MESSAGE("Testing Observation Mode None, Long Grid, No Fixing Checks");
    simulation("11,1Y", false);

    BOOST_TEST_MESSAGE("Testing Observation Mode None, Long Grid, With Fixing Checks");
    simulation("11,1Y", true);
}

BOOST_AUTO_TEST_CASE(testUnregister) {
    ObservationMode::instance().setMode(ObservationMode::Mode::Unregister);
    setConventions();

    BOOST_TEST_MESSAGE("Testing Observation Mode Unregister, Long Grid, No Fixing Checks");
    simulation("11,1Y", false);

    BOOST_TEST_MESSAGE("Testing Observation Mode Unregister, Long Grid, With Fixing Checks");
    simulation("11,1Y", true);

    BOOST_TEST_MESSAGE("Testing Observation Mode Unregister, Short Grid, No Fixing Checks");
    simulation("10,1Y", false);

    BOOST_TEST_MESSAGE("Testing Observation Mode Unregister, Short Grid, With Fixing Checks");
    simulation("10,1Y", true);
}

BOOST_AUTO_TEST_CASE(testDefer) {
    ObservationMode::instance().setMode(ObservationMode::Mode::Defer);
    setConventions();

    BOOST_TEST_MESSAGE("Testing Observation Mode Defer, Long Grid, No Fixing Checks");
    simulation("11,1Y", false);

    BOOST_TEST_MESSAGE("Testing Observation Mode Defer, Long Grid, With Fixing Checks");
    simulation("11,1Y", true);

    BOOST_TEST_MESSAGE("Testing Observation Mode Defer, Short Grid, No Fixing Checks");
    simulation("10,1Y", false);

    BOOST_TEST_MESSAGE("Testing Observation Mode Defer, Short Grid, With Fixing Checks");
    simulation("10,1Y", true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
