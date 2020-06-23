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

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

using testsuite::TestMarket;

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsPerformanceTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SwapPerformaceTest, *boost::unit_test::disabled())

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

        // This variable is not used and only here to ensure that the random numbers generated in
        // subsequent blocks produce a swap portfolio, which is compatible with the archived values.
        string fixedTenor = portfolioSize == 1 ? "1Y" : randString(rng, fixedTenors);
        fixedTenor = fixedTenor + "_";

        // fixed details
        Real fixedRate = portfolioSize == 1 ? 0.02 : randInt(rng, minFixedBps, maxFixedBps) / 100.0;
        string fixFreq = portfolioSize == 1 ? "1Y" : randString(rng, fixedTenors);

        // envelope
        Envelope env("CP");

        // Schedules
        ScheduleData floatSchedule(ScheduleRules(start, end, floatFreq, calStr, conv, conv, rule));
        ScheduleData fixedSchedule(ScheduleRules(start, end, fixFreq, calStr, conv, conv, rule));

        bool isPayer = randBoolean(rng);

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

void test_performance(Size portfolioSize, ObservationMode::Mode om, double nonZeroPVRatio, vector<Real>& epe_archived,
                      vector<Real>& ene_archived) {
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
    parameters->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    parameters->setYieldCurveTenors("",
                                    {1 * Months, 6 * Months, 1 * Years, 2 * Years, 5 * Years, 10 * Years, 20 * Years});
    parameters->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});
    parameters->setYieldCurveDayCounters("", "ACT/ACT");

    parameters->interpolation() = "LogLinear";
    parameters->extrapolate() = true;

    parameters->setSimulateSwapVols(false);
    parameters->setSwapVolTerms("", {6 * Months, 1 * Years});
    parameters->setSwapVolExpiries("", {1 * Years, 2 * Years});
    parameters->swapVolCcys() = ccys;
    parameters->swapVolDecayMode() = "ForwardVariance";
    parameters->setSwapVolDayCounters("", "ACT/ACT");

    parameters->setFxVolExpiries(
        vector<Period>{1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    parameters->setFxVolDecayMode(string("ConstantVariance"));
    parameters->setFxVolDayCounters("", "ACT/ACT");
    parameters->setSimulateFXVols(false);

    parameters->setFxVolCcyPairs({"USDEUR", "GBPEUR", "CHFEUR", "JPYEUR"});
    parameters->setFxCcyPairs({"USDEUR", "GBPEUR", "CHFEUR", "JPYEUR"});

    parameters->equityVolExpiries() = {1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years};
    parameters->equityVolDecayMode() = "ConstantVariance";
    parameters->setEquityVolDayCounters("", "ACT/ACT");
    parameters->setSimulateEquityVols(false);

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

    std::vector<boost::shared_ptr<IrLgmData>> irConfigs;

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

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(boost::make_shared<IrLgmData>(
        "GBP", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(boost::make_shared<IrLgmData>(
        "CHF", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    hValues = {0.04};
    aValues = {0.01};
    irConfigs.push_back(boost::make_shared<IrLgmData>(
        "JPY", calibrationType, revType, volType, false, ParamType::Constant, hTimes, hValues, true,
        ParamType::Piecewise, aTimes, aValues, 0.0, 1.0, swaptionExpiries, swaptionTerms, swaptionStrikes));

    // Compile FX configurations
    vector<string> optionExpiries = {"1Y", "2Y", "3Y", "5Y", "7Y", "10Y"};
    vector<string> optionStrikes(optionExpiries.size(), "ATMF");
    vector<Time> sigmaTimes = {};

    std::vector<boost::shared_ptr<FxBsData>> fxConfigs;

    vector<Real> sigmaValues = {0.15};
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

    std::map<std::pair<std::string, std::string>, Handle<Quote>> corr;
    corr[std::make_pair("IR:EUR", "IR:USD")] = Handle<Quote>(boost::make_shared<SimpleQuote>(0.6));

    boost::shared_ptr<CrossAssetModelData> config(boost::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, corr));

    // Model Builder & Model
    // model builder
    boost::shared_ptr<CrossAssetModelBuilder> modelBuilder(new CrossAssetModelBuilder(initMarket, config));
    boost::shared_ptr<QuantExt::CrossAssetModel> model = *modelBuilder->model();
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
    auto simMarket = boost::make_shared<analytics::ScenarioSimMarket>(initMarket, parameters, conventions);
    simMarket->scenarioGenerator() = scenarioGenerator;

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
    boost::timer::cpu_timer t;
    boost::shared_ptr<NPVCube> cube =
        boost::make_shared<DoublePrecisionInMemoryCube>(today, portfolio->ids(), dg->dates(), samples);
    vector<boost::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(boost::make_shared<NPVCalculator>(baseCcy));
    valEngine.buildCube(portfolio, cube, calculators);
    t.stop();
    double elapsed = t.elapsed().wall * 1e-9;

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

    ObservationMode::instance().setMode(backupOm);
    IndexManager::instance().clearHistories();

    // check results
    BOOST_CHECK_CLOSE(nonZeroPVRatio, nonZeroPerc, 0.005);

    for (Size i = 0; i < epe_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(eeVec[i], epe_archived[i], 0.01);
    }

    for (Size i = 0; i < ene_archived.size(); ++i) {
        BOOST_CHECK_CLOSE(eneVec[i], ene_archived[i], 0.01);
    }
}

BOOST_AUTO_TEST_CASE(testSwapPerformanceNoneObs) {
    BOOST_TEST_MESSAGE("Testing Swap Performance (None observation mode)");
    vector<Real> epe_archived = {
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       212.27,     0.00,       7323.88,    31533.28,   53378.95,   36207.00,  46906.01,
        104095.83,  135746.68,  125782.33,  140930.67,  182959.31,  194273.49,  189484.96,  243642.11, 322150.25,
        399831.48,  369521.42,  439131.00,  551140.41,  674989.42,  635496.50,  683438.19,  965512.49, 1089189.37,
        1120743.38, 1215602.57, 1746485.50, 1911132.65, 1937517.65, 2018342.66, 2749016.80, 2983288.29};
    vector<Real> ene_archived = {
        368479333.37, 366947138.93, 359696820.06, 367678966.20, 360306476.22, 360214990.08, 333790252.36, 333074799.18,
        326284318.93, 325852335.02, 300193565.70, 299829910.92, 292796703.76, 293767241.06, 269068886.49, 270243130.10,
        264709179.78, 266009430.50, 246284950.17, 245916331.08, 240391023.58, 240418624.73, 219999845.91, 220436827.62,
        213036646.42, 213985864.64, 194194752.86, 194421068.57, 187827555.98, 187926831.63, 169284588.22, 170055369.68,
        163559634.19, 164558813.48, 147752949.47, 149093664.98, 142730143.65, 144246717.03, 132026539.31, 137083359.41,
        130631368.01, 130187937.69, 117919243.84, 120129844.58, 113388318.81, 113037081.05, 100703902.26, 104901313.87,
        97792109.49,  97449264.40,  86410086.30,  90522266.38,  84155342.70,  85251403.29,  74564195.80,  80107600.03,
        73456108.79,  76608529.93,  68627084.36,  73334428.12,  64852662.37,  68227498.40,  61045476.14,  63509132.12,
        55901466.52,  59264990.10,  52421890.10,  55762525.83,  47861919.43,  51177217.97,  44467494.67,  47470977.67,
        39894747.69,  43187901.39,  37014398.67,  40137889.11,  32665143.70,  36437914.59,  30581997.34,  33515698.54};

    test_performance(100, ObservationMode::Mode::None, 70.5875, epe_archived, ene_archived);
}

BOOST_AUTO_TEST_CASE(testSingleSwapPerformanceNoneObs) {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (None observation mode)");
    vector<Real> epe_archived = {
        8422.46,  11198.17, 15556.45, 22180.89, 24515.35, 22731.01, 24475.81, 30631.91, 32462.83, 28579.61,
        29796.73, 34820.71, 35791.98, 31444.10, 31421.22, 35378.44, 36713.70, 32176.09, 33109.12, 36913.64,
        38421.16, 33315.37, 33985.69, 37880.01, 39303.00, 34201.58, 34475.59, 37838.68, 38555.66, 33052.73,
        34178.13, 37796.68, 38291.89, 33090.13, 33801.90, 37407.56, 37883.08, 32242.32, 32894.99, 35663.04,
        36199.80, 30599.08, 31125.50, 33598.36, 33774.43, 27907.84, 28320.77, 30593.98, 30704.24, 24996.15,
        25219.71, 27475.72, 27991.94, 22261.52, 22503.79, 24273.17, 24606.12, 19183.91, 19377.61, 21039.90,
        21286.30, 15787.01, 15905.70, 17288.35, 17438.69, 11921.71, 12042.01, 13379.75, 13566.09, 8142.94,
        8244.72,  9312.21,  9336.38,  4025.76,  4011.88,  4742.72,  4713.73,  387.13,   386.43,   0.00};
    vector<Real> ene_archived = {
        15210.48, 23791.57, 25713.78, 21832.12, 24448.58, 30275.09, 31684.96, 28403.17, 30147.18, 35385.39,
        36347.77, 31485.92, 32451.88, 37585.83, 39032.52, 34615.34, 35484.08, 40387.23, 40795.70, 35499.49,
        37112.20, 40725.49, 41757.08, 36557.20, 37031.21, 40500.58, 41313.81, 36469.87, 37160.04, 39547.69,
        39861.84, 33664.42, 34444.08, 37206.24, 37685.74, 32158.34, 32322.59, 34521.05, 35196.78, 30809.12,
        31218.90, 33447.32, 34164.27, 28843.13, 29113.11, 32074.28, 32535.47, 28092.79, 27973.99, 30230.57,
        30331.73, 25129.41, 25443.92, 27195.73, 27727.14, 22540.71, 22623.75, 24868.88, 25036.23, 19195.48,
        19036.40, 21082.11, 21592.57, 15735.17, 15809.72, 17752.04, 17959.20, 12408.72, 12506.82, 13937.71,
        14004.02, 8403.87,  8375.64,  10190.85, 10229.44, 4311.72,  4277.53,  6773.50,  6779.32,  0.00};

    test_performance(1, ObservationMode::Mode::None, 98.75, epe_archived, ene_archived);
}

BOOST_AUTO_TEST_CASE(testSwapPerformanceDisableObs) {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Disable observation mode)");
    vector<Real> epe_archived = {
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       212.27,     0.00,       7323.88,    31533.28,   53378.95,   36207.00,  46906.01,
        104095.83,  135746.68,  125782.33,  140930.67,  182959.31,  194273.49,  189484.96,  243642.11, 322150.25,
        399831.48,  369521.42,  439131.00,  551140.41,  674989.42,  635496.50,  683438.19,  965512.49, 1089189.37,
        1120743.38, 1215602.57, 1746485.50, 1911132.65, 1937517.65, 2018342.66, 2749016.80, 2983288.29};
    vector<Real> ene_archived = {
        368479333.37, 366947138.93, 359696820.06, 367678966.20, 360306476.22, 360214990.08, 333790252.36, 333074799.18,
        326284318.93, 325852335.02, 300193565.70, 299829910.92, 292796703.76, 293767241.06, 269068886.49, 270243130.10,
        264709179.78, 266009430.50, 246284950.17, 245916331.08, 240391023.58, 240418624.73, 219999845.91, 220436827.62,
        213036646.42, 213985864.64, 194194752.86, 194421068.57, 187827555.98, 187926831.63, 169284588.22, 170055369.68,
        163559634.19, 164558813.48, 147752949.47, 149093664.98, 142730143.65, 144246717.03, 132026539.31, 137083359.41,
        130631368.01, 130187937.69, 117919243.84, 120129844.58, 113388318.81, 113037081.05, 100703902.26, 104901313.87,
        97792109.49,  97449264.40,  86410086.30,  90522266.38,  84155342.70,  85251403.29,  74564195.80,  80107600.03,
        73456108.79,  76608529.93,  68627084.36,  73334428.12,  64852662.37,  68227498.40,  61045476.14,  63509132.12,
        55901466.52,  59264990.10,  52421890.10,  55762525.83,  47861919.43,  51177217.97,  44467494.67,  47470977.67,
        39894747.69,  43187901.39,  37014398.67,  40137889.11,  32665143.70,  36437914.59,  30581997.34,  33515698.54};

    test_performance(100, ObservationMode::Mode::Disable, 70.5875, epe_archived, ene_archived);
}

BOOST_AUTO_TEST_CASE(testSingleSwapPerformanceDisableObs) {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Disable observation mode)");
    vector<Real> epe_archived = {
        8422.46,  11198.17, 15556.45, 22180.89, 24515.35, 22731.01, 24475.81, 30631.91, 32462.83, 28579.61,
        29796.73, 34820.71, 35791.98, 31444.10, 31421.22, 35378.44, 36713.70, 32176.09, 33109.12, 36913.64,
        38421.16, 33315.37, 33985.69, 37880.01, 39303.00, 34201.58, 34475.59, 37838.68, 38555.66, 33052.73,
        34178.13, 37796.68, 38291.89, 33090.13, 33801.90, 37407.56, 37883.08, 32242.32, 32894.99, 35663.04,
        36199.80, 30599.08, 31125.50, 33598.36, 33774.43, 27907.84, 28320.77, 30593.98, 30704.24, 24996.15,
        25219.71, 27475.72, 27991.94, 22261.52, 22503.79, 24273.17, 24606.12, 19183.91, 19377.61, 21039.90,
        21286.30, 15787.01, 15905.70, 17288.35, 17438.69, 11921.71, 12042.01, 13379.75, 13566.09, 8142.94,
        8244.72,  9312.21,  9336.38,  4025.76,  4011.88,  4742.72,  4713.73,  387.13,   386.43,   0.00};
    vector<Real> ene_archived = {
        15210.48, 23791.57, 25713.78, 21832.12, 24448.58, 30275.09, 31684.96, 28403.17, 30147.18, 35385.39,
        36347.77, 31485.92, 32451.88, 37585.83, 39032.52, 34615.34, 35484.08, 40387.23, 40795.70, 35499.49,
        37112.20, 40725.49, 41757.08, 36557.20, 37031.21, 40500.58, 41313.81, 36469.87, 37160.04, 39547.69,
        39861.84, 33664.42, 34444.08, 37206.24, 37685.74, 32158.34, 32322.59, 34521.05, 35196.78, 30809.12,
        31218.90, 33447.32, 34164.27, 28843.13, 29113.11, 32074.28, 32535.47, 28092.79, 27973.99, 30230.57,
        30331.73, 25129.41, 25443.92, 27195.73, 27727.14, 22540.71, 22623.75, 24868.88, 25036.23, 19195.48,
        19036.40, 21082.11, 21592.57, 15735.17, 15809.72, 17752.04, 17959.20, 12408.72, 12506.82, 13937.71,
        14004.02, 8403.87,  8375.64,  10190.85, 10229.44, 4311.72,  4277.53,  6773.50,  6779.32,  0.00};
    test_performance(1, ObservationMode::Mode::Disable, 98.75, epe_archived, ene_archived);
}

BOOST_AUTO_TEST_CASE(testSwapPerformanceDeferObs) {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Defer observation mode)");
    vector<Real> epe_archived = {
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       212.27,     0.00,       7323.88,    31533.28,   53378.95,   36207.00,  46906.01,
        104095.83,  135746.68,  125782.33,  140930.67,  182959.31,  194273.49,  189484.96,  243642.11, 322150.25,
        399831.48,  369521.42,  439131.00,  551140.41,  674989.42,  635496.50,  683438.19,  965512.49, 1089189.37,
        1120743.38, 1215602.57, 1746485.50, 1911132.65, 1937517.65, 2018342.66, 2749016.80, 2983288.29};
    vector<Real> ene_archived = {
        368479333.37, 366947138.93, 359696820.06, 367678966.20, 360306476.22, 360214990.08, 333790252.36, 333074799.18,
        326284318.93, 325852335.02, 300193565.70, 299829910.92, 292796703.76, 293767241.06, 269068886.49, 270243130.10,
        264709179.78, 266009430.50, 246284950.17, 245916331.08, 240391023.58, 240418624.73, 219999845.91, 220436827.62,
        213036646.42, 213985864.64, 194194752.86, 194421068.57, 187827555.98, 187926831.63, 169284588.22, 170055369.68,
        163559634.19, 164558813.48, 147752949.47, 149093664.98, 142730143.65, 144246717.03, 132026539.31, 137083359.41,
        130631368.01, 130187937.69, 117919243.84, 120129844.58, 113388318.81, 113037081.05, 100703902.26, 104901313.87,
        97792109.49,  97449264.40,  86410086.30,  90522266.38,  84155342.70,  85251403.29,  74564195.80,  80107600.03,
        73456108.79,  76608529.93,  68627084.36,  73334428.12,  64852662.37,  68227498.40,  61045476.14,  63509132.12,
        55901466.52,  59264990.10,  52421890.10,  55762525.83,  47861919.43,  51177217.97,  44467494.67,  47470977.67,
        39894747.69,  43187901.39,  37014398.67,  40137889.11,  32665143.70,  36437914.59,  30581997.34,  33515698.54};
    test_performance(100, ObservationMode::Mode::Defer, 70.5875, epe_archived, ene_archived);
}

BOOST_AUTO_TEST_CASE(testSingleSwapPerformanceDeferObs) {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Defer observation mode)");
    vector<Real> epe_archived = {
        8422.46,  11198.17, 15556.45, 22180.89, 24515.35, 22731.01, 24475.81, 30631.91, 32462.83, 28579.61,
        29796.73, 34820.71, 35791.98, 31444.10, 31421.22, 35378.44, 36713.70, 32176.09, 33109.12, 36913.64,
        38421.16, 33315.37, 33985.69, 37880.01, 39303.00, 34201.58, 34475.59, 37838.68, 38555.66, 33052.73,
        34178.13, 37796.68, 38291.89, 33090.13, 33801.90, 37407.56, 37883.08, 32242.32, 32894.99, 35663.04,
        36199.80, 30599.08, 31125.50, 33598.36, 33774.43, 27907.84, 28320.77, 30593.98, 30704.24, 24996.15,
        25219.71, 27475.72, 27991.94, 22261.52, 22503.79, 24273.17, 24606.12, 19183.91, 19377.61, 21039.90,
        21286.30, 15787.01, 15905.70, 17288.35, 17438.69, 11921.71, 12042.01, 13379.75, 13566.09, 8142.94,
        8244.72,  9312.21,  9336.38,  4025.76,  4011.88,  4742.72,  4713.73,  387.13,   386.43,   0.00};
    vector<Real> ene_archived = {
        15210.48, 23791.57, 25713.78, 21832.12, 24448.58, 30275.09, 31684.96, 28403.17, 30147.18, 35385.39,
        36347.77, 31485.92, 32451.88, 37585.83, 39032.52, 34615.34, 35484.08, 40387.23, 40795.70, 35499.49,
        37112.20, 40725.49, 41757.08, 36557.20, 37031.21, 40500.58, 41313.81, 36469.87, 37160.04, 39547.69,
        39861.84, 33664.42, 34444.08, 37206.24, 37685.74, 32158.34, 32322.59, 34521.05, 35196.78, 30809.12,
        31218.90, 33447.32, 34164.27, 28843.13, 29113.11, 32074.28, 32535.47, 28092.79, 27973.99, 30230.57,
        30331.73, 25129.41, 25443.92, 27195.73, 27727.14, 22540.71, 22623.75, 24868.88, 25036.23, 19195.48,
        19036.40, 21082.11, 21592.57, 15735.17, 15809.72, 17752.04, 17959.20, 12408.72, 12506.82, 13937.71,
        14004.02, 8403.87,  8375.64,  10190.85, 10229.44, 4311.72,  4277.53,  6773.50,  6779.32,  0.00};
    test_performance(1, ObservationMode::Mode::Defer, 98.75, epe_archived, ene_archived);
}

