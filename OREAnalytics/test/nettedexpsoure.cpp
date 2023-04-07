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
#include <orea/cube/npvcube.hpp>

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
#include <test/testmarket.hpp>

#include <orea/aggregation/exposurecalculator.hpp>
#include <orea/aggregation/nettedexposurecalculator.hpp>
#include <orea/aggregation/dimcalculator.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>
#include <orea/scenario/scenariogeneratordata.hpp>
#include <orea/scenario/aggregationscenariodata.hpp>
#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/portfolio/nettingsetdefinition.hpp>
#include <ored/portfolio/nettingsetmanager.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/engine/mporcalculator.hpp>
#include <ql/time/date.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <cmath>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

using testsuite::TestMarket;

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(CollateralisedExposureTest)

boost::shared_ptr<data::Conventions> convs() {
    boost::shared_ptr<data::Conventions> conventions(new data::Conventions());

    boost::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    boost::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    InstrumentConventions::instance().setConventions(conventions);

    return conventions;
}

boost::shared_ptr<Portfolio> buildPortfolio(Size portfolioSize, boost::shared_ptr<EngineFactory>& factory) {

    boost::shared_ptr<Portfolio> portfolio(new Portfolio());

    vector<string> ccys = {"EUR"};

    map<string, vector<string>> indices = {{"EUR", {"EUR-EURIBOR-6M"}}};

    vector<string> fixedTenors = {"1Y"};

    Size seed = 5; // keep this constant to ensure portfolio doesn't change
    MersenneTwisterUniformRng rng(seed);

    Date today = Settings::instance().evaluationDate();

    Calendar cal = TARGET();
    string calStr = "TARGET";
    string conv = "MF";
    string rule = "Forward";
    Size days = 2;
    string fixDC = "30/360";
    string floatDC = "ACT/365";

    vector<double> notional(1, 1000000);
    vector<double> spread(1, 0);

    for (Size i = 0; i < portfolioSize; i++) {
        Size term = 1;
        // Start today +/- 1 Year
        Date startDate = cal.adjust(today);
        Date endDate = cal.adjust(startDate + term * Years);

        // date 2 string
        ostringstream oss;
        oss << io::iso_date(startDate);
        string start(oss.str());
        oss.str("");
        oss.clear();
        oss << io::iso_date(endDate);
        string end(oss.str());

        // ccy + index
        string ccy = "EUR";
        string index = "EUR-EURIBOR-6M";
        string floatFreq =  "6M";

        // This variable is not used and only here to ensure that the random numbers generated in
        // subsequent blocks produce a swap portfolio, which is compatible with the archived values.
        string fixedTenor = "1Y";
        fixedTenor = fixedTenor + "_";

        // fixed details
        Real fixedRate =  0.02 ;
        string fixFreq = "1Y";

        // envelope
        Envelope env("CP", "NettingSet1");

        // Schedules
        ScheduleData floatSchedule(ScheduleRules(start, end, floatFreq, calStr, conv, conv, rule));
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixFreq, calStr, conv, conv, rule));

        bool isPayer = true;

        // fixed Leg - with dummy rate
        LegData fixedLeg(boost::make_shared<FixedLegData>(vector<double>(1, fixedRate)), isPayer, ccy, fixedSchedule,
                         fixDC, notional);

        // float Leg
        vector<double> spreads(1, 0);
        LegData floatingLeg(boost::make_shared<FloatingLegData>(index, days, false, spread), !isPayer, ccy,
                            floatSchedule, floatDC, notional);

        boost::shared_ptr<Trade> swap(new data::Swap(env, floatingLeg, fixedLeg));

        // id
        oss.clear();
        oss.str("");
        oss << "Trade_" << i + 1;
        swap->id() = oss.str();

        portfolio->add(swap);
    }
    // portfolio->save("port.xml");

    portfolio->build(factory);

    if (portfolio->size() != portfolioSize)
        BOOST_ERROR("Failed to build portfolio (got " << portfolio->size() << " expected " << portfolioSize << ")");

    // Dump stats about portfolio
    Time maturity = 0;
    DayCounter dc = ActualActual(ActualActual::ISDA);
    map<string, Size> fixedFreqs;
    map<string, Size> floatFreqs;
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        maturity += dc.yearFraction(today, trade->maturity());

        // fixed Freq
        boost::shared_ptr<data::Swap> swap = boost::dynamic_pointer_cast<data::Swap>(trade);
        string floatFreq = swap->legData()[0].schedule().rules().front().tenor();
        string fixFreq = swap->legData()[1].schedule().rules().front().tenor();
        QL_REQUIRE(swap->legData()[0].legType() == "Floating" && swap->legData()[1].legType() == "Fixed", "Leg mixup");
        if (fixedFreqs.find(fixFreq) == fixedFreqs.end())
            fixedFreqs[fixFreq] = 1;
        else
            fixedFreqs[fixFreq]++;
        if (floatFreqs.find(floatFreq) == floatFreqs.end())
            floatFreqs[floatFreq] = 1;
        else
            floatFreqs[floatFreq]++;
    }
    maturity /= portfolioSize;
    BOOST_TEST_MESSAGE("Portfolio Size    : " << portfolioSize);
    BOOST_TEST_MESSAGE("Maturity  : " << maturity);
    ostringstream oss;
    for (Size i = 0; i < ccys.size(); i++)
        oss << ccys[i] << " ";
    BOOST_TEST_MESSAGE("Currencies        : " << oss.str());
    // dump % breakdown of tenors
    map<string, Size>::iterator it;
    BOOST_TEST_MESSAGE("Fixed Tenors      : "<< fixedFreqs.begin()->first);
    BOOST_TEST_MESSAGE("Floating Tenors   : "<< floatFreqs.begin()->first);
    return portfolio;
}

