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
        FloatingLegData floatingLegData(index, days, false, spread);
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

struct SwapResults {
    vector<Real> epe;
    vector<Real> ene;
    Time completionTime;
    string processMemory;
    Real nonZeroPVRatio;
};

SwapResults test_performance(Size portfolioSize, ObservationMode::Mode om) {
    BOOST_TEST_MESSAGE("Testing Swap Exposure Performance size=" << portfolioSize << "...");

    SavedSettings backup;
    ObservationMode::Mode backupOm = ObservationMode::instance().mode();
    ObservationMode::instance().setMode(om);

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

    parameters->eqVolExpiries() = {1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years};
    parameters->eqVolDecayMode() = "ConstantVariance";
    parameters->simulateEQVols() = false;

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

    // BOOST_TEST_MESSAGE(os::getSystemDetails());

    // Compute portfolo EPE and ENE
    vector<Real> eeVec, eneVec;
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
        // BOOST_TEST_MESSAGE("Exposures, " << i << " " << epe << " " << ene);
        eeVec.push_back(epe);
        eneVec.push_back(ene);
    }
    SwapResults res;
    res.epe = eeVec;
    res.ene = eneVec;
    res.completionTime = elapsed;
    res.processMemory = os::getMemoryUsage();
    res.nonZeroPVRatio = nonZeroPerc;
    ObservationMode::instance().setMode(backupOm);
    IndexManager::instance().clearHistories();
    return res;
}

void SwapPerformanceTest::testSwapPerformanceNoneObs() {
    BOOST_TEST_MESSAGE("Testing Swap Performance (None observation mode)");
    // Set the Observation mode here
    ObservationMode::Mode om = ObservationMode::Mode::None;
    SwapResults res = test_performance(100, om);
    BOOST_CHECK_CLOSE(70.5875, res.nonZeroPVRatio, 0.005);
    vector<Real> epe_archived = {
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       2045.51,    13666.80,   16321.70,   12121.60,  23261.70,
        54494.30,  68155.30,  68489.40,   83575.70,   118253.00,  123282.00,  118928.00,  149838.00, 218266.00,
        276536.00, 245307.00, 278034.00,  368274.00,  447787.00,  407436.00,  449380.00,  683155.00, 760313.00,
        764175.00, 832288.00, 1313320.00, 1383550.00, 1413170.00, 1500990.00, 2134060.00, 2323430.00};
    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.epe[i], epe_archived[i], 0.01);
    }
    vector<Real> ene_archived = {
        368484000.00, 366973000.00, 359704000.00, 367637000.00, 360213000.00, 360121000.00, 333664000.00, 333002000.00,
        326143000.00, 325711000.00, 300047000.00, 299732000.00, 292758000.00, 293598000.00, 268908000.00, 270144000.00,
        264655000.00, 265927000.00, 246301000.00, 245928000.00, 240306000.00, 240397000.00, 220011000.00, 220454000.00,
        213033000.00, 214004000.00, 194180000.00, 194297000.00, 187696000.00, 187765000.00, 169167000.00, 169970000.00,
        163421000.00, 164495000.00, 147713000.00, 149211000.00, 142856000.00, 144372000.00, 132099000.00, 137104000.00,
        130605000.00, 130117000.00, 117862000.00, 120095000.00, 113333000.00, 112971000.00, 100684000.00, 104869000.00,
        97805700.00,  97476100.00,  86411500.00,  90553700.00,  84189300.00,  85294200.00,  74576700.00,  80118600.00,
        73433300.00,  76594300.00,  68622100.00,  73311100.00,  64758500.00,  68073500.00,  60952400.00,  63464700.00,
        55889700.00,  59212900.00,  52316800.00,  55641700.00,  47762400.00,  51099800.00,  44301900.00,  47286400.00,
        39632700.00,  42983300.00,  36719900.00,  39755700.00,  32314400.00,  36128700.00,  30183700.00,  33114200.00};
    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.ene[i], ene_archived[i], 0.01);
    }
}

