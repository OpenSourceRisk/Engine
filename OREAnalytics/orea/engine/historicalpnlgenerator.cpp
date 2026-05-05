/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/engine/historicalpnlgenerator.hpp>

#include <orea/engine/multithreadedvaluationengine.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>

#include <orea/cube/jointnpvcube.hpp>
#include <orea/cube/inmemorycube.hpp>

#include <orea/app/structuredanalyticserror.hpp>
#include <orea/engine/observationmode.hpp>
#include <ored/marketdata/clonedloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>

#include <boost/range/adaptor/indexed.hpp>

#include <future>
#include <limits>
#include <thread>

using ore::data::EngineBuilder;
using ore::data::EngineData;
using ore::data::EngineFactory;
using ore::data::MarketContext;
using ore::data::Portfolio;
using ore::data::TimePeriod;
using QuantLib::Real;
using QuantLib::io::iso_date;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

HistoricalPnlGenerator::HistoricalPnlGenerator(
    const string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
    const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen, const QuantLib::ext::shared_ptr<NPVCube>& cube,
    const set<std::pair<string, QuantLib::ext::shared_ptr<QuantExt::ModelBuilder>>>& modelBuilders, bool dryRun)
    : useSingleThreadedEngine_(true), portfolio_(portfolio), simMarket_(simMarket), hisScenGen_(hisScenGen),
      cube_(cube), dryRun_(dryRun),
      npvCalculator_([&baseCurrency]() -> std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>> {
          return {QuantLib::ext::make_shared<NPVCalculator>(baseCurrency)};
      }) {

    // Check the cube's dimensions
    // Possibly easier to create the cube here but then need to enforce a cube type or make a templated class.
    QL_REQUIRE(cube_->asof() == simMarket_->asofDate(),
               "The cube's as of date (" << iso_date(cube_->asof()) << ") should equal that of the simulation market ("
                                         << iso_date(simMarket_->asofDate()) << ")");
    
    std::set<std::string> cubeIds;
    for (const auto& [id, pos] : cube->idsAndIndexes()) {
        cubeIds.insert(id);
    }
    QL_REQUIRE(cubeIds == portfolio_->ids(), "The cube ids should equal the portfolio ids");
    QL_REQUIRE(cube_->samples() == hisScenGen_->numScenarios(),
               "The cube sample size (" << cube_->samples() << ") should equal the number of historical scenarios ("
                                        << hisScenGen_->numScenarios() << ")");
    QL_REQUIRE(cube_->numDates() == 1, "The cube should have exactly one date");
    QL_REQUIRE(cube_->depth() == 1, "The cube should have a depth of one");

    simMarket_->scenarioGenerator() = hisScenGen_;

    auto grid = QuantLib::ext::make_shared<DateGrid>();
    valuationEngine_ = QuantLib::ext::make_shared<ValuationEngine>(simMarket_->asofDate(), grid, simMarket_, modelBuilders);
}

HistoricalPnlGenerator::HistoricalPnlGenerator(
    const string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen, const QuantLib::ext::shared_ptr<EngineData>& engineData,
    const Size nThreads, const Date& today, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader,
    const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
    const QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams, const std::string& configuration,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketData,
    const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData,
    const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig,
    bool dryRun, const std::string& context)
    : useSingleThreadedEngine_(false), portfolio_(portfolio), hisScenGen_(hisScenGen), engineData_(engineData),
      nThreads_(nThreads), today_(today), loader_(loader), curveConfigs_(curveConfigs),
      todaysMarketParams_(todaysMarketParams), configuration_(configuration), simMarketData_(simMarketData),
      referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig), dryRun_(dryRun), context_(context),
      npvCalculator_([&baseCurrency]() -> std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>> {
          return {QuantLib::ext::make_shared<NPVCalculator>(baseCurrency)};
      }) {}