boost::shared_ptr<CrossAssetModel> buildCrossAssetModel(boost::shared_ptr<Market>& initMarket){
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

    std::vector<boost::shared_ptr<IrModelData>> irConfigs;

    vector<Real> hValues = {0.02};
    vector<Real> aValues = {0.008};
    irConfigs.push_back(boost::make_shared<IrLgmData>(
        "EUR", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.03};
    aValues = {0.009};
    irConfigs.push_back(boost::make_shared<IrLgmData>(
        "USD", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));


    // Compile FX configurations
    vector<string> optionExpiries = {"1Y", "2Y", "3Y", "5Y", "7Y", "10Y"};
    vector<string> optionStrikes(optionExpiries.size(), "ATMF");
    vector<Time> sigmaTimes = {};

    std::vector<boost::shared_ptr<FxBsData>> fxConfigs;

    vector<Real> sigmaValues = {0.15};
    fxConfigs.push_back(boost::make_shared<FxBsData>("USD", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    map<CorrelationKey, Handle<Quote>> corr;
    CorrelationFactor f_1{ CrossAssetModel::AssetType::IR, "EUR", 0 };
    CorrelationFactor f_2{ CrossAssetModel::AssetType::IR, "USD", 0 };
    corr[make_pair(f_1, f_2)] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.6));

    boost::shared_ptr<CrossAssetModelData> config(boost::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, corr));

    // Model Builder & Model
    // model builder
    boost::shared_ptr<CrossAssetModelBuilder> modelBuilder(new CrossAssetModelBuilder(initMarket, config));
    return *modelBuilder->model();

}