void SwapPerformanceTest::testSingleSwapPerformanceNoneObs() {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (None observation mode)");
    // Set the Observation mode here
    ObservationMode::Mode om = ObservationMode::Mode::None;
    SwapResults res = test_performance(1, om);
    BOOST_CHECK_CLOSE(98.75, res.nonZeroPVRatio, 0.005);
    vector<Real> epe_archived = {
        8422.46, 11198.2, 15556.5, 22180.9, 24515.3, 22731,   24475.8, 30631.9, 32462.8, 28579.6, 29796.7, 34820.7,
        35792,   31444.1, 31421.2, 35378.4, 36713.7, 32176.1, 33109.1, 36913.6, 38421.2, 33315.4, 33985.7, 37880,
        39303,   34201.6, 34475.6, 37838.7, 38555.7, 33052.7, 34178.1, 37796.7, 38291.9, 33090.1, 33801.9, 37407.6,
        37883.1, 32242.3, 32895,   35663,   36199.8, 30599.1, 31125.5, 33598.4, 33774.4, 27907.8, 28320.8, 30594,
        30704.2, 24996.1, 25219.7, 27475.7, 27991.9, 22261.5, 22503.8, 24273.2, 24606.1, 19183.9, 19377.6, 21039.9,
        21286.3, 15787,   15905.7, 17288.3, 17438.7, 11921.7, 12042,   13379.8, 13566.1, 8142.94, 8244.72, 9312.21,
        9336.38, 4025.76, 4011.88, 4742.72, 4713.73, 387.128, 386.435, 0};
    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.epe[i], epe_archived[i], 0.01);
    }
    vector<Real> ene_archived = {
        15210.5, 23791.6, 25713.8, 21832.1, 24448.6, 30275.1, 31685,   28403.2, 30147.2, 35385.4, 36347.8, 31485.9,
        32451.9, 37585.8, 39032.5, 34615.3, 35484.1, 40387.2, 40795.7, 35499.5, 37112.2, 40725.5, 41757.1, 36557.2,
        37031.2, 40500.6, 41313.8, 36469.9, 37160,   39547.7, 39861.8, 33664.4, 34444.1, 37206.2, 37685.7, 32158.3,
        32322.6, 34521.1, 35196.8, 30809.1, 31218.9, 33447.3, 34164.3, 28843.1, 29113.1, 32074.3, 32535.5, 28092.8,
        27974,   30230.6, 30331.7, 25129.4, 25443.9, 27195.7, 27727.1, 22540.7, 22623.7, 24868.9, 25036.2, 19195.5,
        19036.4, 21082.1, 21592.6, 15735.2, 15809.7, 17752,   17959.2, 12408.7, 12506.8, 13937.7, 14004,   8403.87,
        8375.64, 10190.8, 10229.4, 4311.72, 4277.53, 6773.5,  6779.32, 0};
    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.ene[i], ene_archived[i], 0.01);
    }
}