void HistoricalPnlGenerator::generateCube(const QuantLib::ext::shared_ptr<ScenarioFilter>& filter, const bool runRiskFactorBreakdown) {

    DLOG("Filling historical P&L cube for " << portfolio_->size() << " trades and " << hisScenGen_->numScenarios()
                                            << " scenarios.");

    if (useSingleThreadedEngine_) {

        valuationEngine_->unregisterAllProgressIndicators();
        for (auto const& i : this->progressIndicators()) {
            i->reset();
            valuationEngine_->registerProgressIndicator(i);
        }

        hisScenGen_->reset();
        simMarket_->filter() = filter;
        simMarket_->reset();
        simMarket_->scenarioGenerator() = hisScenGen_;
        hisScenGen_->baseScenario() = simMarket_->baseScenario();
        if(hisScenGen_->isRiskFactorBreakdown() && runRiskFactorBreakdown){
            //Run for RiskFactor PnL Breakdown
            ext::shared_ptr<Scenario> sc = hisScenGen_->baseScenario();
            std::vector<RiskFactorKey> deltaKeys = sc->keys();
            for(auto const& key: deltaKeys){
                // If we already have a non-empty shared_ptr for this key, skip rebuilding
                auto it = mapCube_.find(key);
                if (it != mapCube_.end() && it->second) {
                    continue;
                }
                hisScenGen_->setCurrentKey(key);
                hisScenGen_->setIterator(0);
                ext::shared_ptr<NPVCube> newCube = ext::make_shared<InMemoryCubeOpt<double>>(simMarket_->asofDate(), portfolio_->ids(),
                    vector<Date>(1, simMarket_->asofDate()), hisScenGen_->numScenarios());

                valuationEngine_->buildCube(portfolio_, newCube, npvCalculator_(), ValuationEngine::ErrorPolicy::RemoveAll, true,
                                        nullptr, nullptr, {}, dryRun_);
                mapCube_[key] = newCube;
                hisScenGen_->reset();
            }
            //Run for Full report - Remove risk factor key - reset counter
            hisScenGen_->setCurrentKey(RiskFactorKey());
            hisScenGen_->setIterator(0);
        }
        valuationEngine_->buildCube(portfolio_, cube_, npvCalculator_(), ValuationEngine::ErrorPolicy::RemoveAll, true,
                                    nullptr, nullptr, {}, dryRun_);
    } else {
        MultiThreadedValuationEngine engine(
            nThreads_, today_, QuantLib::ext::make_shared<ore::analytics::DateGrid>(), hisScenGen_->numScenarios(), loader_,
            hisScenGen_, engineData_, curveConfigs_, todaysMarketParams_, configuration_, simMarketData_, false, false,
            filter, referenceData_, iborFallbackConfig_, true, true, true, {}, {}, {}, context_);
        for (auto const& i : this->progressIndicators()) {
            i->reset();
            engine.registerProgressIndicator(i);
        }

        if (hisScenGen_->isRiskFactorBreakdown() && runRiskFactorBreakdown) {
            // Run for RiskFactor PnL Breakdown (multi-threaded)
            // Parallelise across risk factor keys rather than across trades.
            // Each thread builds its own market / portfolio / engine ONCE,
            // then iterates over a subset of risk factor keys. This avoids
            // the massive overhead of rebuilding everything per key.

            ext::shared_ptr<Scenario> sc = hisScenGen_->baseScenario();
            QL_REQUIRE(sc != nullptr,
                       "HistoricalPnlGenerator: base scenario must be set for multi-threaded risk factor breakdown");
            std::vector<RiskFactorKey> deltaKeys = sc->keys();

            // Collect keys that still need processing
            std::vector<RiskFactorKey> keysToProcess;
            for (auto const& key : deltaKeys) {
                auto it = mapCube_.find(key);
                if (it == mapCube_.end() || !it->second)
                    keysToProcess.push_back(key);
            }

            if (!keysToProcess.empty()) {
                Size eff_nThreads = std::min(keysToProcess.size(), nThreads_);

                // Partition keys across threads (round-robin)
                std::vector<std::vector<RiskFactorKey>> threadKeys(eff_nThreads);
                for (Size i = 0; i < keysToProcess.size(); ++i)
                    threadKeys[i % eff_nThreads].push_back(keysToProcess[i]);

                // Serialize portfolio for thread-safe deserialization in workers
                std::string portfolioXml = portfolio_->toXMLString();

                // Clone loaders for each thread
                std::vector<QuantLib::ext::shared_ptr<ore::data::ClonedLoader>> loaders;
                for (Size i = 0; i < eff_nThreads; ++i)
                    loaders.push_back(QuantLib::ext::make_shared<ore::data::ClonedLoader>(today_, loader_));

                // Capture QuantLib/ORE settings from main thread
                ore::analytics::ObservationMode::Mode obsMode = ore::analytics::ObservationMode::instance().mode();
                auto includeTodaysCashFlows = QuantLib::Settings::instance().includeTodaysCashFlows();
                auto localIncRefDateEvents = QuantLib::Settings::instance().includeReferenceDateEvents();

                // Capture hisScenGen properties for per-thread cloning
                auto hisScenLoader = hisScenGen_->scenarioLoader();
                auto scenFactory = hisScenGen_->scenarioFactory();
                auto retConfig = hisScenGen_->returnConfiguration();
                auto cal = hisScenGen_->cal();
                auto adjFactors = hisScenGen_->adjFactors();
                Size mporDays = hisScenGen_->mporDays();
                bool overlapping = hisScenGen_->overlapping();
                auto labelPrefix = hisScenGen_->labelPrefix();
                bool genDiffScen = hisScenGen_->generateDifferenceScenarios();
                Size numScenarios = hisScenGen_->numScenarios();

                // Per-thread result maps
                std::vector<std::map<RiskFactorKey, QuantLib::ext::shared_ptr<NPVCube>>> threadMaps(eff_nThreads);
                std::vector<std::future<int>> results(eff_nThreads);
                std::vector<std::thread> jobs;

                for (Size i = 0; i < eff_nThreads; ++i) {
                    auto job = [this, &threadKeys, &loaders, &portfolioXml, &threadMaps,
                                obsMode, includeTodaysCashFlows, localIncRefDateEvents,
                                hisScenLoader, scenFactory, retConfig, cal, adjFactors,
                                mporDays, overlapping, labelPrefix, genDiffScen, numScenarios,
                                &filter](int id) -> int {
                        // Set thread-local QuantLib settings
                        QuantLib::Settings::instance().evaluationDate() = today_;
                        QuantLib::Settings::instance().includeTodaysCashFlows() = includeTodaysCashFlows;
                        QuantLib::Settings::instance().includeReferenceDateEvents() = localIncRefDateEvents;
                        ore::analytics::ObservationMode::instance().setMode(obsMode);

                        LOG("Risk factor breakdown thread " << id << " started, processing "
                            << threadKeys[id].size() << " keys");

                        try {
                            // Build TodaysMarket (once per thread)
                            auto initMarket = QuantLib::ext::make_shared<ore::data::TodaysMarket>(
                                today_, todaysMarketParams_, loaders[id], curveConfigs_,
                                true, true, true, referenceData_, false, iborFallbackConfig_, false, true);

                            // Create per-thread HistoricalScenarioGenerator
                            auto localHisScenGen = QuantLib::ext::make_shared<HistoricalScenarioGenerator>(
                                hisScenLoader, scenFactory, retConfig, cal, adjFactors,
                                mporDays, overlapping, labelPrefix, genDiffScen, true);
                            localHisScenGen->setPopulateAllKeysOnBreakdown(true);

                            // Build ScenarioSimMarket (once per thread)
                            auto simMkt = QuantLib::ext::make_shared<ScenarioSimMarket>(
                                initMarket, simMarketData_, configuration_, *curveConfigs_,
                                *todaysMarketParams_, true, false, false, false,
                                iborFallbackConfig_, true);

                            // Link scenario generator and set filter
                            simMkt->scenarioGenerator() = localHisScenGen;
                            if (filter)
                                simMkt->filter() = filter;

                            // Set base scenario from sim market
                            localHisScenGen->baseScenario() = simMkt->baseScenario();

                            // Build portfolio (once per thread)
                            auto threadPortfolio = QuantLib::ext::make_shared<ore::data::Portfolio>();
                            threadPortfolio->fromXMLString(portfolioXml);
                            auto engFactory = QuantLib::ext::make_shared<ore::data::EngineFactory>(
                                engineData_, simMkt,
                                std::map<ore::data::MarketContext, string>(),
                                referenceData_, iborFallbackConfig_);
                            threadPortfolio->build(engFactory, context_, true);

                            // Create ValuationEngine (once per thread)
                            auto grid = QuantLib::ext::make_shared<ore::data::DateGrid>();
                            auto valEngine = QuantLib::ext::make_shared<ValuationEngine>(
                                simMkt->asofDate(), grid, simMkt, engFactory->modelBuilders());

                            // Process assigned risk factor keys
                            for (auto const& key : threadKeys[id]) {
                                localHisScenGen->setCurrentKey(key);
                                localHisScenGen->setIterator(0);

                                auto newCube = QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(
                                    simMkt->asofDate(), threadPortfolio->ids(),
                                    vector<Date>(1, simMkt->asofDate()), numScenarios);

                                valEngine->buildCube(threadPortfolio, newCube, npvCalculator_(),
                                                     ValuationEngine::ErrorPolicy::RemoveAll, true,
                                                     nullptr, nullptr, {}, dryRun_);

                                threadMaps[id][key] = newCube;
                                localHisScenGen->reset();
                            }

                            LOG("Risk factor breakdown thread " << id << " finished successfully.");
                            return 0;

                        } catch (const std::exception& e) {
                            ore::analytics::StructuredAnalyticsErrorMessage(
                                "Historical PnL Generator", "risk factor breakdown", e.what()).log();
                            return 1;
                        }
                    };

                    std::packaged_task<int(int)> task(job);
                    results[i] = task.get_future();
                    jobs.emplace_back(std::move(task), i);
                }

                // Wait for all threads
                for (auto& t : jobs)
                    t.join();
                for (Size i = 0; i < results.size(); ++i) {
                    results[i].wait();
                    QL_REQUIRE(results[i].get() == 0,
                               "error in risk factor breakdown thread " << i
                               << ". Check structured errors from 'Historical PnL Generator'.");
                }

                // Merge thread results into mapCube_
                for (Size i = 0; i < eff_nThreads; ++i) {
                    for (auto& [key, cube] : threadMaps[i])
                        mapCube_[key] = cube;
                }
            }

            // Run for Full report - Remove risk factor key - reset counter
            hisScenGen_->setCurrentKey(RiskFactorKey());
            hisScenGen_->setIterator(0);
        }
        for (auto const& i : this->progressIndicators()) {
            i->reset();
        }
        engine.buildCube(portfolio_, npvCalculator_, ValuationEngine::ErrorPolicy::RemoveAll, {}, true, dryRun_);
        cube_ = QuantLib::ext::make_shared<JointNPVCube>(engine.outputCubes(), portfolio_->ids(), true);
    }

    DLOG("Historical P&L cube generated");
}

