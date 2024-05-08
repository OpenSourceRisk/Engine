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

#include <ored/marketdata/compositeloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/marketdata/bondspreadimply.hpp>
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
         const QuantLib::ext::shared_ptr<InputParameters>& inputs,
         bool simulationConfig,
         bool sensitivityConfig,
         bool scenarioGeneratorConfig,
         bool crossAssetModelConfig)
    : impl_(std::move(impl)), types_(analyticTypes), inputs_(inputs) {

    configurations().asofDate = inputs->asof();

    // set these here, can be overwritten in setUpConfigurations
    if (inputs->curveConfigs().has())
        configurations().curveConfig = inputs->curveConfigs().get();
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

void Analytic::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                           const std::set<std::string>& runTypes) {
    MEM_LOG_USING_LEVEL(ORE_WARNING)
    if (impl_) {
        impl_->runAnalytic(loader, runTypes);
        MEM_LOG_USING_LEVEL(ORE_WARNING)
    }
}

void Analytic::setUpConfigurations() {
    if (impl_)
        impl_->setUpConfigurations();
}

std::vector<QuantLib::ext::shared_ptr<Analytic>> Analytic::Impl::allDependentAnalytics() const {
    std::vector<QuantLib::ext::shared_ptr<Analytic>> analytics;
    for (const auto& [_, a] : dependentAnalytics_) {
        analytics.push_back(a);
        auto das = a->allDependentAnalytics();
        analytics.insert(end(analytics), begin(das), end(das));
    }
    return analytics;
}

QuantLib::ext::shared_ptr<Analytic> Analytic::Impl::dependentAnalytic(const std::string& key) const {
    auto it = dependentAnalytics_.find(key);
    QL_REQUIRE(it != dependentAnalytics_.end(), "Could not find dependent Analytic " << key);
    return it->second;
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
    return false;
}

std::vector<QuantLib::ext::shared_ptr<Analytic>> Analytic::allDependentAnalytics() const {
    return impl_->allDependentAnalytics();
}

std::set<QuantLib::Date> Analytic::marketDates() const {
    std::set<QuantLib::Date> mds = {inputs_->asof()};
    auto addDates = impl_->additionalMarketDates();
    mds.insert(addDates.begin(), addDates.end());

    for (const auto& a : impl_->dependentAnalytics()) {
        addDates = a.second->impl()->additionalMarketDates();
        mds.insert(addDates.begin(), addDates.end());
    }
    return mds;
}

std::vector<QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>> Analytic::todaysMarketParams() {
    buildConfigurations();
    std::vector<QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>> tmps;
    if (configurations().todaysMarketParams)
        tmps.push_back(configurations().todaysMarketParams);

    for (const auto& a : impl_->dependentAnalytics()) {
        auto ctmps = a.second->todaysMarketParams();
        tmps.insert(end(tmps), begin(ctmps), end(ctmps));
    }

    return tmps;
}

QuantLib::ext::shared_ptr<EngineFactory> Analytic::Impl::engineFactory() {
    LOG("Analytic::engineFactory() called");
    // Note: Calling the constructor here with empty extry builders
    // Override this function in case you have got extra ones
    QuantLib::ext::shared_ptr<EngineData> edCopy = QuantLib::ext::make_shared<EngineData>(*inputs_->pricingEngine());
    edCopy->globalParameters()["GenerateAdditionalResults"] = to_string(generateAdditionalResults());
    edCopy->globalParameters()["RunType"] = "NPV";
    map<MarketContext, string> configurations;
    configurations[MarketContext::irCalibration] = inputs_->marketConfig("lgmcalibration");    
    configurations[MarketContext::fxCalibration] = inputs_->marketConfig("fxcalibration");
    configurations[MarketContext::pricing] = inputs_->marketConfig("pricing");
    LOG("MarketContext::pricing = " << inputs_->marketConfig("pricing"));
    return QuantLib::ext::make_shared<EngineFactory>(edCopy, analytic()->market(), configurations,
                                             inputs_->refDataManager(),
                                             *inputs_->iborFallbackConfig());
}

