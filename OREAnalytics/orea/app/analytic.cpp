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
#include <orea/app/analyticsmanager.hpp>
#include <orea/app/reportwriter.hpp>
#include <orea/app/marketdataloader.hpp>
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
#include <ored/utilities/indexparser.hpp>

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
         const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager,
         bool simulationConfig,
         bool sensitivityConfig,
         bool scenarioGeneratorConfig,
         bool crossAssetModelConfig)
    : impl_(std::move(impl)), types_(analyticTypes), inputs_(inputs), analyticsManager_(analyticsManager) {
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
}


Analytic::analytic_reports Analytic::reports() { 
    auto rpts = reports_;
    for (const auto& [key, a] : impl_->dependentAnalytics()) {
        if (a.second) {
            auto ar = a.first->reports();
            rpts.insert(ar.begin(), ar.end());
        }
	}
    return rpts;
}

void Analytic::runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                           const std::set<std::string>& runTypes) {
    MEM_LOG_USING_LEVEL(ORE_WARNING, "Starting " << label() << " Analytic::runAnalytic()");
    if (!analyticComplete_ && impl_) {
        if (!impl_->initialised())
            QL_FAIL("Analytic " + label() + " is not initialed.");
        impl_->runAnalytic(loader, runTypes);
        MEM_LOG_USING_LEVEL(ORE_WARNING, "Finishing " << label() << " Analytic::runAnalytic()")
    }
    analyticComplete_ = true;
}

void Analytic::initialise() {
    if (impl() && !impl()->initialised()) {
        impl()->initialise();
        buildConfigurations();
    }
}

void Analytic::Impl::initialise() {
    if (!initialised_) {
        buildDependencies();
        setUpConfigurations();
        for (const auto& [_, a] : dependentAnalytics_)
            a.first->initialise();
        initialised_ = true;
    }
}

std::vector<QuantLib::ext::shared_ptr<Analytic>> Analytic::Impl::allDependentAnalytics() const {
    std::vector<QuantLib::ext::shared_ptr<Analytic>> analytics;
    for (const auto& [_, a] : dependentAnalytics_) {
        analytics.push_back(a.first);
        auto das = a.first->allDependentAnalytics();
        analytics.insert(end(analytics), begin(das), end(das));
    }
    return analytics;
}

QuantLib::ext::shared_ptr<Analytic> Analytic::Impl::dependentAnalytic(const std::string& key) const {
    auto it = dependentAnalytics_.find(key);
    QL_REQUIRE(it != dependentAnalytics_.end(), "Could not find dependent Analytic " << key);
    return it->second.first;
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

const Timer& Analytic::getTimer() {

    // Make sure all dependent analytics' timers have been added to this analytic's timer
    for (const auto& [analyticLabel, analytic] : impl_->dependentAnalytics())
        timer_.addTimer(analyticLabel, analytic.first->getTimer());

    return timer_;
}

std::set<QuantLib::Date> Analytic::marketDates() const {
    std::set<QuantLib::Date> mds = {inputs_->asof()};
    auto addDates = impl_->additionalMarketDates();
    mds.insert(addDates.begin(), addDates.end());

    for (const auto& a : impl_->dependentAnalytics()) {
        addDates = a.second.first->impl()->additionalMarketDates();
        mds.insert(addDates.begin(), addDates.end());
    }
    return mds;
}

std::vector<QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>> Analytic::todaysMarketParams() {
    std::vector<QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>> tmps;
    if (configurations().todaysMarketParams)
        tmps.push_back(configurations().todaysMarketParams);

    for (const auto& a : impl_->dependentAnalytics()) {
        auto ctmps = a.second.first->todaysMarketParams();
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
    startTimer("buildMarket()");

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
                inputs()->continueOnError(), false, inputs()->lazyMarketBuilding(), inputs()->refDataManager(), false,
                *inputs()->iborFallbackConfig());
        } catch (const std::exception& e) {
            if (marketRequired) {
                stopTimer("buildMarket()");
                QL_FAIL("Failed to build market: " << e.what());
            }
            else
                WLOG("Failed to build market: " << e.what());
        }
    } else {
        ALOG("Skip building the market due to missing today's market parameters in configurations"); 
    }
    const bool returnTimer = true;
    boost::optional<cpu_timer> mTimer = stopTimer("buildMarket()", returnTimer);
    if (mTimer)
        LOG("Market Build time " << setprecision(2) << mTimer->format(default_places, "%w") << " sec");
}

void Analytic::marketCalibration(const QuantLib::ext::shared_ptr<MarketCalibrationReportBase>& mcr) {
    if (mcr)
        mcr->populateReport(market_, configurations().todaysMarketParams);
}

