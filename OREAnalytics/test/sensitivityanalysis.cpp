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
#include <ql/time/date.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <orea/engine/all.hpp>
#include <ored/portfolio/swap.hpp>
#include <ored/portfolio/swaption.hpp>
#include <ored/portfolio/builders/swap.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/builders/fxforward.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/capfloor.hpp>
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

boost::shared_ptr<data::Conventions> conv() {
    boost::shared_ptr<data::Conventions> conventions(new data::Conventions());

    boost::shared_ptr<data::Convention> swapIndexConv(
        new data::SwapIndexConvention("EUR-CMS-2Y", "EUR-6M-SWAP-CONVENTIONS"));
    conventions->add(swapIndexConv);

    boost::shared_ptr<data::Convention> swapConv(
        new data::IRSwapConvention("EUR-6M-SWAP-CONVENTIONS", "TARGET", "Annual", "MF", "30/360", "EUR-EURIBOR-6M"));
    conventions->add(swapConv);

    return conventions;
}

void test_sensitivity() {
    BOOST_TEST_MESSAGE("Testing Portfolio sensitivity");

    SavedSettings backup;

    Date today = Date(14, April, 2016); // Settings::instance().evaluationDate();
    Settings::instance().evaluationDate() = today;

    BOOST_TEST_MESSAGE("Today is " << today);

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
    boost::shared_ptr<analytics::ScenarioSimMarketParameters> simMarketData(new analytics::ScenarioSimMarketParameters());
    simMarketData->baseCcy() = "EUR";
    simMarketData->ccys() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    simMarketData->yieldCurveTenors() = {1*Months, 6*Months, 1*Years, 2*Years, 5*Years, 10*Years, 20*Years};
    simMarketData->indices() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    simMarketData->interpolation() = "LogLinear";
    simMarketData->extrapolate() = true;

    simMarketData->swapVolTerms() = {6*Months, 1*Years, 5*Years, 10*Years};
    simMarketData->swapVolExpiries() = {1*Years, 2*Years, 5*Years, 10*Years};
    simMarketData->swapVolCcys() = ccys;
    simMarketData->swapVolDecayMode() = "ForwardVariance";
    simMarketData->simulateSwapVols() = true;//false;

    simMarketData->fxVolExpiries() = {1*Months, 3*Months, 6*Months, 2*Years, 3*Years, 4*Years, 5*Years};
    simMarketData->fxVolDecayMode() = "ConstantVariance";
    simMarketData->simulateFXVols() = true; //false;

    simMarketData->ccyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};

    // sensitivity config
    boost::shared_ptr<SensitivityScenarioData> sensiData = boost::make_shared<SensitivityScenarioData>();
    // sensiData->irCurrencies() = {"EUR", "GBP", "USD", "CHF", "JPY"};
    // sensiData->irIndices() = {"EUR-EURIBOR-6M", "USD-LIBOR-3M", "GBP-LIBOR-6M", "CHF-LIBOR-6M", "JPY-LIBOR-6M"};
    sensiData->irByCurrency() = false;
    sensiData->irDomain() = "ZERO";
    sensiData->irShiftTenors() = {1*Years, 2*Years, 3*Years, 5*Years, 7*Years, 10*Years, 15*Years, 20*Years}; // multiple tenors: triangular shifts
    //sensiData->irShiftTenors() = {5*Years}; // one tenor point means parallel shift
    sensiData->irShiftType() = "Absolute";
    sensiData->irShiftSize() = 0.0001; 
    
    // sensiData->fxCurrencyPairs() = {"EURUSD", "EURGBP", "EURCHF", "EURJPY"};
    sensiData->fxShiftType() = "Relative";
    sensiData->fxShiftSize() = 0.01; 
    sensiData->fxVolShiftExpiries() = {1*Years, 2*Years, 3*Years}; 

    sensiData->fxVolShiftType() = "Relative";
    sensiData->fxVolShiftSize() = 0.01; 
    
    sensiData->swaptionVolShiftType() = "Relative";
    sensiData->swaptionVolShiftSize() = 0.01; 
    sensiData->swaptionVolShiftExpiries() = {1*Years, 2*Years, 3*Years, 5*Years}; 
    sensiData->swaptionVolShiftTerms() = {1*Years, 2*Years, 3*Years, 5*Years}; 

    // build scenario generator
    boost::shared_ptr<ScenarioFactory> scenarioFactory(new SimpleScenarioFactory);
    boost::shared_ptr<SensitivityScenarioGenerator> scenarioGenerator(new SensitivityScenarioGenerator(scenarioFactory, sensiData, simMarketData, today, initMarket));
    boost::shared_ptr<ScenarioGenerator> sgen(scenarioGenerator);
    
    // build scenario sim market
    Conventions conventions = *conv();
    boost::shared_ptr<analytics::SimMarket> simMarket =
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

    //boost::shared_ptr<Portfolio> portfolio = buildSwapPortfolio(portfolioSize, factory);
    boost::shared_ptr<Portfolio> portfolio(new Portfolio());
    portfolio->add(buildSwap("1_Swap_EUR", "EUR", true, 10000.0, 0, 10, 0.03, 0.00, "1Y", "30/360", "6M", "A360", "EUR-EURIBOR-6M"));    
    portfolio->add(buildSwap("2_Swap_USD", "USD", true, 10000.0, 0, 15, 0.02, 0.00, "6M", "30/360", "3M", "A360", "USD-LIBOR-3M"));    
    portfolio->add(buildSwap("3_Swap_GBP", "GBP", true, 10000.0, 0, 20, 0.04, 0.00, "6M", "30/360", "3M", "A360", "GBP-LIBOR-6M"));    
    portfolio->add(buildSwap("4_Swap_JPY", "JPY", true, 1000000.0, 0, 5,  0.01, 0.00, "6M", "30/360", "3M", "A360", "JPY-LIBOR-6M"));    
    portfolio->add(buildEuropeanSwaption("5_Swaption_EUR", "Long", "EUR", true, 10000.0, 10, 10, 0.03, 0.00, "1Y", "30/360", "6M", "A360", "EUR-EURIBOR-6M"));    
    portfolio->add(buildFxOption("6_FxOption_EUR_USD", "Long", "Call", 3, "EUR", 10000.0, "USD", 11000.0));
    portfolio->build(factory);

    BOOST_TEST_MESSAGE("Portfolio size after build: " << portfolio->size());

    // build the scenario engine
    ScenarioEngine engine(today, simMarket, simMarketData->baseCcy());

    // run scenarios and fill the cube
    boost::timer t;
    boost::shared_ptr<NPVCube> cube =
      boost::make_shared<DoublePrecisionInMemoryCube>(today, portfolio->ids(), vector<Date>(1, today), scenarioGenerator->samples());
    engine.buildCube(portfolio, cube);
    double elapsed = t.elapsed();

    Real tiny = 1e-10;
    for (Size i = 0; i < portfolio->size(); ++i) {
        Real npv0 = cube->getT0(i, 0); 
	string id = portfolio->trades()[i]->id();
	for (Size j = 0; j < scenarioGenerator->samples(); ++j ) {
	    Real npv = cube->get(i, 0, j, 0);
	    Real sensi = npv - npv0;
	    string label = scenarioGenerator->scenarios()[j]->label();
	    if (fabs(sensi) > tiny)
	      BOOST_TEST_MESSAGE(id << " " << label << " " << sensi); 
	}
    }
    
    BOOST_TEST_MESSAGE("Cube generated in " << elapsed << " seconds");

    Size numNPVs = scenarioGenerator->samples() * portfolio->size();
    BOOST_TEST_MESSAGE("Cube size = " << numNPVs << " elements");
    Real pricingTimeMicroSeconds = elapsed * 1000000 / numNPVs;
    BOOST_TEST_MESSAGE("Avg Pricing time = " << pricingTimeMicroSeconds << " microseconds");

    BOOST_TEST_MESSAGE(os::getSystemDetails());

    // Compute sensitivities and check vs cached results
    /*
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
    */
}

void SensitivityAnalysisTest::testPortfolioSensitivity() { test_sensitivity(); }

test_suite* SensitivityAnalysisTest::suite() {
    // Uncomment the below to get detailed output TODO: custom logger that uses BOOST_MESSAGE
    
    boost::shared_ptr<ore::data::FileLogger> logger
        = boost::make_shared<ore::data::FileLogger>("sensitivity.log");
    ore::data::Log::instance().removeAllLoggers();
    ore::data::Log::instance().registerLogger(logger);
    ore::data::Log::instance().switchOn();
    ore::data::Log::instance().setMask(255);
    

    test_suite* suite = BOOST_TEST_SUITE("SensitivityAnalysisTest");
    // Set the Observation mode here
    ObservationMode::instance().setMode(ObservationMode::Mode::Unregister);
    suite->add(BOOST_TEST_CASE(&SensitivityAnalysisTest::testPortfolioSensitivity));
    return suite;
}
}
