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

#include <boost/range/adaptor/indexed.hpp>

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

    // Build per-scenario maps for scenarios within the period
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
        // Aggregate PnL across trades and collapse duplicates by risk factor name
        std::unordered_map<std::string, std::pair<RiskFactorKey, Real>> byName;
        for (const auto& kv : mapCube_) {
            const RiskFactorKey& rfKey = kv.first;
            const ext::shared_ptr<NPVCube>& rfCube = kv.second;
            Real pnl = 0.0;
            Size tradeCount = rfCube->numIds();
            for (Size i = 0; i < tradeCount; ++i) {
                pnl += rfCube->get(i, dateIdx, s) - rfCube->getT0(i);
            }
            if (pnl == 0.0)
                continue;
            auto it = byName.find(rfKey.name);
            if (it == byName.end()) {
                byName.emplace(rfKey.name, std::make_pair(rfKey, pnl));
            } else {
                it->second.second += pnl;
            }
        }
        // Write one entry per risk factor name into the scenario map
        for (const auto& e : byName) {
            series[s].emplace(e.second.first, e.second.second);
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
