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

#include "testmarket.hpp"

using namespace std;
using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace ore;
using namespace ore::data;
using namespace ore::analytics;

using testsuite::TestMarket;

BOOST_FIXTURE_TEST_SUITE(OREAnalyticsTestSuite, ore::test::OreaTopLevelFixture)

BOOST_AUTO_TEST_SUITE(SwapPerformaceTest, *boost::unit_test::disabled())

// Returns an int in the interval [min, max]. Inclusive.
inline unsigned long randInt(MersenneTwisterUniformRng& rng, Size min, Size max) {
    return min + (rng.nextInt32() % (max + 1 - min));
}

inline const string& randString(MersenneTwisterUniformRng& rng, const vector<string>& strs) {
    return strs[randInt(rng, 0, strs.size() - 1)];
}

inline bool randBoolean(MersenneTwisterUniformRng& rng) { return randInt(rng, 0, 1) == 1; }

QuantLib::ext::shared_ptr<data::Conventions> convs() {
    QuantLib::ext::shared_ptr<data::Conventions> conventions(new data::Conventions());

    QuantLib::ext::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    QuantLib::ext::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    InstrumentConventions::instance().setConventions(conventions);

    return conventions;
}

QuantLib::ext::shared_ptr<Portfolio> buildPortfolio(Size portfolioSize, QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    QuantLib::ext::shared_ptr<Portfolio> portfolio(new Portfolio());

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
        LegData fixedLeg(QuantLib::ext::make_shared<FixedLegData>(vector<double>(1, fixedRate)), isPayer, ccy, fixedSchedule,
                         fixDC, notional);

        // float Leg
        vector<double> spreads(1, 0);
        LegData floatingLeg(QuantLib::ext::make_shared<FloatingLegData>(index, days, false, spread), !isPayer, ccy,
                            floatSchedule, floatDC, notional);

        QuantLib::ext::shared_ptr<Trade> swap(new data::Swap(env, floatingLeg, fixedLeg));

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
        QuantLib::ext::shared_ptr<data::Swap> swap = QuantLib::ext::dynamic_pointer_cast<data::Swap>(trade);
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

    // Log::instance().registerLogger(QuantLib::ext::make_shared<StderrLogger>());
    // Log::instance().switchOn();

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

    string dateGridStr = "80,3M"; // 20 years
    QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>(dateGridStr);
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
    QuantLib::ext::shared_ptr<Market> initMarket = QuantLib::ext::make_shared<TestMarket>(today);

    // build scenario sim market parameters
    QuantLib::ext::shared_ptr<analytics::ScenarioSimMarketParameters> parameters(new analytics::ScenarioSimMarketParameters());
    parameters->baseCcy() = "EUR";
    parameters->setDiscountCurveNames({"EUR", "GBP", "USD", "CHF", "JPY"});
    parameters->setYieldCurveTenors("",
                                    {1 * Months, 6 * Months, 1 * Years, 2 * Years, 5 * Years, 10 * Years, 20 * Years});
    parameters->setIndices({"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"});

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

    parameters->setFxVolCcyPairs({"USDEUR", "GBPEUR", "CHFEUR", "JPYEUR"});
    parameters->setFxCcyPairs({"USDEUR", "GBPEUR", "CHFEUR", "JPYEUR"});

    parameters->setEquityVolExpiries("", {1 * Months, 3 * Months, 6 * Months, 2 * Years, 3 * Years, 4 * Years, 5 * Years});
    parameters->setEquityVolDecayMode("ConstantVariance");
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
    CorrelationFactor f_1{ CrossAssetModel::AssetType::IR, "EUR", 0 };
    CorrelationFactor f_2{ CrossAssetModel::AssetType::IR, "USD", 0 };
    corr[make_pair(f_1, f_2)] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.6));

    QuantLib::ext::shared_ptr<CrossAssetModelData> config(QuantLib::ext::make_shared<CrossAssetModelData>(irConfigs, fxConfigs, corr));

    // Model Builder & Model
    // model builder
    QuantLib::ext::shared_ptr<CrossAssetModelBuilder> modelBuilder(new CrossAssetModelBuilder(initMarket, config));
    QuantLib::ext::shared_ptr<QuantExt::CrossAssetModel> model = *modelBuilder->model();
    modelBuilder = NULL;

    // Path generator
    Size seed = 5;
    bool antithetic = false;
    if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(model->stateProcess())) {
        tmp->resetCache(dg->timeGrid().size() - 1);
    }
    QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGen =
        QuantLib::ext::make_shared<MultiPathGeneratorMersenneTwister>(model->stateProcess(), dg->timeGrid(), seed, antithetic);

    // build scenario generator
    QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory = QuantLib::ext::make_shared<SimpleScenarioFactory>(true);
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator = QuantLib::ext::make_shared<CrossAssetModelScenarioGenerator>(
        model, pathGen, scenarioFactory, parameters, today, dg, initMarket);

    // build scenario sim market
    convs();
    auto simMarket = QuantLib::ext::make_shared<analytics::ScenarioSimMarket>(initMarket, parameters);
    simMarket->scenarioGenerator() = scenarioGenerator;

    // Build Portfolio
    QuantLib::ext::shared_ptr<EngineData> data = QuantLib::ext::make_shared<EngineData>();
    data->model("Swap") = "DiscountedCashflows";
    data->engine("Swap") = "DiscountingSwapEngine";
    QuantLib::ext::shared_ptr<EngineFactory> factory = QuantLib::ext::make_shared<EngineFactory>(data, simMarket);

    QuantLib::ext::shared_ptr<Portfolio> portfolio = buildPortfolio(portfolioSize, factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // Now calculate exposure
    ValuationEngine valEngine(today, dg, simMarket);

    // Calculate Cube
    boost::timer::cpu_timer t;
    QuantLib::ext::shared_ptr<NPVCube> cube =
        QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(today, portfolio->ids(), dg->dates(), samples);
    vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
    calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(baseCcy));
    valEngine.buildCube(portfolio, cube, calculators);
    t.stop();
    double elapsed = t.elapsed().wall * 1e-9;

    BOOST_TEST_MESSAGE("Cube generated in " << elapsed << " seconds");

    Size dates = dg->dates().size();
    Size numNPVs = dates * samples * portfolioSize;
    BOOST_TEST_MESSAGE("Cube size = " << numNPVs << " elements");
    BOOST_TEST_MESSAGE("Cube elements theoretical storage " << numNPVs * sizeof(Real) / (1024 * 1024) << " MB");
    Real pricingTimeMicroSeconds = elapsed * 1000000 / numNPVs;
    BOOST_TEST_MESSAGE("Avg Pricing time = " << pricingTimeMicroSeconds << " microseconds");

    // Count the number of times we have an empty line, ie. swap expired.
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

    // Compute portfolio EPE and ENE
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