void SwapPerformanceTest::testSwapPerformanceDisableObs() {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Disable observation mode)");
    // Set the Observation mode here
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    SwapResults res = test_performance(100, om);
    BOOST_CHECK_CLOSE(70.5875, res.nonZeroPVRatio, 0.005);
    vector<Real> epe_archived = {
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       2045.51,    13666.80,   16321.70,   12121.60,  23261.70,
        54494.30,  68155.30,  68489.40,   83575.70,   118253.00,  123282.00,  118928.00,  149838.00, 218266.00,
        276536.00, 245307.00, 278034.00,  368274.00,  447787.00,  407436.00,  449380.00,  683155.00, 760313.00,
        764175.00, 832288.00, 1313320.00, 1383550.00, 1413170.00, 1500990.00, 2134060.00, 2323430.00};
    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.epe[i], epe_archived[i], 0.01);
    }
    vector<Real> ene_archived = {
        368484000.00, 366973000.00, 359704000.00, 367637000.00, 360213000.00, 360121000.00, 333664000.00, 333002000.00,
        326143000.00, 325711000.00, 300047000.00, 299732000.00, 292758000.00, 293598000.00, 268908000.00, 270144000.00,
        264655000.00, 265927000.00, 246301000.00, 245928000.00, 240306000.00, 240397000.00, 220011000.00, 220454000.00,
        213033000.00, 214004000.00, 194180000.00, 194297000.00, 187696000.00, 187765000.00, 169167000.00, 169970000.00,
        163421000.00, 164495000.00, 147713000.00, 149211000.00, 142856000.00, 144372000.00, 132099000.00, 137104000.00,
        130605000.00, 130117000.00, 117862000.00, 120095000.00, 113333000.00, 112971000.00, 100684000.00, 104869000.00,
        97805700.00,  97476100.00,  86411500.00,  90553700.00,  84189300.00,  85294200.00,  74576700.00,  80118600.00,
        73433300.00,  76594300.00,  68622100.00,  73311100.00,  64758500.00,  68073500.00,  60952400.00,  63464700.00,
        55889700.00,  59212900.00,  52316800.00,  55641700.00,  47762400.00,  51099800.00,  44301900.00,  47286400.00,
        39632700.00,  42983300.00,  36719900.00,  39755700.00,  32314400.00,  36128700.00,  30183700.00,  33114200.00};
    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.ene[i], ene_archived[i], 0.01);
    }
}

void SwapPerformanceTest::testSingleSwapPerformanceDisableObs() {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Disable observation mode)");
    // Set the Observation mode here
    ObservationMode::Mode om = ObservationMode::Mode::Disable;
    SwapResults res = test_performance(1, om);
    BOOST_CHECK_CLOSE(98.75, res.nonZeroPVRatio, 0.005);
    vector<Real> epe_archived = {
        8422.46, 11198.2, 15556.5, 22180.9, 24515.3, 22731,   24475.8, 30631.9, 32462.8, 28579.6, 29796.7, 34820.7,
        35792,   31444.1, 31421.2, 35378.4, 36713.7, 32176.1, 33109.1, 36913.6, 38421.2, 33315.4, 33985.7, 37880,
        39303,   34201.6, 34475.6, 37838.7, 38555.7, 33052.7, 34178.1, 37796.7, 38291.9, 33090.1, 33801.9, 37407.6,
        37883.1, 32242.3, 32895,   35663,   36199.8, 30599.1, 31125.5, 33598.4, 33774.4, 27907.8, 28320.8, 30594,
        30704.2, 24996.1, 25219.7, 27475.7, 27991.9, 22261.5, 22503.8, 24273.2, 24606.1, 19183.9, 19377.6, 21039.9,
        21286.3, 15787,   15905.7, 17288.3, 17438.7, 11921.7, 12042,   13379.8, 13566.1, 8142.94, 8244.72, 9312.21,
        9336.38, 4025.76, 4011.88, 4742.72, 4713.73, 387.128, 386.435, 0};
    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.epe[i], epe_archived[i], 0.01);
    }
    vector<Real> ene_archived = {
        15210.5, 23791.6, 25713.8, 21832.1, 24448.6, 30275.1, 31685,   28403.2, 30147.2, 35385.4, 36347.8, 31485.9,
        32451.9, 37585.8, 39032.5, 34615.3, 35484.1, 40387.2, 40795.7, 35499.5, 37112.2, 40725.5, 41757.1, 36557.2,
        37031.2, 40500.6, 41313.8, 36469.9, 37160,   39547.7, 39861.8, 33664.4, 34444.1, 37206.2, 37685.7, 32158.3,
        32322.6, 34521.1, 35196.8, 30809.1, 31218.9, 33447.3, 34164.3, 28843.1, 29113.1, 32074.3, 32535.5, 28092.8,
        27974,   30230.6, 30331.7, 25129.4, 25443.9, 27195.7, 27727.1, 22540.7, 22623.7, 24868.9, 25036.2, 19195.5,
        19036.4, 21082.1, 21592.6, 15735.2, 15809.7, 17752,   17959.2, 12408.7, 12506.8, 13937.7, 14004,   8403.87,
        8375.64, 10190.8, 10229.4, 4311.72, 4277.53, 6773.5,  6779.32, 0};
    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.ene[i], ene_archived[i], 0.01);
    }
}