void Analytic::buildPortfolio(const bool emitStructuredError) {
    startTimer("buildPortfolio()");
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
        portfolio()->build(factory, "analytic/" + label(), emitStructuredError);

        // remove dates that will have matured
        Date maturityDate = inputs()->asof();
        if (inputs()->portfolioFilterDate() != Null<Date>())
            maturityDate = inputs()->portfolioFilterDate();

        LOG("Filter trades that expire before " << maturityDate);
        portfolio()->removeMatured(maturityDate);
    } else {
        ALOG("Skip building the portfolio, because market not set");
    }
    stopTimer("buildPortfolio()");
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
            QuantLib::ext::make_shared<TodaysMarket>(asof, todaysMarketParams, loader, curveConfigs, true, false, true,
                                             params->refDataManager(), false, *params->iborFallbackConfig());
        return BondSpreadImply::implyBondSpreads(securities, params->refDataManager(), market, params->pricingEngine(),
                                                 Market::defaultConfiguration, *params->iborFallbackConfig());
    } else {
        // no bonds that require a spread imply => return null ptr
        return QuantLib::ext::shared_ptr<Loader>();
    }
}

void Analytic::enrichIndexFixings(const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio) {

    if (!inputs()->enrichIndexFixings())
        return;

    startTimer("enrichIndexFixings()");
    QL_REQUIRE(portfolio, "portfolio cannot be empty");

    auto isFallbackFixingDateWithinLimit = [this](Date originalFixingDate, Date fallbackFixingDate) {
        if (fallbackFixingDate > originalFixingDate) {
            return (inputs()->ignoreFixingLead() == 0 ||
                    (fallbackFixingDate - originalFixingDate <= inputs()->ignoreFixingLead()));
        }
        if (fallbackFixingDate < originalFixingDate) {
            return (inputs()->ignoreFixingLag() == 0 ||
                    (originalFixingDate - fallbackFixingDate <= inputs()->ignoreFixingLag()));
        }
        return true;
    };

    auto compTs = [](const std::pair<Date,Real>& a, const std::pair<Date,Real>& b) {
        return a.first < b.first;
    };

    for (const auto& [index_name, dates] : portfolio->fixings(inputs()->asof())) {
        try {
            auto index = parseIndex(index_name);
            const auto& timeSeries = index->timeSeries();
            if (timeSeries.size() == 0)
                continue;

            vector<Date> datesToAdd;
            vector<Real> fixingsToAdd;

            for(const auto& [date, mandatory] : dates) {
                if (mandatory && date != inputs()->asof()) {
                    auto tmp = std::pair<Date,Real>(date, Null<Real>());

                    if (timeSeries[date] != Null<Real>())
                        continue;

                    Date fallbackDate;
                    Real fallbackFixing = Null<Real>();
                    if (date < timeSeries.firstDate()) {
                        fallbackDate = timeSeries.firstDate();
                    }
                    else if (date > timeSeries.lastDate()) {
                        fallbackDate = timeSeries.lastDate();
                    }
                    else {
                        auto iter = std::upper_bound(timeSeries.begin(), timeSeries.end(), tmp, compTs);
                        if (isFallbackFixingDateWithinLimit(date, (--iter)->first)) {
                            fallbackDate = iter->first;
                        } else {
                            fallbackDate = (++iter)->first;
                        }
                    }
                    if (!isFallbackFixingDateWithinLimit(date, fallbackDate)) {
                        continue;
                    }
                    fallbackFixing = timeSeries[fallbackDate];
                    if (fallbackFixing == Null<Real>()) {
                        continue;
                    }
                    datesToAdd.push_back(date);
                    fixingsToAdd.push_back(fallbackFixing);
                    StructuredFixingWarningMessage(
                        index->name(), date, "Missing fixing",
                        "Could not find required fixing ID. "
                        "Using fallback fixing on " + to_string(fallbackDate)).log();
                }
            }

            for(Size i = 0; i < datesToAdd.size(); ++i) {
                index->addFixing(datesToAdd[i], fixingsToAdd[i], false);
                DLOG("Added fallback fixing " << index->name() << " "
                     << datesToAdd[i] << " " << fixingsToAdd[i]);
            }
            DLOG("Added " << datesToAdd.size() << " fallback(s) fixing for " << index->name())
        }
        catch (const std::exception& e) {
            WLOG("Failed to enrich historical index fixings: " << e.what());
        }
    }

    stopTimer("enrichIndexFixings()");
}

} // namespace analytics
} // namespace ore