namespace {
vector<Real> swap_epe_archived = {
    0,           0,           0,           0,           0,           0,           0,           0,          0,
    0,           0,           0,           0,           0,           0,           0,           0,          0,
    0,           0,           0,           0,           0,           0,           0,           0,          0,
    0,           0,           0,           0,           0,           0,           0,           0,          0,
    0,           0,           0,           0,           0,           0,           0,           0,          0,
    0,           0,           212.058,     0,           7323.56,     31533.6,     53382.9,     36210.1,    46908.3,
    104101,      135755,      125789,      140937,      182967,      194282,      189492,      243649,     322158,
    399840,      369531,      439146,      551159,      675010,      635516,      683456,      965534,     1.08921e+06,
    1.12077e+06, 1.21563e+06, 1.74652e+06, 1.91117e+06, 1.93755e+06, 2.01838e+06, 2.74905e+06, 2.98333e+06};
vector<Real> swap_ene_archived = {
    3.68479e+08, 3.66947e+08, 3.59697e+08, 3.67679e+08, 3.60306e+08, 3.60215e+08, 3.3379e+08,  3.33075e+08, 3.26284e+08,
    3.25852e+08, 3.00194e+08, 2.9983e+08,  2.92797e+08, 2.93767e+08, 2.69069e+08, 2.70243e+08, 2.64709e+08, 2.66009e+08,
    2.46285e+08, 2.45916e+08, 2.40391e+08, 2.40419e+08, 2.2e+08,     2.20437e+08, 2.13037e+08, 2.13986e+08, 1.94195e+08,
    1.94421e+08, 1.87828e+08, 1.87927e+08, 1.69285e+08, 1.70055e+08, 1.6356e+08,  1.64559e+08, 1.47753e+08, 1.49094e+08,
    1.4273e+08,  1.44247e+08, 1.32027e+08, 1.37083e+08, 1.30631e+08, 1.30188e+08, 1.17919e+08, 1.2013e+08,  1.13388e+08,
    1.13037e+08, 1.00704e+08, 1.04901e+08, 9.77921e+07, 9.74493e+07, 8.64101e+07, 9.05223e+07, 8.41554e+07, 8.52514e+07,
    7.45642e+07, 8.01076e+07, 7.34561e+07, 7.66085e+07, 6.86271e+07, 7.33344e+07, 6.48527e+07, 6.82275e+07, 6.10455e+07,
    6.35091e+07, 5.59015e+07, 5.9265e+07,  5.24219e+07, 5.57625e+07, 4.78619e+07, 5.11772e+07, 4.44675e+07, 4.7471e+07,
    3.98948e+07, 4.31879e+07, 3.70144e+07, 4.01379e+07, 3.26652e+07, 3.64379e+07, 3.0582e+07,  3.35157e+07};
vector<Real> single_swap_epe_archived = {
    8422.98, 11198.9, 15557.4, 22182,   24516.4, 22732.1, 24476.9, 30633,   32463.9, 28580.7, 29797.8, 34821.8,
    35793,   31445.1, 31422.2, 35379.4, 36714.7, 32177,   33110.1, 36914.5, 38422.1, 33316.3, 33986.7, 37881,
    39304,   34202.6, 34476.6, 37839.7, 38556.6, 33053.6, 34179,   37797.4, 38292.6, 33090.8, 33802.5, 37408.1,
    37883.6, 32242.8, 32895.4, 35663.4, 36200.2, 30599.5, 31125.9, 33598.7, 33774.8, 27908.2, 28321.2, 30594.3,
    30704.6, 24996.5, 25220.1, 27476.1, 27992.3, 22261.9, 22504.1, 24273.5, 24606.4, 19184.2, 19377.9, 21040.2,
    21286.6, 15787.3, 15905.9, 17288.6, 17438.9, 11921.9, 12042.2, 13379.9, 13566.2, 8143.05, 8244.83, 9312.29,
    9336.46, 4025.82, 4011.94, 4742.76, 4713.78, 387.137, 386.445, 0};
vector<Real> single_swap_ene_archived = {
    15211,   23792.4, 25714.7, 21833.1, 24449.6, 30276.1, 31685.9, 28404.2, 30148.2, 35386.3, 36348.7, 31486.9,
    32452.8, 37586.7, 39033.4, 34616.2, 35485,   40388.1, 40796.5, 35500.3, 37113.1, 40726.3, 41758,   36558.1,
    37032.1, 40501.5, 41314.7, 36470.8, 37160.9, 39548.5, 39862.6, 33665.1, 34444.8, 37206.8, 37686.3, 32158.8,
    32323.1, 34521.5, 35197.2, 30809.5, 31219.3, 33447.7, 34164.7, 28843.5, 29113.5, 32074.7, 32535.9, 28093.2,
    27974.4, 30230.9, 30332.1, 25129.8, 25444.3, 27196.1, 27727.5, 22541,   22624.1, 24869.2, 25036.5, 19195.8,
    19036.7, 21082.3, 21592.8, 15735.4, 15809.9, 17752.2, 17959.4, 12408.9, 12507,   13937.8, 14004.1, 8403.95,
    8375.73, 10190.9, 10229.5, 4311.76, 4277.57, 6773.51, 6779.33, 0};
} // namespace

