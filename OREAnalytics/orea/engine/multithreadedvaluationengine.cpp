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

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/engine/observationmode.hpp>
#include <orea/scenario/clonedscenariogenerator.hpp>

#include <ored/marketdata/clonedloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/dategrid.hpp>

#include <boost/timer/timer.hpp>

#include <future>

// #include <ctpl_stl.h>

namespace ore {
namespace analytics {

using QuantLib::Size;

MultiThreadedValuationEngine::MultiThreadedValuationEngine(
    const Size nThreads, const QuantLib::Date& today, const QuantLib::ext::shared_ptr<ore::data::DateGrid>& dateGrid,
    const Size nSamples, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator>& scenarioGenerator,
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams, const std::string& configuration,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
    const bool useSpreadedTermStructures, const bool cacheSimData,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioFilter>& scenarioFilter,
    const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData,
    const ore::data::IborFallbackConfig& iborFallbackConfig, const bool handlePseudoCurrenciesTodaysMarket,
    const bool handlePseudoCurrenciesSimMarket, const bool recalibrateModels,
    const std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                                   const std::vector<QuantLib::Date>&,
                                                                   const QuantLib::Size)>& cubeFactory,
    const std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(
        const QuantLib::Date&, const std::vector<QuantLib::Date>&, const QuantLib::Size)>& nettingSetCubeFactory,
    const std::function<QuantLib::ext::shared_ptr<ore::analytics::NPVCube>(const QuantLib::Date&, const std::set<std::string>&,
                                                                   const std::vector<QuantLib::Date>&,
                                                                   const QuantLib::Size)>& cptyCubeFactory,
    const std::string& context, const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& offSetScenario)
    : nThreads_(nThreads), today_(today), dateGrid_(dateGrid), nSamples_(nSamples), loader_(loader),
      scenarioGenerator_(scenarioGenerator), engineData_(engineData), curveConfigs_(curveConfigs),
      todaysMarketParams_(todaysMarketParams), configuration_(configuration), simMarketData_(simMarketData),
      useSpreadedTermStructures_(useSpreadedTermStructures), cacheSimData_(cacheSimData),
      scenarioFilter_(scenarioFilter), referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig),
      handlePseudoCurrenciesTodaysMarket_(handlePseudoCurrenciesTodaysMarket),
      handlePseudoCurrenciesSimMarket_(handlePseudoCurrenciesSimMarket), recalibrateModels_(recalibrateModels),
      cubeFactory_(cubeFactory), nettingSetCubeFactory_(nettingSetCubeFactory), cptyCubeFactory_(cptyCubeFactory),
      context_(context), offsetScenario_(offSetScenario){

    QL_REQUIRE(nThreads_ != 0, "MultiThreadedValuationEngine: nThreads must be > 0");

    // check whether sessions are enabled, if not exit with an error

#ifndef QL_ENABLE_SESSIONS
    QL_FAIL("MultiThreadedValuationEngine requires a build with QL_ENABLE_SESSIONS = ON.");
#endif

    // if no cube factory is given, create a default one

    if (!cubeFactory_)
        cubeFactory_ = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                          const std::vector<QuantLib::Date>& dates, const Size samples) {
            return QuantLib::ext::make_shared<ore::analytics::DoublePrecisionInMemoryCube>(asof, ids, dates, samples);
        };

    if (!nettingSetCubeFactory_)
        nettingSetCubeFactory_ = [](const QuantLib::Date& asof, const std::vector<QuantLib::Date>& dates,
                                    const Size samples) { return nullptr; };

    if (!cptyCubeFactory_)
        cptyCubeFactory_ = [](const QuantLib::Date& asof, const std::set<std::string>& ids,
                              const std::vector<QuantLib::Date>& dates, const Size samples) { return nullptr; };
}

void MultiThreadedValuationEngine::setAggregationScenarioData(
    const QuantLib::ext::shared_ptr<AggregationScenarioData>& aggregationScenarioData) {
    aggregationScenarioData_ = aggregationScenarioData;
}