void Analytic::buildMarket(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                           const bool marketRequired) {
    LOG("Analytic::buildMarket called");    
    cpu_timer mtimer;

    QL_REQUIRE(loader, "market data loader not set");
    QL_REQUIRE(configurations().curveConfig, "curve configurations not set");
    
    // first build the market if we have a todaysMarketParams
    if (configurations().todaysMarketParams) {
        try {
            // imply bond spreads (no exclusion of securities in ore, just in ore+) and add results to loader
            auto bondSpreads = implyBondSpreads(configurations().asofDate, inputs_, configurations_.todaysMarketParams,
                                                loader, configurations_.curveConfig, std::string());

            // Join the loaders
            loader_ = QuantLib::ext::make_shared<CompositeLoader>(loader, bondSpreads);

            // Check that the loader has quotes
            QL_REQUIRE(loader_->hasQuotes(configurations().asofDate),
                       "There are no quotes available for date " << configurations().asofDate);
            // Build the market
            market_ = QuantLib::ext::make_shared<TodaysMarket>(
                configurations().asofDate, configurations().todaysMarketParams, loader_, configurations().curveConfig,
                inputs()->continueOnError(), true, inputs()->lazyMarketBuilding(), inputs()->refDataManager(), false,
                *inputs()->iborFallbackConfig());
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

void Analytic::marketCalibration(const QuantLib::ext::shared_ptr<MarketCalibrationReportBase>& mcr) {
    if (mcr)
        mcr->populateReport(market_, configurations().todaysMarketParams);
}

void Analytic::buildPortfolio() {
    QuantLib::ext::shared_ptr<Portfolio> tmp = portfolio_ ? portfolio_ : inputs()->portfolio();
        
    // create a new empty portfolio
    portfolio_ = QuantLib::ext::make_shared<Portfolio>(inputs()->buildFailedTrades());

    tmp->reset();
    // populate with trades
    for (const auto& [tradeId, trade] : tmp->trades())
        // If portfolio was already provided to the analytic, make sure to only process those given trades.
        portfolio()->add(trade);
    
    if (market_) {
        replaceTrades();

        LOG("Build the portfolio");
        QuantLib::ext::shared_ptr<EngineFactory> factory = impl()->engineFactory();
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
    const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader, 
    const std::set<std::string>& runTypes) {

    Settings::instance().evaluationDate() = inputs_->asof();
    ObservationMode::instance().setMode(inputs_->observationModel());

    CONSOLEW("Build Market");
    analytic()->buildMarket(loader);
    CONSOLE("OK");
}

QuantLib::ext::shared_ptr<Loader> implyBondSpreads(const Date& asof,
                                           const QuantLib::ext::shared_ptr<ore::analytics::InputParameters>& params,
                                           const QuantLib::ext::shared_ptr<TodaysMarketParameters>& todaysMarketParams,
                                           const QuantLib::ext::shared_ptr<Loader>& loader,
                                           const QuantLib::ext::shared_ptr<CurveConfigurations>& curveConfigs,
                                           const std::string& excludeRegex) {

    auto securities = BondSpreadImply::requiredSecurities(asof, todaysMarketParams, curveConfigs, *loader,
                                                          true, excludeRegex);

    if (!securities.empty()) {
        // always continue on error and always use lazy market building
        QuantLib::ext::shared_ptr<Market> market =
            QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParams, loader, curveConfigs, true, true, true,
                                             params->refDataManager(), false, *params->iborFallbackConfig());
        return BondSpreadImply::implyBondSpreads(securities, params->refDataManager(), market, params->pricingEngine(),
                                                 Market::defaultConfiguration, *params->iborFallbackConfig());
    } else {
        // no bonds that require a spread imply => return null ptr
        return QuantLib::ext::shared_ptr<Loader>();
    }
}

} // namespace analytics
} // namespace ore