BOOST_AUTO_TEST_CASE(testSwapPerformanceUnregisterObs) {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Unregister observation mode)");
    vector<Real> epe_archived = {
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,       0.00,      0.00,
        0.00,       0.00,       212.27,     0.00,       7323.88,    31533.28,   53378.95,   36207.00,  46906.01,
        104095.83,  135746.68,  125782.33,  140930.67,  182959.31,  194273.49,  189484.96,  243642.11, 322150.25,
        399831.48,  369521.42,  439131.00,  551140.41,  674989.42,  635496.50,  683438.19,  965512.49, 1089189.37,
        1120743.38, 1215602.57, 1746485.50, 1911132.65, 1937517.65, 2018342.66, 2749016.80, 2983288.29};
    vector<Real> ene_archived = {
        368479333.37, 366947138.93, 359696820.06, 367678966.20, 360306476.22, 360214990.08, 333790252.36, 333074799.18,
        326284318.93, 325852335.02, 300193565.70, 299829910.92, 292796703.76, 293767241.06, 269068886.49, 270243130.10,
        264709179.78, 266009430.50, 246284950.17, 245916331.08, 240391023.58, 240418624.73, 219999845.91, 220436827.62,
        213036646.42, 213985864.64, 194194752.86, 194421068.57, 187827555.98, 187926831.63, 169284588.22, 170055369.68,
        163559634.19, 164558813.48, 147752949.47, 149093664.98, 142730143.65, 144246717.03, 132026539.31, 137083359.41,
        130631368.01, 130187937.69, 117919243.84, 120129844.58, 113388318.81, 113037081.05, 100703902.26, 104901313.87,
        97792109.49,  97449264.40,  86410086.30,  90522266.38,  84155342.70,  85251403.29,  74564195.80,  80107600.03,
        73456108.79,  76608529.93,  68627084.36,  73334428.12,  64852662.37,  68227498.40,  61045476.14,  63509132.12,
        55901466.52,  59264990.10,  52421890.10,  55762525.83,  47861919.43,  51177217.97,  44467494.67,  47470977.67,
        39894747.69,  43187901.39,  37014398.67,  40137889.11,  32665143.70,  36437914.59,  30581997.34,  33515698.54};
    test_performance(100, ObservationMode::Mode::Unregister, 70.5875, epe_archived, ene_archived);
}