boost::shared_ptr<analytics::ScenarioSimMarket> buildScenarioSimMarket(boost::shared_ptr<DateGrid> dateGrid,
                                                                       boost::shared_ptr<Market>& initMarket, 
                                                                       boost::shared_ptr<CrossAssetModel>& model,  
                                                                       Size samples=1,
                                                                       Size seed=5,
                                                                       bool antithetic=false){
    // build scenario sim market parameters
    Date today = initMarket->asofDate();

    string baseCcy = "EUR";
    vector<string> ccys;
    ccys.push_back(baseCcy);
    ccys.push_back("USD");

    boost::shared_ptr<analytics::ScenarioSimMarketParameters> parameters(new analytics::ScenarioSimMarketParameters());
    parameters->baseCcy() = baseCcy;
    parameters->setDiscountCurveNames(ccys);
    parameters->setYieldCurveTenors("",
                                    {1 * Months, 6 * Months, 1 * Years, 2 * Years, 5 * Years, 10 * Years, 20 * Years});
    parameters->setIndices({"EUR-EONIA", "EUR-EURIBOR-6M", "USD-LIBOR-3M"});

    parameters->interpolation() = "LogLinear";

    parameters->setSimulateSwapVols(false);
    parameters->setSwapVolTerms("", {6 * Months, 1 * Years});
    parameters->setSwapVolExpiries("", {1 * Years, 2 * Years});
    parameters->swapVolKeys() = ccys;
    parameters->swapVolDecayMode() = "ForwardVariance";

    parameters->setFxVolExpiries("",
        vector<Period>{1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    parameters->setFxVolDecayMode(string("ConstantVariance"));
    parameters->setSimulateFXVols(false);

    parameters->setFxVolCcyPairs({"USDEUR"});
    parameters->setFxCcyPairs({"USDEUR"});

    parameters->setAdditionalScenarioDataIndices({"EUR-EONIA"});
    parameters->setAdditionalScenarioDataCcys({"EUR"});

    // Path generator
    boost::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGen =
        boost::make_shared<MultiPathGeneratorMersenneTwister>(model->stateProcess(), dateGrid->timeGrid(), seed, antithetic);

    // build scenario generator
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    boost::shared_ptr<ScenarioGenerator> scenarioGenerator = boost::make_shared<CrossAssetModelScenarioGenerator>(model, 
                                                                                                                  pathGen, 
                                                                                                                  scenarioFactory, 
                                                                                                                  parameters, 
                                                                                                                  today, 
                                                                                                                  dateGrid, 
                                                                                                                  initMarket);

    // build scenario sim market
    convs();
    auto simMarket = boost::make_shared<analytics::ScenarioSimMarket>(initMarket, parameters);
    simMarket->scenarioGenerator() = scenarioGenerator;

    boost::shared_ptr<AggregationScenarioData> scenarioData = boost::make_shared<InMemoryAggregationScenarioData>(dateGrid->timeGrid().size(), samples);
    simMarket->aggregationScenarioData() = scenarioData;
    
    return simMarket;
}

boost::shared_ptr<NPVCube> buildNPVCube(boost::shared_ptr<DateGrid> dateGrid, 
                                        bool withCloseOutGrid,
                                        boost::shared_ptr<analytics::ScenarioSimMarket>& simMarket,
                                        boost::shared_ptr<Portfolio>& portfolio,
                                        bool mporStickyDate=false,
                                        Size samples=1,
                                        Size seed=5){

    Date today = Settings::instance().evaluationDate();
    // Now calculate exposure
    ValuationEngine valEngine(today, dateGrid, simMarket);

    Size depth = 1;
    if (withCloseOutGrid)
        depth = 2;

    // Calculate Cube
    boost::timer::cpu_timer t;
    //BOOST_TEST_MESSAGE("dg->numDates() " << dateGrid->valuationDates().size()<<", " <<"dg->dates() "<<dateGrid->dates().size());
    boost::shared_ptr<NPVCube> cube =
        boost::make_shared<DoublePrecisionInMemoryCubeN>(today, portfolio->ids(), dateGrid->valuationDates(), samples, depth);

    vector<boost::shared_ptr<ValuationCalculator>> calculators;
    boost::shared_ptr<NPVCalculator> npvCalc = boost::make_shared<NPVCalculator>("EUR");
    calculators.push_back(npvCalc);
    if (withCloseOutGrid)
        calculators.push_back(boost::make_shared<MPORCalculator>(npvCalc));
    BOOST_TEST_MESSAGE("mporStickyDate "<<mporStickyDate);
    valEngine.buildCube(portfolio, cube, calculators, mporStickyDate);
    t.stop();
    double elapsed = t.elapsed().wall * 1e-9;
    
    if (withCloseOutGrid){
        std::string fileName = "scenarioData_closeout.csv";
        saveAggregationScenarioData(fileName, *simMarket->aggregationScenarioData());
        fileName = "cube_closeout.csv";
        saveCube(fileName, *cube);
    }else{
        std::string fileName = "scenarioData.csv";
        saveAggregationScenarioData(fileName, *simMarket->aggregationScenarioData());
        fileName = "cube.csv";
        saveCube(fileName, *cube);
    }

    BOOST_TEST_MESSAGE("Cube generated in " << elapsed << " seconds");
    return cube;
}

struct TestData : ore::test::OreaTopLevelFixture {

    TestData(Date referenceDate, boost::shared_ptr<DateGrid> dateGrid, bool withCloseOutGrid = false, bool mporStickyDate = false, Size samples=1, Size seed=5){
        // Init market
        BOOST_TEST_MESSAGE("Setting initial market ...");
        this->initMarket_ = boost::make_shared<TestMarket>(referenceDate);
        BOOST_TEST_MESSAGE("Setting initial market done!");

        BOOST_TEST_MESSAGE("Building CAM ...");
        this->model_ = buildCrossAssetModel(this->initMarket_);
        BOOST_TEST_MESSAGE("Building CAM done!");

        BOOST_TEST_MESSAGE("Building SimMarket ...");
        this->simMarket_ = buildScenarioSimMarket(dateGrid, this->initMarket_, this->model_, samples, seed);
        BOOST_TEST_MESSAGE("Building SimMarket done!");

        // Build Portfolio
        BOOST_TEST_MESSAGE("Building Portfolio ...");
        boost::shared_ptr<EngineData> data = boost::make_shared<EngineData>();
        data->model("Swap") = "DiscountedCashflows";
        data->engine("Swap") = "DiscountingSwapEngine";
        boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(data, this->simMarket_);
        //factory->registerBuilder(boost::make_shared<SwapEngineBuilder>());
        Size portfolioSize = 1;
        this->portfolio_ = buildPortfolio(portfolioSize, factory);
        BOOST_TEST_MESSAGE("Building Portfolio done!");
        BOOST_TEST_MESSAGE("Portfolio size after build: " << this->portfolio_->size());

        BOOST_TEST_MESSAGE("Building NPV cube ...");
        this->cube_ = buildNPVCube(dateGrid, withCloseOutGrid, this->simMarket_, this->portfolio_, mporStickyDate, samples, seed);
        BOOST_TEST_MESSAGE("Building NPV done!");
    }

    boost::shared_ptr<Market> initMarket_;
    boost::shared_ptr<analytics::ScenarioSimMarket> simMarket_;
    boost::shared_ptr<QuantExt::CrossAssetModel> model_;
    boost::shared_ptr<NPVCube> cube_;
    boost::shared_ptr<Portfolio> portfolio_;
};

struct CachedResultsData : ore::test::OreaTopLevelFixture {

CachedResultsData(){

    tuple<string, string, string, string, string, string> key; 
    key = make_tuple("13,1W", "1W", "woCloseOutGrid", "ActualDate", "Symmetric", "woCompounding");
    vector<Date> defaultDate = {Date(42481),Date(42488),Date(42495),Date(42502),Date(42509),Date(42516),Date(42523),Date(42530),Date(42537),Date(42544),Date(42551),Date(42558),Date(42565)};
    vector<Real> defaultValue = {-5186.33839194918,-4905.49750714248,-4548.69910041274,-4934.74752203131,-4721.35208103624,-4728.18840345129,-4942.29809042893,-4829.79935176886,-4872.32605339788,-4661.93964349494,-4836.57610017441,-5209.59281418126,-5111.60742248725};
    vector<Date> closeOutDate = {};
    vector<Real> closeOutValue = {-4964.24586512324,-5186.33839194918,-4905.49750714248,-4548.69910041274,-4934.74752203131,-4721.35208103624,-4728.18840345129,-4942.29809042893,-4829.79935176886,-4872.32605339788,-4661.93964349494,-4836.57610017441,-5209.59281418126};
    defaultDates[key] = defaultDate;
    defaultValues[key] = defaultValue;
    closeOutDates[key] = closeOutDate;
    closeOutValues[key] = closeOutValue;

    key = make_tuple("13,1W", "1W", "woCloseOutGrid", "ActualDate", "AsymmetricCVA", "woCompounding");
    defaultDate = {Date(42481),Date(42488),Date(42495),Date(42502),Date(42509),Date(42516),Date(42523),Date(42530),Date(42537),Date(42544),Date(42551),Date(42558),Date(42565)};
    defaultValue = {-5186.33839194918,-4905.49750714248,-4548.69910041274,-4934.74752203131,-4721.35208103624,-4728.18840345129,-4942.29809042893,-4829.79935176886,-4872.32605339788,-4661.93964349494,-4836.57610017441,-5209.59281418126,-5111.60742248725};
    closeOutValue = {-5186.33839194918,-5186.33839194918,-4905.49750714248,-4934.74752203131,-4934.74752203131,-4728.18840345129,-4942.29809042893,-4942.29809042893,-4872.32605339788,-4872.32605339788,-4836.57610017441,-5209.59281418126,-5209.59281418126};
    defaultDates[key] = defaultDate;
    defaultValues[key] = defaultValue;
    closeOutDates[key] = closeOutDate;
    closeOutValues[key] = closeOutValue;


    key = make_tuple("13,1W", "1W", "woCloseOutGrid", "ActualDate", "AsymmetricDVA", "woCompounding");
    defaultDate = {Date(42481),Date(42488),Date(42495),Date(42502),Date(42509),Date(42516),Date(42523),Date(42530),Date(42537),Date(42544),Date(42551),Date(42558),Date(42565)};
    defaultValue = {-5186.33839194918,-4905.49750714248,-4548.69910041274,-4934.74752203131,-4721.35208103624,-4728.18840345129,-4942.29809042893,-4829.79935176886,-4872.32605339788,-4661.93964349494,-4836.57610017441,-5209.59281418126,-5111.60742248725};
    closeOutValue = {-4964.24586512324,-4905.49750714248,-4548.69910041274,-4548.69910041274,-4721.35208103624,-4721.35208103624,-4728.18840345129,-4829.79935176886,-4829.79935176886,-4661.93964349494,-4661.93964349494,-4836.57610017441,-5111.60742248725};
    defaultDates[key] = defaultDate;
    defaultValues[key] = defaultValue;
    closeOutDates[key] = closeOutDate;
    closeOutValues[key] = closeOutValue;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    key = make_tuple("13,1M", "1W", "withCloseOutGrid", "ActualDate", "NoLag", "woCompounding");
    defaultDate = {Date(42506),Date(42535),Date(42565),Date(42597),Date(42627),Date(42657),Date(42688),Date(42718),Date(42751),Date(42780),Date(42808),Date(42843),Date(42870)};
    defaultValue = {-5201.05244612274,-4824.95292840006,-4477.84062127441,-4840.59273964169,-4783.19175595342,-10033.5472051449,-10033.8518374855,-10042.17634737,-10050.6267511171,-10028.9670934353,-10053.5482862015,0,0};
    closeOutValue = {-5480.43076439459,-4433.281369965,-4468.94866890169,-4953.02218430623,-4985.17359043462,-10026.4183374549,-10030.1017618035,-10036.1263742983,-10048.9573202704,-10023.9263145357,-10050.3607175246,0,0};
    defaultDates[key] = defaultDate;
    defaultValues[key] = defaultValue;
    closeOutDates[key] = closeOutDate;
    closeOutValues[key] = closeOutValue;
    
    key = make_tuple("13,1M", "1W", "withCloseOutGrid", "ActualDate", "NoLag", "withCompounding");
    defaultDate = {Date(42506),Date(42535),Date(42565),Date(42597),Date(42627),Date(42657),Date(42688),Date(42718),Date(42751),Date(42780),Date(42808),Date(42843),Date(42870)};
    defaultValue = {-5201.05244612274,-4824.95292840006,-4477.84062127441,-4840.59273964169,-4783.19175595342,-10033.5472051449,-10033.8518374855,-10042.17634737,-10050.6267511171,-10028.9670934353,-10053.5482862015,0,0};
    closeOutValue = {-5480.43076439459,-4433.281369965,-4468.94866890169,-4953.02218430623,-4985.17359043462,-10026.4183374549,-10030.1017618035,-10036.1263742983,-10048.9573202704,-10023.9263145357,-10050.3607175246,0,0};
    defaultDates[key] = defaultDate;
    defaultValues[key] = defaultValue;
    closeOutDates[key] = closeOutDate;
    closeOutValues[key] = closeOutValue;
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}


struct Results {
        string dateGridStr;
        string nettingSetMporStr;
        string closeOutGridStr;
        string mporModeStr;
        string calcType;
        string compoundingStr;
        vector<Date> defaultDate;
        vector<Real> defaultValue;
        vector<Date> closeOutDate;
        vector<Real> closeOutValue;
};

std::map<tuple<string, string, string, string, string, string>, vector<Date>> defaultDates;
std::map<tuple<string, string, string, string, string, string>, vector<Real>> defaultValues;
std::map<tuple<string, string, string, string, string, string>, vector<Date>> closeOutDates;
std::map<tuple<string, string, string, string, string, string>, vector<Real>> closeOutValues;

};


BOOST_AUTO_TEST_CASE(NettedExposureCalculatorTest) {



    CachedResultsData cachedResults;
    std::map<tuple<string, string, string, string, string, string>, vector<Date>> cachedDefaultDates = cachedResults.defaultDates;
    std::map<tuple<string, string, string, string, string, string>, vector<Real>> cachedDefaultValues = cachedResults.defaultValues;
    std::map<tuple<string, string, string, string, string, string>, vector<Date>> cachedCloseOutDates = cachedResults.closeOutDates;
    std::map<tuple<string, string, string, string, string, string>, vector<Real>> cachedCloseOutValues = cachedResults.closeOutValues;
    

    BOOST_TEST_MESSAGE("Running NettedExposureCalculatorTestWithCloseOutGrid...");
    Date referenceDate = Date(14, April, 2016);
    BOOST_TEST_MESSAGE("Reference Date is "<<QuantLib::io::iso_date(referenceDate));
    Settings::instance().evaluationDate() = referenceDate;

    string dateGridStr;
    boost::shared_ptr<DateGrid> dateGrid;
    std::string nettingSetMpor = "1W";
    BOOST_TEST_MESSAGE("Neting-set mpor is "<< nettingSetMpor);
    vector<bool> withCloseOutGrid = {false, true}; 
    bool mporStickyDate = false;
    string mporModeStr = mporStickyDate ? "StickyDate": "ActualDate";
    bool withCompounding = false;
    string compoundingStr = withCompounding ? "withCompounding" : "woCompounding";
    string calcTypeStr;

    for(Size k = 0; k<withCloseOutGrid.size(); k++){

        string closeOutGridStr = withCloseOutGrid[k] ? "withCloseOutGrid": "woCloseOutGrid";
        std::vector<CollateralExposureHelper::CalculationType> calcTypes; 
        if (withCloseOutGrid[k]){
            dateGridStr = "13,1M";
            dateGrid = boost::make_shared<DateGrid>(dateGridStr);
            calcTypes= {CollateralExposureHelper::CalculationType::NoLag};
        }
        else{
            dateGridStr = "13,1W";
            dateGrid = boost::make_shared<DateGrid>(dateGridStr);
            calcTypes= {CollateralExposureHelper::CalculationType::Symmetric,
                        CollateralExposureHelper::CalculationType::AsymmetricCVA,
                        CollateralExposureHelper::CalculationType::AsymmetricDVA
                        };
        }
        Period Mpor(1, Weeks);
        if (withCloseOutGrid[k]){
            BOOST_TEST_MESSAGE("With close-out grid!");
            BOOST_TEST_MESSAGE("MPOR in close-out grid= "<< Mpor);
            dateGrid->addCloseOutDates(Mpor);
            if (mporStickyDate){
                BOOST_TEST_MESSAGE("With mpor mode sticky date!");
            }else
                BOOST_TEST_MESSAGE("With mpor mode actual date!");
        }else
            BOOST_TEST_MESSAGE("Without close-out grid!");

        TestData td(referenceDate, dateGrid, withCloseOutGrid[k], mporStickyDate);

        BOOST_TEST_MESSAGE("DateGrid: ");
        BOOST_TEST_MESSAGE("t_0, "<<io::iso_date(referenceDate));
        for(Size i=0; i<dateGrid->times().size(); i++)
            BOOST_TEST_MESSAGE("t_"<<i+1<<", "<<QuantLib::io::iso_date(dateGrid->dates()[i]));
        
        boost::shared_ptr<Market> initMarket = td.initMarket_;
        boost::shared_ptr<NPVCube> cube = td.cube_;
        boost::shared_ptr<Portfolio> portfolio = td.portfolio_;

        std::string nettingSetId = portfolio->trades().begin()->second->envelope().nettingSetId();
        NettingSetDetails nettingSetDetails(nettingSetId);

        if (withCloseOutGrid[k]){
            Period nettingSetMporPeriod = PeriodParser::parse(nettingSetMpor);
            QL_REQUIRE(nettingSetMporPeriod == Mpor, "Netting-set mpor is not consistent with the closeout grid!");
        }

        std::vector<std::string> elgColls = {"EUR"};
        boost::shared_ptr<NettingSetDefinition> nettingSetDefinition = boost::make_shared<NettingSetDefinition>(nettingSetDetails, 
                                                                                                                "Bilateral", "EUR", "EUR-EONIA", 
                                                                                                                0.0, 0.0, 0.0, 0.0, 0.0, 
                                                                                                                "FIXED", "1D", "1D",
                                                                                                                nettingSetMpor, 0.0, 0.0, elgColls);
        boost::shared_ptr<NettingSetManager> nettingSetManager = boost::make_shared<NettingSetManager>();
        nettingSetManager->add(nettingSetDefinition);

        map<string, vector<vector<Real>>> nettingSetDefaultValue;
        map<string, vector<vector<Real>>> nettingSetCloseOutValue;
        map<string, vector<vector<Real>>> nettingSetValue;
        vector<Real> collateralBalance;
        vector<vector<Real>> defaultValue;
        
        std::string fileName;
        boost::shared_ptr<AggregationScenarioData> asd;
        boost::shared_ptr<CubeInterpretation> cubeInterpreter;
        if (withCloseOutGrid[k]){
            fileName = "scenarioData_closeout.csv";
            asd = loadAggregationScenarioData(fileName);
            cubeInterpreter = boost::make_shared<MporGridCubeInterpretation>(asd, dateGrid);
        }else{ 
            fileName = "scenarioData.csv";
            asd = loadAggregationScenarioData(fileName);
            cubeInterpreter = boost::make_shared<RegularCubeInterpretation>(asd);
        }

        if (!withCompounding){
            compoundingStr = "woCompounding";
            for (Size i =0; i<cube->dates().size(); i++)
                asd->set(i, 0, 0, AggregationScenarioDataType::IndexFixing, "EUR-EONIA");
        }

        vector<string> regressors = {"EUR-EURIBOR-6M"};
        boost::shared_ptr<InputParameters> inputs = boost::make_shared<InputParameters>();
        boost::shared_ptr<RegressionDynamicInitialMarginCalculator> dimCalculator = boost::make_shared<RegressionDynamicInitialMarginCalculator>(inputs, portfolio, cube, cubeInterpreter, asd, 0.99, 14, 2, regressors);
        
        BOOST_TEST_MESSAGE("initial NPV at "<< QuantLib::io::iso_date(referenceDate)<<": "<<cube->getT0(0));
        for (Size i = 0; i<cube->dates().size(); i++)     
            BOOST_TEST_MESSAGE("defaultValue at "<< QuantLib::io::iso_date(dateGrid->valuationDates()[i])<<": "<<cubeInterpreter->getDefaultNpv(cube, 0, i, 0));      

        for (Size i = 0; i<cube->dates().size(); i++){
            if (withCloseOutGrid[k]){
                if (i != cube->dates().size()-1)
                    BOOST_TEST_MESSAGE("closeOutValue at "<< QuantLib::io::iso_date(dateGrid->closeOutDates()[i])<<": "<<cubeInterpreter->getCloseOutNpv(cube, 0, i, 0));
            }
            else{
                if (i != cube->dates().size()-1)
                    BOOST_TEST_MESSAGE("closeOutValue at "<< QuantLib::io::iso_date(dateGrid->valuationDates()[i])<<": "<<cubeInterpreter->getCloseOutNpv(cube, 0, i, 0));
            }
            
        }
    
        for(auto calcType : calcTypes){
            if (calcType == CollateralExposureHelper::Symmetric)
                calcTypeStr = "Symmetric";
            else if (calcType == CollateralExposureHelper::AsymmetricCVA)
                calcTypeStr = "AsymmetricCVA";
            else if (calcType == CollateralExposureHelper::AsymmetricDVA)
                calcTypeStr = "AsymmetricDVA";
            else if (calcType == CollateralExposureHelper::NoLag)
                calcTypeStr = "NoLag";
            else
                QL_FAIL("Collateral calculation type not covered");
            BOOST_TEST_MESSAGE("Calculation type: "<< calcTypeStr); 

            boost::shared_ptr<ExposureCalculator> exposureCalculator = boost::make_shared<ExposureCalculator>(portfolio, cube, cubeInterpreter,
                                                                    initMarket, false, "EUR", "Market",
                                                                    0.99, calcType, false, false);
            exposureCalculator->build();
            nettingSetDefaultValue = exposureCalculator->nettingSetDefaultValue();
            nettingSetCloseOutValue = exposureCalculator->nettingSetCloseOutValue();
            boost::shared_ptr<NettedExposureCalculator> nettedExposureCalculator = boost::make_shared<NettedExposureCalculator>(portfolio, initMarket, cube, "EUR", "Market",
                                                                                                                    0.99, calcType, false, nettingSetManager, nettingSetDefaultValue,
                                                                                                                    nettingSetCloseOutValue, asd, cubeInterpreter,
                                                                                                                    false, dimCalculator, false, false, 0.1, exposureCalculator->exposureCube(),
                                                                                                                    0, 0, false);
            nettedExposureCalculator->build();
            nettingSetValue = (calcType == CollateralExposureHelper::CalculationType::NoLag
                                ? nettedExposureCalculator->nettingSetCloseOutValue()
                                : nettedExposureCalculator->nettingSetDefaultValue());
            collateralBalance = nettedExposureCalculator->expectedCollateral(nettingSetId);
            BOOST_TEST_MESSAGE("defaultDate, defaultValue, closeOutDate, collateralBalance");
            auto key = make_tuple(dateGridStr, nettingSetMpor, closeOutGridStr, mporModeStr, calcTypeStr, compoundingStr); 

            vector<Date> cdd = cachedDefaultDates[key];
            vector<Real> cdv = cachedDefaultValues[key];
            vector<Date> ccd = cachedCloseOutDates[key];
            vector<Real> ccv = cachedCloseOutValues[key];
            BOOST_TEST_MESSAGE("cdd "<<cdd.size());
            BOOST_TEST_MESSAGE("cdv "<<cdv.size());
            BOOST_TEST_MESSAGE("ccd "<<ccd.size());
            BOOST_TEST_MESSAGE("ccv "<<ccv.size());
            Real tolerance = 0.000001;
            for (auto n : nettingSetValue){
                vector<vector<Real>> defaultValue = n.second;
                for (Size j = 0; j < cube->dates().size(); j++){
                    BOOST_TEST_MESSAGE(io::iso_date(dateGrid->valuationDates()[j])<<", "<<defaultValue[j][0] <<", "<< collateralBalance[j+1]);

                    BOOST_CHECK_MESSAGE(dateGrid->valuationDates()[j] == cdd[j], 
                                                "default date "<< dateGrid->valuationDates()[j] << " does not match with cashed default date "<< cdd[j]);
                    BOOST_CHECK_MESSAGE(fabs(defaultValue[j][0] - cdv[j]) < tolerance, 
                                                "default value "<< defaultValue[j][0] << " does not match with cashed default value "<< cdv[j]);  
                    BOOST_CHECK_MESSAGE(fabs(collateralBalance[j+1] - ccv[j]) < tolerance,
                                                "collateral balance "<< collateralBalance[j+1] << " does not match with cashed collateral balance "<< ccv[j]);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()