vector<Real> HistoricalPnlGenerator::pnl(const TimePeriod& period, const set<pair<string, Size>>& tradeIds) const {

    // Create result with enough space
    vector<Real> pnls;
    pnls.reserve(cube_->samples());

    // Look up the date index once
    Size dateIdx = indexAsof();

    for (Size s = 0; s < cube_->samples(); ++s) {
        // Start and end of period relating to current sample
        Date start = hisScenGen_->startDates()[s];
        Date end = hisScenGen_->endDates()[s];

        if (period.contains(start) && period.contains(end)) {
            Real pnl = 0.0;
            for (const auto& tradeId : tradeIds) {
                pnl -= cube_->getT0(tradeId.second);
                pnl += cube_->get(tradeId.second, dateIdx, s);
            }
            pnls.push_back(pnl);
        }
    }

    pnls.shrink_to_fit();

    return pnls;
}

vector<Real> HistoricalPnlGenerator::pnl(const TimePeriod& period) const { return pnl(period, tradeIdIndexPairs()); }

vector<Real> HistoricalPnlGenerator::pnl(const set<pair<string, Size>>& tradeIds) const {
    return pnl(timePeriod(), tradeIds);
}

vector<Real> HistoricalPnlGenerator::pnl() const { return pnl(timePeriod()); }