void MultiThreadedValuationEngine::buildCube(
    const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
    const std::function<std::vector<QuantLib::ext::shared_ptr<ore::analytics::ValuationCalculator>>()>& calculators,
    const std::function<std::vector<QuantLib::ext::shared_ptr<ore::analytics::CounterpartyCalculator>>()>& cptyCalculators,
    bool mporStickyDate, bool dryRun) {

    boost::timer::cpu_timer timer;

    LOG("MultiThreadedValuationEngine::buildCube() was called");

    // extract pricing stats accumulated so far and clear them

    LOG("Extract pricing stats and clear them in the current portfolio");

    std::map<std::string, std::pair<std::size_t, boost::timer::nanosecond_type>> pricingStats;
    for (auto const& [tid, t] : portfolio->trades())
        pricingStats[tid] = std::make_pair(t->getNumberOfPricings(), t->getCumulativePricingTime());

    // build portfolio against init market and trigger single pricing to generate pricing stats

    LOG("Reset and build portfolio against init market to produce pricing stats from a single pricing. Using pricing "
        "configuration '"
        << configuration_ << "'.");

    QuantLib::ext::shared_ptr<ore::data::Market> initMarket = QuantLib::ext::make_shared<ore::data::TodaysMarket>(
        today_, todaysMarketParams_, loader_, curveConfigs_, true, true, true, referenceData_, false,
        iborFallbackConfig_, false, handlePseudoCurrenciesTodaysMarket_);

    auto engineFactory = QuantLib::ext::make_shared<ore::data::EngineFactory>(
        engineData_, initMarket,
        std::map<ore::data::MarketContext, string>{{ore::data::MarketContext::pricing, configuration_}}, referenceData_,
        iborFallbackConfig_);

    portfolio->build(engineFactory, context_, true);

    for (auto const& [tid, t] : portfolio->trades()) {
        TLOG("got npv for " << tid << ": " << std::setprecision(12) << t->instrument()->NPV() << " "
                            << t->npvCurrency());
    }

    // split portfolio into nThreads parts such that each part has an approximately similar total avg pricing time

    Size eff_nThreads = std::min(portfolio->size(), nThreads_);

    LOG("Splitting portfolio.");

    LOG("portfolio size = " << portfolio->size());
    LOG("nThreads       = " << nThreads_);
    LOG("eff nThreads   = " << eff_nThreads);

    QL_REQUIRE(eff_nThreads > 0, "effective threads are zero, this is not allowed.");

    std::vector<QuantLib::ext::shared_ptr<ore::data::Portfolio>> portfolios;
    for (Size i = 0; i < eff_nThreads; ++i)
        portfolios.push_back(QuantLib::ext::make_shared<ore::data::Portfolio>());

    double totalAvgPricingTime = 0.0;
    std::vector<std::pair<std::string, double>> timings;
    for (auto const& [tid, t] : portfolio->trades()) {
        if (t->getNumberOfPricings() != 0) {
            double dt = t->getCumulativePricingTime() / static_cast<double>(t->getNumberOfPricings());
            timings.push_back(std::make_pair(tid, dt));
            totalAvgPricingTime += dt;
        } else {
            // trade might be a failed trade
            timings.push_back(std::make_pair(tid, 0.0));
        }
    }

    std::sort(timings.begin(), timings.end(),
              [](const std::pair<std::string, double>& p1, const std::pair<std::string, double> p2) {
                  if (p1.second == p2.second)
                      return p1.first < p2.first;
                  else
                      return p1.second > p2.second;
              });

    std::vector<double> portfolioTotalAvgPricingTime(portfolios.size());
    Size portfolioIndex = 0;
    for (auto const& t : timings) {
        portfolios[portfolioIndex]->add(portfolio->get(t.first));
        portfolioTotalAvgPricingTime[portfolioIndex] += t.second;
        if (++portfolioIndex >= eff_nThreads)
            portfolioIndex = 0;
    }

    // output the portfolios into strings so that the worker threads can load them from there

    std::vector<std::string> portfoliosAsString;
    for (auto const& p : portfolios) {
        portfoliosAsString.emplace_back(p->toXMLString());
    }

    // log info on the portfolio split

    LOG("Total avg pricing time     : " << totalAvgPricingTime / 1E6 << " ms");
    for (Size i = 0; i < eff_nThreads; ++i) {
        LOG("Portfolio #" << i << " number of trades       : " << portfolios[i]->size());
        LOG("Portfolio #" << i << " total avg pricing time : " << portfolioTotalAvgPricingTime[i] / 1E6 << " ms");
    }

    // build scenario generators for each thread as clones of the original one

    LOG("Cloning scenario generators for " << eff_nThreads << " threads...");
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::ScenarioGenerator>> scenarioGenerators;
    auto tmp =
        QuantLib::ext::make_shared<ore::analytics::ClonedScenarioGenerator>(scenarioGenerator_, dateGrid_->dates(), nSamples_);
    scenarioGenerators.push_back(tmp);
    DLOG("generator for thread 1 cloned.");
    for (Size i = 1; i < eff_nThreads; ++i) {
        scenarioGenerators.push_back(QuantLib::ext::make_shared<ore::analytics::ClonedScenarioGenerator>(*tmp));
        DLOG("generator for thread " << (i + 1) << " cloned.");
    }

    // build loaders for each thread as clones of the original one

    LOG("Cloning loaders for " << eff_nThreads << " threads...");
    std::vector<QuantLib::ext::shared_ptr<ore::data::ClonedLoader>> loaders;
    for (Size i = 0; i < eff_nThreads; ++i)
        loaders.push_back(QuantLib::ext::make_shared<ore::data::ClonedLoader>(today_, loader_));

    // build nThreads mini-cubes to which each thread writes its results

    LOG("Build " << eff_nThreads << " mini result cubes...");
    miniCubes_.clear();
    miniNettingSetCubes_.clear();
    miniCptyCubes_.clear();
    for (Size i = 0; i < eff_nThreads; ++i) {
        miniCubes_.push_back(cubeFactory_(today_, portfolios[i]->ids(), dateGrid_->valuationDates(), nSamples_));
        miniNettingSetCubes_.push_back(nettingSetCubeFactory_(today_, dateGrid_->valuationDates(), nSamples_));
        miniCptyCubes_.push_back(
            cptyCubeFactory_(today_, portfolios[i]->counterparties(), dateGrid_->valuationDates(), nSamples_));
    }

    // build progress indicator consolidating the results from the threads

    auto progressIndicator =
        QuantLib::ext::make_shared<ore::analytics::MultiThreadedProgressIndicator>(this->progressIndicators());

    // create the thread pool with eff_nThreads and queue size = eff_nThreads as well

    // LOG("Create thread pool with " << eff_nThreads);
    // ctpl::thread_pool threadPool(eff_nThreads);

    // create the jobs and push them to the pool

    using resultType = int;
    std::vector<std::future<resultType>> results(eff_nThreads);

    std::vector<std::thread> jobs; // not needed if thread pool is used

    // pricing stats accumulated in worker threads
    std::vector<std::map<std::string, std::pair<std::size_t, boost::timer::nanosecond_type>>> workerPricingStats(
        eff_nThreads);

    // get obs mode of main thread, so that we can set this mode in the worker threads below
    ore::analytics::ObservationMode::Mode obsMode = ore::analytics::ObservationMode::instance().mode();

    for (Size i = 0; i < eff_nThreads; ++i) {

        auto job = [this, obsMode, dryRun, &calculators, &cptyCalculators, mporStickyDate, &portfoliosAsString,
                    &scenarioGenerators, &loaders, &workerPricingStats, &progressIndicator](int id) -> resultType {
            // set thread local singletons

            QuantLib::Settings::instance().evaluationDate() = today_;
            ore::analytics::ObservationMode::instance().setMode(obsMode);

            LOG("Start thread " << id);

            int rc;

            try {

                // build todays market using cloned market data

                QuantLib::ext::shared_ptr<ore::data::Market> initMarket = QuantLib::ext::make_shared<ore::data::TodaysMarket>(
                    today_, todaysMarketParams_, loaders[id], curveConfigs_, true, true, true, referenceData_, false,
                    iborFallbackConfig_, false, handlePseudoCurrenciesTodaysMarket_);

                // build sim market

                QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket> simMarket =
                    QuantLib::ext::make_shared<ore::analytics::ScenarioSimMarket>(
                        initMarket, simMarketData_, configuration_, *curveConfigs_, *todaysMarketParams_, true,
                        useSpreadedTermStructures_, cacheSimData_, false, iborFallbackConfig_,
                        handlePseudoCurrenciesSimMarket_, offsetScenario_);

                // set aggregation scenario data, but only in one of the sim markets, that's sufficient to populate it

                if (id == 0 && aggregationScenarioData_ != nullptr)
                    simMarket->aggregationScenarioData() = aggregationScenarioData_;

                // link scenario generator to sim market

                simMarket->scenarioGenerator() = scenarioGenerators[id];

                // set scenario filter

                if (scenarioFilter_)
                    simMarket->filter() = scenarioFilter_;

                // build portfolio against sim market

                auto portfolio = QuantLib::ext::make_shared<ore::data::Portfolio>();
                portfolio->fromXMLString(portfoliosAsString[id]);
                auto engineFactory = QuantLib::ext::make_shared<ore::data::EngineFactory>(
                    engineData_, simMarket, std::map<ore::data::MarketContext, string>(), referenceData_,
                    iborFallbackConfig_);

                portfolio->build(engineFactory, context_, true);

                // build valuation engine

                auto valEngine = QuantLib::ext::make_shared<ore::analytics::ValuationEngine>(
                    today_, dateGrid_, simMarket,
                    recalibrateModels_ ? engineFactory->modelBuilders()
                                       : std::set<std::pair<std::string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>());
                valEngine->registerProgressIndicator(progressIndicator);

                // build mini-cube

                valEngine->buildCube(portfolio, miniCubes_[id], calculators(), mporStickyDate, miniNettingSetCubes_[id],
                                     miniCptyCubes_[id],
                                     cptyCalculators ? cptyCalculators()
                                                     : std::vector<QuantLib::ext::shared_ptr<CounterpartyCalculator>>(),
                                     dryRun);

                // set pricing stats for val engine run

                for (auto const& [tid, t] : portfolio->trades())
                    workerPricingStats[id][tid] =
                        std::make_pair(t->getNumberOfPricings(), t->getCumulativePricingTime());

                // return code 0 = ok

                LOG("Thread " << id << " successfully finished.");

                rc = 0;

            } catch (const std::exception& e) {

                // log error and return code 1 = not ok

                ore::analytics::StructuredAnalyticsErrorMessage("Multithreaded Valuation Engine", "", e.what()).log();
                rc = 1;
            }

            // exit

            return rc;
        };

        // results[i] = threadPool.push(job);

        // not needed if thread pool is used
        std::packaged_task<resultType(int)> task(job);
        results[i] = task.get_future();
        std::thread thread(std::move(task), i);
        jobs.emplace_back(std::move(thread));
    }

    // check return codes from jobs

    // not needed if thread pool is used
    for (auto& t : jobs)
        t.join();

    for (Size i = 0; i < results.size(); ++i) {
        results[i].wait();
    }

    for (Size i = 0; i < results.size(); ++i) {
        QL_REQUIRE(results[i].valid(), "internal error: did not get a valid result");
        int rc = results[i].get();
        QL_REQUIRE(rc == 0, "error: thread " << i << " exited with return code " << rc
                                             << ". Check for structured errors from 'MultiThreaded Valuation Engine'.");
    }

    // stop the thread pool, wait for unfinished jobs

    // LOG("Stop thread pool");
    // threadPool.stop(true);

    // set updated pricing stats in original portfolio

    LOG("Update pricing stats of trades.");

    for (auto const& [tid, t] : portfolio->trades()) {
        auto p = pricingStats[tid];
        std::size_t n = p.first;
        boost::timer::nanosecond_type d = p.second;
        for (auto const& w : workerPricingStats) {
            auto p = w.find(tid);
            if (p != w.end()) {
                n += p->second.first;
                d += p->second.second;
            }
        }
        t->resetPricingStats(n, d);
    }

    // log timings and return the result mini-cubes

    LOG("MultiThreadedValuationEngine::buildCube() successfully finished, timings: "
        << static_cast<double>(timer.elapsed().wall) / 1.0E9 << "s Wall, "
        << static_cast<double>(timer.elapsed().user) / 1.0E9 << "s User, "
        << static_cast<double>(timer.elapsed().system) / 1.0E9 << "s System.");
}

} // namespace analytics
} // namespace ore
