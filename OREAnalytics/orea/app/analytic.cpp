/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <orea/app/analytic.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/structuredanalyticswarning.hpp>
#include <orea/engine/bufferedsensitivitystream.hpp>
#include <orea/engine/filteredsensitivitystream.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/engine/zerotoparcube.hpp>
#include <orea/cube/cubewriter.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/engine/valuationengine.hpp>
#include <orea/aggregation/dimregressioncalculator.hpp>

#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/builders/currencyswap.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/multilegoption.hpp>
#include <ored/portfolio/builders/swaption.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>

#include <boost/timer/timer.hpp>

#include <iostream>

using namespace ore::data;
using namespace boost::filesystem;
using boost::timer::cpu_timer;
using boost::timer::default_places;

namespace ore {
namespace analytics {

Analytic::Analytic(std::unique_ptr<Impl> impl,
         const std::set<std::string>& analyticTypes,
         const boost::shared_ptr<InputParameters>& inputs,
         bool simulationConfig,
         bool sensitivityConfig,
         bool scenarioGeneratorConfig,
         bool crossAssetModelConfig)
    : impl_(std::move(impl)), types_(analyticTypes), inputs_(inputs) {
    
    // set these here, can be overwritten in setUpConfigurations
    if (inputs->curveConfigs().size() > 0)
        configurations().curveConfig = inputs->curveConfigs()[0];
    if (inputs->pricingEngine())
        configurations().engineData = inputs->pricingEngine();

    configurations().simulationConfigRequired = simulationConfig;
    configurations().sensitivityConfigRequired = sensitivityConfig;
    configurations().scenarioGeneratorConfigRequired = scenarioGeneratorConfig;
    configurations().crossAssetModelConfigRequired = crossAssetModelConfig;

    if (impl_) {
        impl_->setAnalytic(this);
        impl_->setGenerateAdditionalResults(inputs_->outputAdditionalResults());
    }

    setUpConfigurations();
}

void Analytic::runAnalytic(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                         const std::set<std::string>& runTypes) {
    if (impl_)
        impl_->runAnalytic(loader, runTypes);
}

void Analytic::setUpConfigurations() {
    if (impl_)
        impl_->setUpConfigurations();
}

const std::string Analytic::label() const { 
    return impl_ ? impl_->label() : string(); 
}

// Analytic
bool Analytic::match(const std::set<std::string>& runTypes) {
    if (runTypes.size() == 0)
        return true;
    
    for (const auto& rt : runTypes) {
        if (types_.find(rt) != types_.end()) {
            LOG("Requested analytics " <<  to_string(runTypes) << " match analytic class " << label());
            return true;
        }
    }
    WLOG("None of the requested analytics " << to_string(runTypes) << " are covered by the analytic class " << label());
    return false;
}

std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> Analytic::todaysMarketParams() {
    buildConfigurations();
    std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>> tmps;
    if (configurations().todaysMarketParams)
        tmps.push_back(configurations().todaysMarketParams);

    for (const auto& a : dependentAnalytics_) {
        auto ctmps = a.second->todaysMarketParams();
        tmps.insert(end(tmps), begin(ctmps), end(ctmps));
    }

    return tmps;
}

boost::shared_ptr<EngineFactory> Analytic::Impl::engineFactory() {
    LOG("Analytic::engineFactory() called");
    // Note: Calling the constructor here with empty extry builders
    // Override this function in case you have got extra ones
    boost::shared_ptr<EngineData> edCopy = boost::make_shared<EngineData>(*inputs_->pricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = to_string(generateAdditionalResults());
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    LOG("MarketContext::pricing = " << inputs_->marketConfig("pricing"));
    return boost::make_shared<EngineFactory>(edCopy, analytic()->market(), configurations,
                                             inputs_->refDataManager(),
                                             *inputs_->iborFallbackConfig());
}

void Analytic::buildMarket(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                           const bool marketRequired) {
    LOG("Analytic::buildMarket called");    
    cpu_timer mtimer;

    QL_REQUIRE(loader, "market data loader not set");
    QL_REQUIRE(configurations().curveConfig, "curve configurations not set");
    
    // first build the market if we have a todaysMarketParams
    if (configurations().todaysMarketParams) {
        try {
            // Note: we usually update the loader with implied data, but we simply use the provided loader here
             loader_ = loader;
            // Check that the loader has quotes
            QL_REQUIRE( loader_->hasQuotes(inputs()->asof()),
                       "There are no quotes available for date " << inputs()->asof());
            // Build the market
            market_ = boost::make_shared<TodaysMarket>(inputs()->asof(), configurations().todaysMarketParams, loader_,
                                                       configurations().curveConfig, inputs()->continueOnError(),
                                                       true, inputs()->lazyMarketBuilding(), inputs()->refDataManager(),
                                                       false, *inputs()->iborFallbackConfig());
            // Note: we usually wrap the market into a PC market, but skip this step here
        } catch (const std::exception& e) {
            if (marketRequired)
                QL_FAIL("Failed to build market: " << e.what());
            else
                WLOG("Failed to build market: " << e.what());
        }
    } else {
        ALOG("Skip building the market due to missing today's market parameters in configurations"); 
    }
    mtimer.stop();
    LOG("Market Build time " << setprecision(2) << mtimer.format(default_places, "%w") << " sec");
}

void Analytic::marketCalibration(const boost::shared_ptr<MarketCalibrationReport>& mcr) {
    if (mcr)
        mcr->populateReport(market_, configurations().todaysMarketParams);
}

void Analytic::buildPortfolio() {
    // create a new empty portfolio
    portfolio_ = boost::make_shared<Portfolio>(inputs()->buildFailedTrades());

    inputs()->portfolio()->reset();
    // populate with trades
    for (const auto& [tradeId, trade] : inputs()->portfolio()->trades())
        portfolio()->add(trade);
    
    if (market_) {
        replaceTrades();

        LOG("Build the portfolio");
        boost::shared_ptr<EngineFactory> factory = impl()->engineFactory();
        portfolio()->build(factory, "analytic/" + label());

        // remove dates that will have matured
        Date maturityDate = inputs()->asof();
        if (inputs()->portfolioFilterDate() != Null<Date>())
            maturityDate = inputs()->portfolioFilterDate();

        LOG("Filter trades that expire before " << maturityDate);
        portfolio()->removeMatured(maturityDate);
    } else {
        ALOG("Skip building the portfolio, because market not set");
    }
}

/*******************************************************************
 * MARKET Analytic
 *******************************************************************/

void MarketDataAnalyticImpl::setUpConfigurations() {    
    analytic()->configurations().todaysMarketParams = inputs_->todaysMarketParams();
}

void MarketDataAnalyticImpl::runAnalytic( 
    const boost::shared_ptr<ore::data::InMemoryLoader>& loader, 
    const std::set<std::string>& runTypes) {

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    CONSOLEW("Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");

    if (inputs_->outputTodaysMarketCalibration()) {
        CONSOLEW("Market Calibration");
        LOG("Write todays market calibration report");
        auto t = boost::dynamic_pointer_cast<TodaysMarket>(analytic()->market());
        QL_REQUIRE(t != nullptr, "expected todays market instance");
        boost::shared_ptr<InMemoryReport> mktReport = boost::make_shared<InMemoryReport>();
        ore::analytics::ReportWriter(inputs_->reportNaString())
            .writeTodaysMarketCalibrationReport(*mktReport, t->calibrationInfo());
        analytic()->reports()["MARKET"]["todaysmarketcalibration"] = mktReport;
        CONSOLE("OK");
    }
}

} // namespace analytics
} // namespace ore
