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

#include <test/swapperformance.hpp>
#include <test/testmarket.hpp>
#include <ored/utilities/osutils.hpp>
#include <ored/utilities/log.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <ored/model/lgmdata.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/model/crossassetmodelbuilder.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <ql/time/date.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <orea/engine/all.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/builders/swap.hpp>
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

// Returns an int in the interval [min, max]. Inclusive.
inline unsigned long randInt(MersenneTwisterUniformRng& rng, Size min, Size max) {
    return min + (rng.nextInt32() % (max + 1 - min));
}

inline const string& randString(MersenneTwisterUniformRng& rng, const vector<string>& strs) {
    return strs[randInt(rng, 0, strs.size() - 1)];
}

inline bool randBoolean(MersenneTwisterUniformRng& rng) { return randInt(rng, 0, 1) == 1; }

boost::shared_ptr<data::Conventions> convs() {
    boost::shared_ptr<data::Conventions> conventions(new data::Conventions());

    boost::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    boost::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    return conventions;
}

boost::shared_ptr<Portfolio> buildPortfolio(Size portfolioSize, boost::shared_ptr<EngineFactory>& factory) {

    boost::shared_ptr<Portfolio> portfolio(new Portfolio());

    vector<string> ccys = {"EUR", "USD", "GBP", "JPY", "CHF"};

    map<string, vector<string>> indices = {{"EUR", {"EUR-EURIBOR-6M"}},
                                           {"USD", {"USD-LIBOR-3M"}},
                                           {"GBP", {"GBP-LIBOR-6M"}},
                                           {"CHF", {"CHF-LIBOR-6M"}},
                                           {"JPY", {"JPY-LIBOR-6M"}}};

    vector<string> fixedTenors = {"6M", "1Y"};

    Size minTerm = 2;
    Size maxTerm = 30;

    Size minFixedBps = 10;
    Size maxFixedBps = 400;

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
        Size term = portfolioSize == 1 ? 20 : randInt(rng, minTerm, maxTerm);

        // Start today +/- 1 Year
        Date startDate = portfolioSize == 1 ? cal.adjust(today) : cal.adjust(today - 365 + randInt(rng, 0, 730));
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
        string ccy = portfolioSize == 1 ? "EUR" : randString(rng, ccys);
        string index = portfolioSize == 1 ? "EUR-EURIBOR-6M" : randString(rng, indices[ccy]);
        string floatFreq = portfolioSize == 1 ? "6M" : index.substr(index.find('-', 4) + 1);

        // fixed details
        string fixedTenor = portfolioSize == 1 ? "1Y" : randString(rng, fixedTenors);
        Real fixedRate = portfolioSize == 1 ? 0.02 : randInt(rng, minFixedBps, maxFixedBps) / 100.0;
        string fixFreq = portfolioSize == 1 ? "1Y" : randString(rng, fixedTenors);

        // envelope
        Envelope env("CP");

        // Schedules
        ScheduleData floatSchedule(ScheduleRules(start, end, floatFreq, calStr, conv, conv, rule));
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixFreq, calStr, conv, conv, rule));

        bool isPayer = randBoolean(rng);

        // fixed Leg - with dummy rate
        FixedLegData fixedLegData(vector<double>(1, fixedRate));
        LegData fixedLeg(isPayer, ccy, fixedLegData, fixedSchedule, fixDC, notional);

        // float Leg
        vector<double> spreads(1, 0);
        FloatingLegData floatingLegData(index, days, true, spread);
        LegData floatingLeg(!isPayer, ccy, floatingLegData, floatSchedule, floatDC, notional);

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
    DayCounter dc = ActualActual();
    map<string, Size> fixedFreqs;
    map<string, Size> floatFreqs;
    for (Size i = 0; i < portfolioSize; ++i) {
        boost::shared_ptr<Trade> trade = portfolio->trades()[i];
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
    BOOST_TEST_MESSAGE("Average Maturity  : " << maturity);
    ostringstream oss;
    for (Size i = 0; i < ccys.size(); i++)
        oss << ccys[i] << " ";
    BOOST_TEST_MESSAGE("Currencies        : " << oss.str());
    // dump % breakdown of tenors
    map<string, Size>::iterator it;
    BOOST_TEST_MESSAGE("Fixed Tenors      : ");
    for (it = fixedFreqs.begin(); it != fixedFreqs.end(); ++it) {
        Real perc = 100 * it->second / (Real)portfolioSize;
        BOOST_TEST_MESSAGE("  " << it->first << "  " << perc << " %");
    }
    BOOST_TEST_MESSAGE("Floating Tenors   : ");
    for (it = floatFreqs.begin(); it != floatFreqs.end(); ++it) {
        Real perc = 100 * it->second / (Real)portfolioSize;
        BOOST_TEST_MESSAGE("  " << it->first << "  " << perc << " %");
    }

    return portfolio;
}