BOOST_AUTO_TEST_CASE(testSwapPerformanceNoneObs) {
    BOOST_TEST_MESSAGE("Testing Swap Performance (None observation mode)");
    test_performance(100, ObservationMode::Mode::None, 70.5875, swap_epe_archived, swap_ene_archived);
}

BOOST_AUTO_TEST_CASE(testSingleSwapPerformanceNoneObs) {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (None observation mode)");
    test_performance(1, ObservationMode::Mode::None, 98.75, single_swap_epe_archived, single_swap_ene_archived);
}

BOOST_AUTO_TEST_CASE(testSwapPerformanceDisableObs) {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Disable observation mode)");
    test_performance(100, ObservationMode::Mode::None, 70.5875, swap_epe_archived, swap_ene_archived);
}

BOOST_AUTO_TEST_CASE(testSingleSwapPerformanceDisableObs) {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Disable observation mode)");
    test_performance(1, ObservationMode::Mode::None, 98.75, single_swap_epe_archived, single_swap_ene_archived);
}

BOOST_AUTO_TEST_CASE(testSwapPerformanceDeferObs) {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Defer observation mode)");
    test_performance(100, ObservationMode::Mode::None, 70.5875, swap_epe_archived, swap_ene_archived);
}

BOOST_AUTO_TEST_CASE(testSingleSwapPerformanceDeferObs) {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Defer observation mode)");
    test_performance(1, ObservationMode::Mode::None, 98.75, single_swap_epe_archived, single_swap_ene_archived);
}

BOOST_AUTO_TEST_CASE(testSwapPerformanceUnregisterObs) {
    BOOST_TEST_MESSAGE("Testing Swap Performance (Unregister observation mode)");
    test_performance(100, ObservationMode::Mode::None, 70.5875, swap_epe_archived, swap_ene_archived);
}

BOOST_AUTO_TEST_CASE(testSingleSwapPerformanceUnregisterObs) {
    BOOST_TEST_MESSAGE("Testing Single Swap Performance (Unregister observation mode)");
    test_performance(1, ObservationMode::Mode::None, 98.75, single_swap_epe_archived, single_swap_ene_archived);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