void SwapPerformanceTest::testSwapPerformanceDeferObs() {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Defer observation mode)");
    // Set the Observation mode here
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    SwapResults res = test_performance(100, om);
    BOOST_CHECK_CLOSE(70.5875, res.nonZeroPVRatio, 0.005);
    vector<Real> epe_archived = {
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       2045.51,    13666.80,   16321.70,   12121.60,  23261.70,
        54494.30,  68155.30,  68489.40,   83575.70,   118253.00,  123282.00,  118928.00,  149838.00, 218266.00,
        276536.00, 245307.00, 278034.00,  368274.00,  447787.00,  407436.00,  449380.00,  683155.00, 760313.00,
        764175.00, 832288.00, 1313320.00, 1383550.00, 1413170.00, 1500990.00, 2134060.00, 2323430.00};
    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.epe[i], epe_archived[i], 0.01);
    }
    vector<Real> ene_archived = {
        368484000.00, 366973000.00, 359704000.00, 367637000.00, 360213000.00, 360121000.00, 333664000.00, 333002000.00,
        326143000.00, 325711000.00, 300047000.00, 299732000.00, 292758000.00, 293598000.00, 268908000.00, 270144000.00,
        264655000.00, 265927000.00, 246301000.00, 245928000.00, 240306000.00, 240397000.00, 220011000.00, 220454000.00,
        213033000.00, 214004000.00, 194180000.00, 194297000.00, 187696000.00, 187765000.00, 169167000.00, 169970000.00,
        163421000.00, 164495000.00, 147713000.00, 149211000.00, 142856000.00, 144372000.00, 132099000.00, 137104000.00,
        130605000.00, 130117000.00, 117862000.00, 120095000.00, 113333000.00, 112971000.00, 100684000.00, 104869000.00,
        97805700.00,  97476100.00,  86411500.00,  90553700.00,  84189300.00,  85294200.00,  74576700.00,  80118600.00,
        73433300.00,  76594300.00,  68622100.00,  73311100.00,  64758500.00,  68073500.00,  60952400.00,  63464700.00,
        55889700.00,  59212900.00,  52316800.00,  55641700.00,  47762400.00,  51099800.00,  44301900.00,  47286400.00,
        39632700.00,  42983300.00,  36719900.00,  39755700.00,  32314400.00,  36128700.00,  30183700.00,  33114200.00};
    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.ene[i], ene_archived[i], 0.01);
    }
}