using TradePnlStore = HistoricalPnlGenerator::TradePnlStore;

TradePnlStore HistoricalPnlGenerator::tradeLevelPnl(const TimePeriod& period,
                                                    const set<pair<string, Size>>& tradeIds) const {

    // Create result with enough space
    TradePnlStore pnls;
    pnls.reserve(cube_->samples());

    // Look up the date index once
    Size dateIdx = indexAsof();

    // Store t0 npvs on first pass
    vector<Real> t0Npvs;
    t0Npvs.reserve(tradeIds.size());

    for (Size s = 0; s < cube_->samples(); ++s) {

        // Start and end of period relating to current sample
        Date start = hisScenGen_->startDates()[s];
        Date end = hisScenGen_->endDates()[s];

        if (period.contains(start) && period.contains(end)) {

            // Store the t0 NPVs on first pass.
            if (t0Npvs.empty()) {
                for (const auto& p : tradeIds) {
                    t0Npvs.push_back(cube_->getT0(p.second));
                }
            }

            // Add vector to hold the trade level P&Ls
            pnls.push_back(vector<Real>(tradeIds.size(), 0.0));

            // Populate the trade level P&L vector
            for (const auto elem : tradeIds | boost::adaptors::indexed(0)) {
                auto idx = elem.index();
                pnls.back()[idx] = cube_->get(elem.value().second, dateIdx, s) - t0Npvs[idx];
            }
        }
    }

    pnls.shrink_to_fit();

    return pnls;
}