BOOST_AUTO_TEST_CASE(testSingleSwapPerformanceUnregisterObs) {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Unregister observation mode)");
    vector<Real> epe_archived = {
        8422.46,  11198.17, 15556.45, 22180.89, 24515.35, 22731.01, 24475.81, 30631.91, 32462.83, 28579.61,
        29796.73, 34820.71, 35791.98, 31444.10, 31421.22, 35378.44, 36713.70, 32176.09, 33109.12, 36913.64,
        38421.16, 33315.37, 33985.69, 37880.01, 39303.00, 34201.58, 34475.59, 37838.68, 38555.66, 33052.73,
        34178.13, 37796.68, 38291.89, 33090.13, 33801.90, 37407.56, 37883.08, 32242.32, 32894.99, 35663.04,
        36199.80, 30599.08, 31125.50, 33598.36, 33774.43, 27907.84, 28320.77, 30593.98, 30704.24, 24996.15,
        25219.71, 27475.72, 27991.94, 22261.52, 22503.79, 24273.17, 24606.12, 19183.91, 19377.61, 21039.90,
        21286.30, 15787.01, 15905.70, 17288.35, 17438.69, 11921.71, 12042.01, 13379.75, 13566.09, 8142.94,
        8244.72,  9312.21,  9336.38,  4025.76,  4011.88,  4742.72,  4713.73,  387.13,   386.43,   0.00};
    vector<Real> ene_archived = {
        15210.48, 23791.57, 25713.78, 21832.12, 24448.58, 30275.09, 31684.96, 28403.17, 30147.18, 35385.39,
        36347.77, 31485.92, 32451.88, 37585.83, 39032.52, 34615.34, 35484.08, 40387.23, 40795.70, 35499.49,
        37112.20, 40725.49, 41757.08, 36557.20, 37031.21, 40500.58, 41313.81, 36469.87, 37160.04, 39547.69,
        39861.84, 33664.42, 34444.08, 37206.24, 37685.74, 32158.34, 32322.59, 34521.05, 35196.78, 30809.12,
        31218.90, 33447.32, 34164.27, 28843.13, 29113.11, 32074.28, 32535.47, 28092.79, 27973.99, 30230.57,
        30331.73, 25129.41, 25443.92, 27195.73, 27727.14, 22540.71, 22623.75, 24868.88, 25036.23, 19195.48,
        19036.40, 21082.11, 21592.57, 15735.17, 15809.72, 17752.04, 17959.20, 12408.72, 12506.82, 13937.71,
        14004.02, 8403.87,  8375.64,  10190.85, 10229.44, 4311.72,  4277.53,  6773.50,  6779.32,  0.00};

    test_performance(1, ObservationMode::Mode::Unregister, 98.75, epe_archived, ene_archived);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