void SwapPerformanceTest::testSingleSwapPerformanceDeferObs() {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Defer observation mode)");
    // Set the Observation mode here
    ObservationMode::Mode om = ObservationMode::Mode::Defer;
    SwapResults res = test_performance(1, om);
    BOOST_CHECK_CLOSE(98.75, res.nonZeroPVRatio, 0.005);
    vector<Real> epe_archived = {
        8422.46, 11198.2, 15556.5, 22180.9, 24515.3, 22731,   24475.8, 30631.9, 32462.8, 28579.6, 29796.7, 34820.7,
        35792,   31444.1, 31421.2, 35378.4, 36713.7, 32176.1, 33109.1, 36913.6, 38421.2, 33315.4, 33985.7, 37880,
        39303,   34201.6, 34475.6, 37838.7, 38555.7, 33052.7, 34178.1, 37796.7, 38291.9, 33090.1, 33801.9, 37407.6,
        37883.1, 32242.3, 32895,   35663,   36199.8, 30599.1, 31125.5, 33598.4, 33774.4, 27907.8, 28320.8, 30594,
        30704.2, 24996.1, 25219.7, 27475.7, 27991.9, 22261.5, 22503.8, 24273.2, 24606.1, 19183.9, 19377.6, 21039.9,
        21286.3, 15787,   15905.7, 17288.3, 17438.7, 11921.7, 12042,   13379.8, 13566.1, 8142.94, 8244.72, 9312.21,
        9336.38, 4025.76, 4011.88, 4742.72, 4713.73, 387.128, 386.435, 0};
    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.epe[i], epe_archived[i], 0.01);
    }
    vector<Real> ene_archived = {
        15210.5, 23791.6, 25713.8, 21832.1, 24448.6, 30275.1, 31685,   28403.2, 30147.2, 35385.4, 36347.8, 31485.9,
        32451.9, 37585.8, 39032.5, 34615.3, 35484.1, 40387.2, 40795.7, 35499.5, 37112.2, 40725.5, 41757.1, 36557.2,
        37031.2, 40500.6, 41313.8, 36469.9, 37160,   39547.7, 39861.8, 33664.4, 34444.1, 37206.2, 37685.7, 32158.3,
        32322.6, 34521.1, 35196.8, 30809.1, 31218.9, 33447.3, 34164.3, 28843.1, 29113.1, 32074.3, 32535.5, 28092.8,
        27974,   30230.6, 30331.7, 25129.4, 25443.9, 27195.7, 27727.1, 22540.7, 22623.7, 24868.9, 25036.2, 19195.5,
        19036.4, 21082.1, 21592.6, 15735.2, 15809.7, 17752,   17959.2, 12408.7, 12506.8, 13937.7, 14004,   8403.87,
        8375.64, 10190.8, 10229.4, 4311.72, 4277.53, 6773.5,  6779.32, 0};
    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.ene[i], ene_archived[i], 0.01);
    }
}

void SwapPerformanceTest::testSwapPerformanceUnregisterObs() {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Unregister observation mode)");
    // Set the Observation mode here
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    SwapResults res = test_performance(100, om);
    BOOST_CHECK_CLOSE(70.5875, res.nonZeroPVRatio, 0.005);
    vector<Real> epe_archived = {
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,      0.00,      0.00,       0.00,       2045.51,    13666.80,   16321.70,   12121.60,  23261.70,
        54494.30,  68155.30,  68489.40,   83575.70,   118253.00,  123282.00,  118928.00,  149838.00, 218266.00,
        276536.00, 245307.00, 278034.00,  368274.00,  447787.00,  407436.00,  449380.00,  683155.00, 760313.00,
        764175.00, 832288.00, 1313320.00, 1383550.00, 1413170.00, 1500990.00, 2134060.00, 2323430.00};
    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.epe[i], epe_archived[i], 0.01);
    }
    vector<Real> ene_archived = {
        368484000.00, 366973000.00, 359704000.00, 367637000.00, 360213000.00, 360121000.00, 333664000.00, 333002000.00,
        326143000.00, 325711000.00, 300047000.00, 299732000.00, 292758000.00, 293598000.00, 268908000.00, 270144000.00,
        264655000.00, 265927000.00, 246301000.00, 245928000.00, 240306000.00, 240397000.00, 220011000.00, 220454000.00,
        213033000.00, 214004000.00, 194180000.00, 194297000.00, 187696000.00, 187765000.00, 169167000.00, 169970000.00,
        163421000.00, 164495000.00, 147713000.00, 149211000.00, 142856000.00, 144372000.00, 132099000.00, 137104000.00,
        130605000.00, 130117000.00, 117862000.00, 120095000.00, 113333000.00, 112971000.00, 100684000.00, 104869000.00,
        97805700.00,  97476100.00,  86411500.00,  90553700.00,  84189300.00,  85294200.00,  74576700.00,  80118600.00,
        73433300.00,  76594300.00,  68622100.00,  73311100.00,  64758500.00,  68073500.00,  60952400.00,  63464700.00,
        55889700.00,  59212900.00,  52316800.00,  55641700.00,  47762400.00,  51099800.00,  44301900.00,  47286400.00,
        39632700.00,  42983300.00,  36719900.00,  39755700.00,  32314400.00,  36128700.00,  30183700.00,  33114200.00};
    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.ene[i], ene_archived[i], 0.01);
    }
}