TradePnlStore HistoricalPnlGenerator::tradeLevelPnl(const ore::data::TimePeriod& period) const {
    return tradeLevelPnl(period, tradeIdIndexPairs());
}

TradePnlStore HistoricalPnlGenerator::tradeLevelPnl(const set<pair<string, Size>>& tradeIds) const {
    return tradeLevelPnl(timePeriod(), tradeIds);
}

TradePnlStore HistoricalPnlGenerator::tradeLevelPnl() const { return tradeLevelPnl(timePeriod()); }

using RiskFactorPnLSeries = HistoricalPnlGenerator::RiskFactorPnLSeries;
RiskFactorPnLSeries HistoricalPnlGenerator::riskFactorLevelPnlSeries(const ore::data::TimePeriod& period) const {
    RiskFactorPnLSeries series;
    if (mapCube_.empty()) {
        DLOG("riskFactorLevelPnlSeries: mapCube_ is empty; returning empty result");
        return series;
    }

    // Determine how many scenarios we have from any cube in the map
    Size samples = mapCube_.begin()->second->samples();
    series.resize(samples);

    Size dateIdx = 0; // risk factor cubes have a single date at asof
    for (Size s = 0; s < samples; ++s) {
        Date start = hisScenGen_->startDates()[s];
        Date end = hisScenGen_->endDates()[s];
        if (!(period.contains(start) && period.contains(end))) {
            continue; // leave this scenario's map empty
        }
        // Map each RiskFactorKey to its per-trade PnL vector
        std::map<RiskFactorKey, std::vector<Real>> byKey;
        for (const auto& kv : mapCube_) {
            const RiskFactorKey& rfKey = kv.first;
            const ext::shared_ptr<NPVCube>& rfCube = kv.second;
            Size tradeCount = rfCube->numIds();
            // Initialize with NaN to indicate no sensitivity
            std::vector<Real> pnlVec(tradeCount, std::numeric_limits<Real>::quiet_NaN());
            bool anyNonZero = false;
            for (Size i = 0; i < tradeCount; ++i) {
                Real v = rfCube->get(i, dateIdx, s) - rfCube->getT0(i);
                // Only store actual PnL values, leave NaN for no sensitivity
                if (v != 0.0) {
                    pnlVec[i] = v;
                    anyNonZero = true;
                }
            }
            if (!anyNonZero)
                continue;
            byKey.emplace(rfKey, std::move(pnlVec));
        }
        for (auto& e : byKey) {
            series[s].emplace(e.first, std::move(e.second));
        }
    }

    return series;
}

const QuantLib::ext::shared_ptr<NPVCube>& HistoricalPnlGenerator::cube() const { return cube_; }

set<pair<string, Size>> HistoricalPnlGenerator::tradeIdIndexPairs() const {
    set<pair<string, Size>> res;
    Size i = 0;
    for (const auto& [id, trade]: portfolio_->trades())
        res.insert(std::make_pair(id, i++));
    return res;
}

TimePeriod HistoricalPnlGenerator::timePeriod() const {
    vector<Date> dates{hisScenGen_->startDates().front(), hisScenGen_->endDates().back()};
    return TimePeriod(dates);
}

Size HistoricalPnlGenerator::indexAsof() const {
    Date asof = useSingleThreadedEngine_ ? simMarket_->asofDate() : today_;
    const auto& dates = cube_->dates();
    auto it = std::find(dates.begin(), dates.end(), asof);
    QL_REQUIRE(it != dates.end(), "Can't find an index for asof date " << asof << " in cube");
    return std::distance(dates.begin(), it);
}

} // namespace analytics
} // namespace ore