void test_performance(Size portfolioSize) {
    BOOST_TEST_MESSAGE("Testing Swap Exposure Performance size=" << portfolioSize << "...");

    SavedSettings backup;

    // Log::instance().registerLogger(boost::make_shared<StderrLogger>());
    // Log::instance().switchOn();

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    string dateGridStr = "80,3M"; // 20 years
    boost::shared_ptr<DateGrid> dg = boost::make_shared<DateGrid>(dateGridStr);
    Size samples = 1000;

    BOOST_TEST_MESSAGE("Date Grid : " << dateGridStr);
    BOOST_TEST_MESSAGE("Samples   : " << samples);
    BOOST_TEST_MESSAGE("Swaps     : " << portfolioSize);

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
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> parameters(new analytics::ScenarioSimMarketParameters());
    parameters->baseCcy() = "EUR";
    parameters->ccys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    parameters->yieldCurveTenors() = {1 * Months, 6 * Months, 1 * Years, 2 * Years, 5 * Years, 10 * Years, 20 * Years};
    parameters->indices() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    parameters->interpolation() = "LogLinear";
    parameters->extrapolate() = true;

    parameters->simulateSwapVols() = false;
    parameters->swapVolTerms() = {6 * Months, 1 * Years};
    parameters->swapVolExpiries() = {1 * Years, 2 * Years};
    parameters->swapVolCcys() = ccys;
    parameters->swapVolDecayMode() = "ForwardVariance";

    parameters->fxVolExpiries() = {1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years};
    parameters->fxVolDecayMode() = "ConstantVariance";
    parameters->simulateFXVols() = false;

    parameters->fxVolCcyPairs() = {"USDEUR", "GBPEUR", "CHFEUR", "JPYEUR"};

    parameters->fxCcyPairs() = {"USDEUR", "GBPEUR", "CHFEUR", "JPYEUR"};

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
    vector<Real> hValues = {};
    vector<Real> aValues = {};

    std::vector<boost::shared_ptr<LgmData>> irConfigs;

    hValues = {0.02};
    aValues = {0.008};
    irConfigs.push_back(boost::make_shared<LgmData>(
        "EUR", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.03};
    aValues = {0.009};
    irConfigs.push_back(boost::make_shared<LgmData>(
        "USD", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(boost::make_shared<LgmData>(
        "GBP", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(boost::make_shared<LgmData>(
        "CHF", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(boost::make_shared<LgmData>(
        "JPY", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    // Compile FX configurations
    vector<string> optionExpiries = {"1Y", "2Y", "3Y", "5Y", "7Y", "10Y"};
    vector<string> optionStrikes(optionExpiries.size(), "ATMF");
    vector<Time> sigmaTimes = {};
    vector<Real> sigmaValues = {};

    std::vector<boost::shared_ptr<FxBsData>> fxConfigs;
    sigmaValues = {0.15};
    fxConfigs.push_back(boost::make_shared<FxBsData>("USD", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    sigmaValues = {0.20};
    fxConfigs.push_back(boost::make_shared<FxBsData>("GBP", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    sigmaValues = {0.20};
    fxConfigs.push_back(boost::make_shared<FxBsData>("CHF", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    sigmaValues = {0.20};
    fxConfigs.push_back(boost::make_shared<FxBsData>("JPY", "EUR", calibrationType, true, ParamType::Piecewise,
                                                     sigmaTimes, sigmaValues, optionExpiries, optionStrikes));

    std::map<std::pair<std::string, std::string>, Real> corr;
    corr[std::make_pair("IR:EUR", "IR:USD")] = 0.6;

    boost::shared_ptr<CrossAssetModelData> config(boost::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, corr));

    // Model Builder & Model
    // model builder
    boost::shared_ptr<CrossAssetModelBuilder> modelBuilder(new CrossAssetModelBuilder(initMarket));
    boost::shared_ptr<QuantExt::CrossAssetModel> model = modelBuilder->build(config);
    modelBuilder = NULL;

    // Path generator
    Size seed = 5;
    bool antithetic = false;
    boost::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGen =
        boost::make_shared<MultiPathGeneratorMersenneTwister>(model->stateProcess(), dg->timeGrid(), seed, antithetic);

    // build scenario generator
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    boost::shared_ptr<ScenarioGenerator> scenarioGenerator = boost::make_shared<CrossAssetModelScenarioGenerator>(
        model, pathGen, scenarioFactory, parameters, today, dg, initMarket);

    // build scenario sim market
    Conventions conventions = *convs();
    boost::shared_ptr<analytics::SimMarket> simMarket =
        boost::make_shared<analytics::ScenarioSimMarket>(scenarioGenerator, initMarket, parameters, conventions);

    // Build Porfolio
    boost::shared_ptr<EngineData> data = boost::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    boost::shared_ptr<EngineFactory> factory = boost::make_shared<EngineFactory>(data, simMarket);
    factory->registerBuilder(boost::make_shared<SwapEngineBuilder>());

    boost::shared_ptr<Portfolio> portfolio = buildPortfolio(portfolioSize, factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // Now calculate exposure
    ValuationEngine valEngine(today, dg, simMarket);

    // Calculate Cube
    boost::timer t;
    boost::shared_ptr<NPVCube> cube =
        boost::make_shared<DoublePrecisionInMemoryCube>(today, portfolio->ids(), dg->dates(), samples);
    vector<boost::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(boost::make_shared<NPVCalculator>(baseCcy));
    valEngine.buildCube(portfolio, cube, calculators);
    double elapsed = t.elapsed();

    BOOST_TEST_MESSAGE("Cube generated in " << elapsed << " seconds");

    Size dates = dg->dates().size();
    Size numNPVs = dates * samples * portfolioSize;
    BOOST_TEST_MESSAGE("Cube size = " << numNPVs << " elements");
    BOOST_TEST_MESSAGE("Cube elements theortical storage " << numNPVs * sizeof(Real) / (1024 * 1024) << " MB");
    Real pricingTimeMicroSeconds = elapsed * 1000000 / numNPVs;
    BOOST_TEST_MESSAGE("Avg Pricing time = " << pricingTimeMicroSeconds << " microseconds");

    // Count the number of times we have an empty line, ie. swap exipired.
    // cube is trades/dates/samples
    Size count = 0;
    for (Size i = 0; i < portfolioSize; ++i) {
        for (Size j = 0; j < dates; ++j) {
            if (cube->get(i, j, 0) == 0 && cube->get(i, j, 1) == 0 && cube->get(i, j, 2) == 0)
                ++count;
        }
    }
    Real nonZeroPerc = 100 * (1 - (count / ((Real)portfolioSize * dates)));
    BOOST_TEST_MESSAGE("Percentage of cube that is non-expired : " << nonZeroPerc << " %");
    BOOST_TEST_MESSAGE("Avg Pricing time (for non-expired trades) = " << pricingTimeMicroSeconds * 100 / nonZeroPerc
                                                                      << " microseconds");

    BOOST_TEST_MESSAGE(os::getSystemDetails());

    // Compute portfolo EPE and ENE
    for (Size i = 0; i < dates; ++i) {
        Real epe = 0.0, ene = 0.0;
        for (Size j = 0; j < samples; ++j) {
            Real npv = 0.0;
            for (Size k = 0; k < portfolioSize; ++k)
                npv += cube->get(k, i, j);
            epe += std::max(npv, 0.0);
            ene += std::max(-npv, 0.0);
        }
        epe /= samples;
        ene /= samples;
        LOG("Exposures, " << i << " " << epe << " " << ene);
    }
}

void SwapPerformanceTest::testSwapPerformance() { test_performance(100); }

void SwapPerformanceTest::testSingleSwapPerformance() { test_performance(1); }

test_suite* SwapPerformanceTest::suite() {
    // Uncomment the below to get detailed output TODO: custom logger that uses BOOST_MESSAGE

  /*
    boost::shared_ptr<ore::data::FileLogger> logger
        = boost::make_shared<ore::data::FileLogger>("swapperformace_test.log");
    ore::data::Log::instance().removeAllLoggers();
    ore::data::Log::instance().registerLogger(logger);
    ore::data::Log::instance().switchOn();
    ore::data::Log::instance().setMask(255);
  */

    test_suite* suite = BOOST_TEST_SUITE("SwapPerformanceTest");
    // Set the Observation mode here
    ObservationMode::instance().setMode(ObservationMode::Mode::Unregister);
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSingleSwapPerformance));
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSwapPerformance));
    return suite;
}
}