void SwapPerformanceTest::testSingleSwapPerformanceUnregisterObs() {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Unregister observation mode)");
    // Set the Observation mode here
    ObservationMode::Mode om = ObservationMode::Mode::Unregister;
    SwapResults res = test_performance(1, om);
    BOOST_CHECK_CLOSE(98.75, res.nonZeroPVRatio, 0.005);
    vector<Real> epe_archived = {
        8422.46, 11198.2, 15556.5, 22180.9, 24515.3, 22731,   24475.8, 30631.9, 32462.8, 28579.6, 29796.7, 34820.7,
        35792,   31444.1, 31421.2, 35378.4, 36713.7, 32176.1, 33109.1, 36913.6, 38421.2, 33315.4, 33985.7, 37880,
        39303,   34201.6, 34475.6, 37838.7, 38555.7, 33052.7, 34178.1, 37796.7, 38291.9, 33090.1, 33801.9, 37407.6,
        37883.1, 32242.3, 32895,   35663,   36199.8, 30599.1, 31125.5, 33598.4, 33774.4, 27907.8, 28320.8, 30594,
        30704.2, 24996.1, 25219.7, 27475.7, 27991.9, 22261.5, 22503.8, 24273.2, 24606.1, 19183.9, 19377.6, 21039.9,
        21286.3, 15787,   15905.7, 17288.3, 17438.7, 11921.7, 12042,   13379.8, 13566.1, 8142.94, 8244.72, 9312.21,
        9336.38, 4025.76, 4011.88, 4742.72, 4713.73, 387.128, 386.435, 0};
    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.epe[i], epe_archived[i], 0.01);
    }
    vector<Real> ene_archived = {
        15210.5, 23791.6, 25713.8, 21832.1, 24448.6, 30275.1, 31685,   28403.2, 30147.2, 35385.4, 36347.8, 31485.9,
        32451.9, 37585.8, 39032.5, 34615.3, 35484.1, 40387.2, 40795.7, 35499.5, 37112.2, 40725.5, 41757.1, 36557.2,
        37031.2, 40500.6, 41313.8, 36469.9, 37160,   39547.7, 39861.8, 33664.4, 34444.1, 37206.2, 37685.7, 32158.3,
        32322.6, 34521.1, 35196.8, 30809.1, 31218.9, 33447.3, 34164.3, 28843.1, 29113.1, 32074.3, 32535.5, 28092.8,
        27974,   30230.6, 30331.7, 25129.4, 25443.9, 27195.7, 27727.1, 22540.7, 22623.7, 24868.9, 25036.2, 19195.5,
        19036.4, 21082.1, 21592.6, 15735.2, 15809.7, 17752,   17959.2, 12408.7, 12506.8, 13937.7, 14004,   8403.87,
        8375.64, 10190.8, 10229.4, 4311.72, 4277.53, 6773.5,  6779.32, 0};
    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(res.ene[i], ene_archived[i], 0.01);
    }
}

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
    // (2017-03-28) The single swap performance tests should each complete within 20-25 seconds
    // The portfolio swap performance tests should completed within 400-700 seconds
    // Unregister and Disable should be faster than None and Defer
    // The actual run-times are not checked with any unit test assertions
    // This is because the actual run-times may vary depending upon the machine specs and other competing processes
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSingleSwapPerformanceNoneObs));
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSingleSwapPerformanceDisableObs));
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSingleSwapPerformanceDeferObs));
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSingleSwapPerformanceUnregisterObs));
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSwapPerformanceNoneObs));
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSwapPerformanceDisableObs));
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSwapPerformanceDeferObs));
    suite->add(BOOST_TEST_CASE(&SwapPerformanceTest::testSwapPerformanceUnregisterObs));
    return suite;
}
}
