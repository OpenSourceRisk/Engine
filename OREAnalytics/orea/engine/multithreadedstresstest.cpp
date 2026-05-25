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

#include <orea/engine/multithreadedstresstest.hpp>
#include <orea/app/structuredanalyticserror.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <orea/scenario/clonescenariofactory.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/scenariowriter.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/clonedloader.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <future>

#ifdef ORE_MULTITHREADING_CPU_AFFINITY
#include <pthread.h>
#include <sched.h>
#endif

namespace ore {
namespace analytics {

namespace {

#ifdef ORE_MULTITHREADING_CPU_AFFINITY
std::vector<std::size_t> getCpuIds(std::size_t nThreads) {

    std::size_t nCPU = std::max(1U, std::thread::hardware_concurrency());
    WLOG("[MULTITHREADING] Number of CPUs found: " << nCPU);

    std::vector<std::size_t> result(nThreads);

    std::mt19937 gen{std::random_device{}()};
    std::vector<std::size_t> availableCpus;

    for (std::size_t i = 0; i < nThreads; ++i) {
        if (availableCpus.empty()) {
            availableCpus.resize(nCPU);
            std::iota(availableCpus.begin(), availableCpus.end(), 0);
        }
        std::uniform_int_distribution<> distrib(0, availableCpus.size() - 1);
        auto pos = std::next(availableCpus.begin(), distrib(gen));
        result[i] = *pos;
        availableCpus.erase(pos);
    }

    for (std::size_t i = 0; i < nThreads; ++i) {
        WLOG("[MULTITHREADING] Assigning thread " << i << " to CPU #" << result[i]);
    }
    return result;
}
#endif

} // namespace

MultiThreadedStressTest::MultiThreadedStressTest(
    const QuantLib::Size nThreads, const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
    const QuantLib::ext::shared_ptr<ore::data::Market>& market, const std::string& marketConfiguration,
    const QuantLib::ext::shared_ptr<ore::data::EngineData>& engineData,
    const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketData,
    const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressData,
    const ore::data::CurveConfigurations& curveConfigs, const ore::data::TodaysMarketParameters& todaysMarketParams,
    const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory,
    const QuantLib::ext::shared_ptr<ore::data::ReferenceDataManager>& referenceData,
    const QuantLib::ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig,
    const QuantLib::ext::shared_ptr<ore::data::Loader>& loader, bool continueOnError,
    const bool useAtParCouponsTrades)
    : nThreads_(nThreads), portfolio_(portfolio), market_(market), marketConfiguration_(marketConfiguration),
      engineData_(engineData), simMarketData_(simMarketData), stressData_(stressData), curveConfigs_(curveConfigs),
      todaysMarketParams_(todaysMarketParams), scenarioFactory_(scenarioFactory), referenceData_(referenceData),
      iborFallbackConfig_(iborFallbackConfig), loader_(loader), continueOnError_(continueOnError),
      useAtParCouponsTrades_(useAtParCouponsTrades) {

    QL_REQUIRE(nThreads_ > 0, "MultiThreadedStressTest: nThreads must be greater than 0");

    // check whether sessions are enabled, if not exit with an error

#ifndef QL_ENABLE_SESSIONS
    QL_FAIL("MultiThreadedStressTest requires a build with QL_ENABLE_SESSIONS = ON.");
#endif
}

void MultiThreadedStressTest::runStressTest(
    const QuantLib::ext::shared_ptr<ore::data::Report>& report,
    const QuantLib::ext::shared_ptr<ore::data::Report>& cfReport, const double threshold,
    const QuantLib::Size precision, const bool includePastCashflows,
    const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& scenarioReport) {

    boost::timer::cpu_timer timer;
    LOG("MultiThreadedStressTest::runStressTest() called");

    QuantLib::Date asof = market_->asofDate();
    std::string baseCcy = simMarketData_->baseCcy();

    // Capture thread-local settings from main thread
    auto includeTodaysCashFlows = QuantLib::Settings::instance().includeTodaysCashFlows();
    auto includeReferenceDateEvents = QuantLib::Settings::instance().includeReferenceDateEvents();

    // Build base scenario
    LOG("Building base scenario sim market");

    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket = QuantLib::ext::make_shared<ScenarioSimMarket>(
        market_, simMarketData_, marketConfiguration_, curveConfigs_, todaysMarketParams_, continueOnError_,
        stressData_->useSpreadedTermStructures(), false, false, iborFallbackConfig_, true);

    QuantLib::ext::shared_ptr<Scenario> baseScenario = simMarket->baseScenario();

    // Write scenario report in main thread to avoid conflicts
    if (scenarioReport) {
        LOG("Writing scenario report in main thread");

        auto scenFactory =
            scenarioFactory_ ? scenarioFactory_ : QuantLib::ext::make_shared<CloneScenarioFactory>(baseScenario);
        auto scenGen = QuantLib::ext::make_shared<StressScenarioGenerator>(
            stressData_, baseScenario, simMarketData_, simMarket, scenFactory, simMarket->baseScenarioAbsolute());

        ScenarioWriter writer(scenGen, scenarioReport, std::vector<RiskFactorKey>{}, false);
        for (Size i = 0; i < scenGen->samples(); ++i) {
            writer.next(asof);
        }
        scenarioReport->end();

        LOG("Scenario report written with " << scenGen->samples() << " scenarios");
    }

    // Determine total number of stress scenarios
    Size nScenarios = stressData_->data().size();
    LOG("Total number of stress scenarios: " << nScenarios);

    QL_REQUIRE(nScenarios > 0, "MultiThreadedStressTest: no stress scenarios defined");

    // Calculate effective thread count
    Size eff_nThreads = std::min(nScenarios, nThreads_);
    LOG("nThreads       = " << nThreads_);
    LOG("nScenarios     = " << nScenarios);
    LOG("eff nThreads   = " << eff_nThreads);

    QL_REQUIRE(eff_nThreads > 0, "effective threads are zero, this is not allowed.");

    // Clone loaders for each thread to populate fixings
    std::vector<QuantLib::ext::shared_ptr<ore::data::ClonedLoader>> loaders;
    if (loader_) {
        for (Size i = 0; i < eff_nThreads; ++i)
            loaders.push_back(QuantLib::ext::make_shared<ore::data::ClonedLoader>(asof, loader_));
    }

    // Split scenarios
    LOG("Splitting scenarios among threads");

    std::vector<std::pair<Size, Size>> scenarioRanges(eff_nThreads);
    Size scenariosPerThread = nScenarios / eff_nThreads;
    Size remainder = nScenarios % eff_nThreads;
    Size startIdx = 0;

    for (Size i = 0; i < eff_nThreads; ++i) {
        Size count = scenariosPerThread + (i < remainder ? 1 : 0);
        Size endIdx = startIdx + count;
        scenarioRanges[i] = std::make_pair(startIdx, endIdx);
        LOG("Thread #" << i << " handles scenarios [" << startIdx << ", " << endIdx << ") - count: " << count);
        startIdx = endIdx;
    }

    // Serialize portfolio to string for thread-safe distribution
    //    - portfolioAsString = portfolio_->toXMLString()
    LOG("Serializing portfolio for thread distribution");
    std::string portfolioAsString = portfolio_->toXMLString();

    // Result containers
    struct StressResultData {
        std::string tradeId;
        std::string scenarioLabel;
        QuantLib::Real baseNpv;
        QuantLib::Real scenarioNpv;
        QuantLib::Real sensitivity;
    };

    std::vector<std::vector<StressResultData>> miniResults(eff_nThreads);

    struct CashflowResultData {
        std::string tradeId;
        std::string scenarioLabel;
        std::string tradeType;
        Size cashflowNo;
        Size legNo;
        QuantLib::Date payDate;
        std::string flowType;
        QuantLib::Real amountBase;
        QuantLib::Real amountScen;
        std::string currency;
        QuantLib::Real couponBase;
        QuantLib::Real couponScen;
        QuantLib::Real accrual;
        QuantLib::Date accrualStartDate;
        QuantLib::Date accrualEndDate;
        QuantLib::Real accruedAmountBase;
        QuantLib::Real accruedAmountScen;
        QuantLib::Date fixingDate;
        QuantLib::Real fixingValueBase;
        QuantLib::Real fixingValueScen;
        QuantLib::Real notionalBase;
        QuantLib::Real notionalScen;
        QuantLib::Real discountFactorBase;
        QuantLib::Real discountFactorScen;
        QuantLib::Real presentValueBase;
        QuantLib::Real presentValueScen;
        QuantLib::Real fxRateLocalBaseBase;
        QuantLib::Real fxRateLocalBaseScen;
        QuantLib::Real presentValueBaseBase;
        QuantLib::Real presentValueBaseScen;
        std::string baseCurrency;
        QuantLib::Real floorStrike;
        QuantLib::Real capStrike;
        QuantLib::Real floorVolatilityBase;
        QuantLib::Real floorVolatilityScen;
        QuantLib::Real capVolatilityBase;
        QuantLib::Real capVolatilityScen;
        QuantLib::Real effectiveFloorVolatilityBase;
        QuantLib::Real effectiveFloorVolatilityScen;
        QuantLib::Real effectiveCapVolatilityBase;
        QuantLib::Real effectiveCapVolatilityScen;
    };

    std::vector<std::vector<CashflowResultData>> miniCfResults(eff_nThreads);

    auto progressIndicator =
        QuantLib::ext::make_shared<ore::analytics::MultiThreadedProgressIndicator>(this->progressIndicators());

    // 8. Get CPU IDs for affinity
    std::vector<std::size_t> cpuIds;
#ifdef ORE_MULTITHREADING_CPU_AFFINITY
    cpuIds = getCpuIds(eff_nThreads);
#endif

    // Prepare thread synchronization
    using resultType = int;
    std::vector<std::future<resultType>> results(eff_nThreads);
    std::vector<std::thread> jobs;

    // Create worker threads
    for (Size i = 0; i < eff_nThreads; ++i) {

        auto job = [this,
#ifdef ORE_MULTITHREADING_CPU_AFFINITY
                    &cpuIds,
#endif
                    asof, baseCcy, includeTodaysCashFlows, includeReferenceDateEvents, &loaders, &scenarioRanges,
                    &portfolioAsString, &miniResults, &miniCfResults, &progressIndicator,
                    includePastCashflows, &cfReport, threshold](int id) -> resultType {

#ifdef ORE_MULTITHREADING_CPU_AFFINITY

            // 9a. SET CPU AFFINITY (if enabled)
            pthread_t self = pthread_self();
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(cpuIds[id], &cpuset);
            if (int rc = pthread_setaffinity_np(self, sizeof(cpu_set_t), &cpuset)) {
                WLOG("[MULTITHREADING] Error while setting cpu affinity for thread " << id << " to cpu id " << cpuIds[id]
                                                                                     << ": got return code " << rc);
            } else {
                WLOG("[MULTITHREADING] Setting cpu affinity for thread " << id << " to cpu id " << cpuIds[id]
                                                                             << ", running on cpu " << sched_getcpu());
            }
    #endif

            QuantLib::Settings::instance().evaluationDate() = asof;
            QuantLib::Settings::instance().includeTodaysCashFlows() = includeTodaysCashFlows;
            QuantLib::Settings::instance().includeReferenceDateEvents() = includeReferenceDateEvents;

            LOG("Start stress test thread " << id);

            int rc;

            try {
                // Get scenario range for this thread
                Size startIdx = scenarioRanges[id].first;
                Size endIdx = scenarioRanges[id].second;
                Size nScenariosForThread = endIdx - startIdx + 1;

                LOG("Thread " << id << " processing scenarios [" << startIdx << ", " << endIdx << ")");

                // Build thread-local sim market and fixings
                QuantLib::ext::shared_ptr<ScenarioSimMarket> threadSimMarket =
                    QuantLib::ext::make_shared<ScenarioSimMarket>(
                        market_, simMarketData_, marketConfiguration_, curveConfigs_, todaysMarketParams_, continueOnError_,
                        stressData_->useSpreadedTermStructures(), false, false, iborFallbackConfig_, true);

                if (!loaders.empty()) {
                    ore::data::applyFixings(loaders[id]->loadFixings());
                }

                QuantLib::ext::shared_ptr<Scenario> threadBaseScenario = threadSimMarket->baseScenario();

                // Build thread-local scenarios
                auto threadStressData = QuantLib::ext::make_shared<StressTestScenarioData>();
                threadStressData->useSpreadedTermStructures() = stressData_->useSpreadedTermStructures();
                const auto& allScenarios = stressData_->data();
                std::vector<StressTestScenarioData::StressTestData> threadScenarios;
                for (Size j = startIdx; j < endIdx; ++j) {
                    threadScenarios.push_back(allScenarios[j]);
                }
                threadStressData->setData(threadScenarios);
                auto scenFactory = scenarioFactory_ ? scenarioFactory_
                                                    : QuantLib::ext::make_shared<CloneScenarioFactory>(threadBaseScenario);
                QuantLib::ext::shared_ptr<StressScenarioGenerator> scenarioGenerator =
                    QuantLib::ext::make_shared<StressScenarioGenerator>(threadStressData, threadBaseScenario,
                                                                        simMarketData_, threadSimMarket, scenFactory,
                                                                        threadSimMarket->baseScenarioAbsolute());

                threadSimMarket->scenarioGenerator() = scenarioGenerator;

                // Build portfolio
                auto threadPortfolio = QuantLib::ext::make_shared<ore::data::Portfolio>();
                threadPortfolio->fromXMLString(portfolioAsString);

                std::map<ore::data::MarketContext, std::string> configurations;
                configurations[ore::data::MarketContext::pricing] = marketConfiguration_;

                auto ed = QuantLib::ext::make_shared<ore::data::EngineData>(*engineData_);
                ed->globalParameters()["RunType"] = "Stress";

                QuantLib::ext::shared_ptr<ore::data::EngineFactory> factory =
                    QuantLib::ext::make_shared<ore::data::EngineFactory>(ed, threadSimMarket, configurations,
                                                                         referenceData_, iborFallbackConfig_);

                threadPortfolio->build(factory, "stress analysis", true, useAtParCouponsTrades_);

                // Results cube
                QuantLib::ext::shared_ptr<NPVCube> cube = QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(
                    asof, threadPortfolio->ids(), std::vector<QuantLib::Date>(1, asof), nScenariosForThread);

                std::vector<std::vector<std::vector<TradeCashflowReportData>>> cfCube;
                if (cfReport) {
                    cfCube = std::vector<std::vector<std::vector<TradeCashflowReportData>>>(
                        threadPortfolio->ids().size(),
                        std::vector<std::vector<TradeCashflowReportData>>(nScenariosForThread + 1));
                }

                // Valuation
                QuantLib::ext::shared_ptr<DateGrid> dg = QuantLib::ext::make_shared<DateGrid>("1,0W", NullCalendar());
                std::vector<QuantLib::ext::shared_ptr<ValuationCalculator>> calculators;
                calculators.push_back(QuantLib::ext::make_shared<NPVCalculator>(baseCcy));
                if (cfReport) {
                    calculators.push_back(
                        QuantLib::ext::make_shared<CashflowReportCalculator>(baseCcy, includePastCashflows, cfCube));
                }
                ValuationEngine engine(asof, dg, threadSimMarket, factory->modelBuilders());
                engine.registerProgressIndicator(progressIndicator);
                ValuationEngine::Errors errors;
                engine.buildCube(threadPortfolio, cube, calculators, ValuationEngine::ErrorPolicy::RemoveSample, true,
                                 nullptr, nullptr, {}, false, &errors);

                // Results
                for (auto const& [tradeId, trade] : threadPortfolio->trades()) {
                    auto index = cube->idsAndIndexes().find(tradeId);
                    if (index == cube->idsAndIndexes().end())
                        continue;
                    Real npv0 =
                        errors.t0.find(index->second) == errors.t0.end() ? cube->getT0(index->second, 0) : Null<Real>();

                    // if first thread, include scenario 0 which is the base scenario
                    Size jStart = id == 0 ? 0 : 1;
                    for (Size j = jStart; j < nScenariosForThread; ++j) {
                        const std::string& label = scenarioGenerator->scenarios()[j]->label();

                        Real npv = npv0 != Null<Real>() &&
                                           errors.samples.find(std::make_pair(index->second, j)) == errors.samples.end()
                                       ? cube->get(index->second, 0, j, 0)
                                       : Null<Real>();

                        Real sensitivity = (npv0 == Null<Real>() || npv == Null<Real>()) ? Null<Real>() : npv - npv0;

                        // Apply threshold filter
                        if (sensitivity != Null<Real>() &&
                            (std::fabs(sensitivity) > threshold || QuantLib::close_enough(sensitivity, threshold))) {
                            StressResultData result;
                            result.tradeId = tradeId;
                            result.scenarioLabel = label;
                            result.baseNpv = npv0;
                            result.scenarioNpv = npv;
                            result.sensitivity = sensitivity;
                            miniResults[id].push_back(result);
                        }
                    }

                    // Collect cashflow results if needed
                    if (cfReport) {
                        std::map<std::pair<Size, Size>, TradeCashflowReportData> baseCf;
                        for (auto const& t : cfCube[index->second][0])
                            baseCf[std::make_pair(t.legNo, t.cashflowNo)] = t;

                        for (Size j = 0; j < nScenariosForThread; ++j) {
                            const std::string& label = scenarioGenerator->scenarios()[j]->label();

                            std::map<std::pair<Size, Size>, TradeCashflowReportData> scenCf;
                            for (auto const& t : cfCube[index->second][j + 1])
                                scenCf[std::make_pair(t.legNo, t.cashflowNo)] = t;

                            for (auto const& [idx, t0] : baseCf) {
                                CashflowResultData cfResult;
                                cfResult.tradeId = tradeId;
                                cfResult.scenarioLabel = label;
                                cfResult.tradeType = trade->tradeType();
                                cfResult.cashflowNo = idx.second;
                                cfResult.legNo = idx.first;
                                cfResult.payDate = t0.payDate;
                                cfResult.flowType = t0.flowType;
                                cfResult.amountBase = t0.amount;
                                cfResult.currency = t0.currency;
                                cfResult.couponBase = t0.coupon;
                                cfResult.accrual = t0.accrual;
                                cfResult.accrualStartDate = t0.accrualStartDate;
                                cfResult.accrualEndDate = t0.accrualEndDate;
                                cfResult.accruedAmountBase = t0.accruedAmount;
                                cfResult.fixingDate = t0.fixingDate;
                                cfResult.fixingValueBase = t0.fixingValue;
                                cfResult.notionalBase = t0.notional;
                                cfResult.discountFactorBase = t0.discountFactor;
                                cfResult.presentValueBase = t0.presentValue;
                                cfResult.fxRateLocalBaseBase = t0.fxRateLocalBase;
                                cfResult.presentValueBaseBase = t0.presentValueBase;
                                cfResult.baseCurrency = t0.baseCurrency;
                                cfResult.floorStrike = t0.floorStrike;
                                cfResult.capStrike = t0.capStrike;
                                cfResult.floorVolatilityBase = t0.floorVolatility;
                                cfResult.capVolatilityBase = t0.capVolatility;
                                cfResult.effectiveFloorVolatilityBase = t0.effectiveFloorVolatility;
                                cfResult.effectiveCapVolatilityBase = t0.effectiveCapVolatility;

                                if (auto scen = scenCf.find(idx); scen != scenCf.end()) {
                                    cfResult.amountScen = scen->second.amount;
                                    cfResult.couponScen = scen->second.coupon;
                                    cfResult.accruedAmountScen = scen->second.accruedAmount;
                                    cfResult.fixingValueScen = scen->second.fixingValue;
                                    cfResult.notionalScen = scen->second.notional;
                                    cfResult.discountFactorScen = scen->second.discountFactor;
                                    cfResult.presentValueScen = scen->second.presentValue;
                                    cfResult.fxRateLocalBaseScen = scen->second.fxRateLocalBase;
                                    cfResult.presentValueBaseScen = scen->second.presentValueBase;
                                    cfResult.floorVolatilityScen = scen->second.floorVolatility;
                                    cfResult.capVolatilityScen = scen->second.capVolatility;
                                    cfResult.effectiveFloorVolatilityScen = scen->second.effectiveFloorVolatility;
                                    cfResult.effectiveCapVolatilityScen = scen->second.effectiveCapVolatility;
                                } else {
                                    cfResult.amountScen = Null<Real>();
                                    cfResult.couponScen = Null<Real>();
                                    cfResult.accruedAmountScen = Null<Real>();
                                    cfResult.fixingValueScen = Null<Real>();
                                    cfResult.notionalScen = Null<Real>();
                                    cfResult.discountFactorScen = Null<Real>();
                                    cfResult.presentValueScen = Null<Real>();
                                    cfResult.fxRateLocalBaseScen = Null<Real>();
                                    cfResult.presentValueBaseScen = Null<Real>();
                                    cfResult.floorVolatilityScen = Null<Real>();
                                    cfResult.capVolatilityScen = Null<Real>();
                                    cfResult.effectiveFloorVolatilityScen = Null<Real>();
                                    cfResult.effectiveCapVolatilityScen = Null<Real>();
                                }

                                miniCfResults[id].push_back(cfResult);
                            }
                        }
                    }
                }

                LOG("Thread " << id << " successfully finished.");
                rc = 0;
            } catch (const std::exception& e) {
                ore::analytics::StructuredAnalyticsErrorMessage("MultiThreadedStressTest: ", "", e.what()).log();
                rc = 1;
            }

            return rc;
        };
    
        // Launch thread
        std::packaged_task<resultType(int)> task(job);
        results[i] = task.get_future();
        std::thread thread(std::move(task), i);
        jobs.emplace_back(std::move(thread));
    }

    // Join results
    for (auto& t : jobs)
        t.join();

    for (Size i = 0; i < results.size(); ++i) {
        results[i].wait();
    }

    for (Size i = 0; i < results.size(); ++i) {
        QL_REQUIRE(results[i].valid(), "internal error: did not get a valid result");
        int rc = results[i].get();
        QL_REQUIRE(rc == 0, "error: thread " << i << " exited with return code " << rc
                                             << ". Check for structured errors from 'MultiThreaded Stress Test Engine'.");
    }

    LOG("Aggregating stress test results into final report");

    // Merge all thread results and sort by tradeId, then scenarioLabel
    std::vector<StressResultData> allResults;
    for (Size i = 0; i < eff_nThreads; ++i) {
        allResults.insert(allResults.end(), miniResults[i].begin(), miniResults[i].end());
    }
    std::sort(allResults.begin(), allResults.end(), [](const StressResultData& a, const StressResultData& b) {
        if (a.tradeId != b.tradeId)
            return a.tradeId < b.tradeId;
        return a.scenarioLabel < b.scenarioLabel;
    });

    report->addColumn("TradeId", std::string());
    report->addColumn("ScenarioLabel", std::string());
    report->addColumn("Base NPV", double(), precision);
    report->addColumn("Scenario NPV", double(), precision);
    report->addColumn("Sensitivity", double(), precision);

    for (const auto& result : allResults) {
        report->next();
        report->add(result.tradeId);
        report->add(result.scenarioLabel);
        report->add(result.baseNpv);
        report->add(result.scenarioNpv);
        report->add(result.sensitivity);
    }

    report->end();
    LOG("Stress NPV report completed with results from " << eff_nThreads << " threads");

    if (cfReport) {
        LOG("Aggregating stressed cashflow results into final report");

        // Helper lambda for null-safe difference
        auto diffWithNull = [](Real x, Real y) -> Real {
            if (x == Null<Real>() || y == Null<Real>())
                return Null<Real>();
            return x - y;
        };

        cfReport->addColumn("TradeId", std::string());
        cfReport->addColumn("ScenarioLabel", std::string());
        cfReport->addColumn("Type", std::string());
        cfReport->addColumn("CashflowNo", Size());
        cfReport->addColumn("LegNo", Size());
        cfReport->addColumn("PayDate", Date());
        cfReport->addColumn("FlowType", std::string());
        cfReport->addColumn("Amount_Base", double(), precision);
        cfReport->addColumn("Amount_Scen", double(), precision);
        cfReport->addColumn("Amount_Diff", double(), precision);
        cfReport->addColumn("Currency", std::string());
        cfReport->addColumn("Coupon_Base", double(), 10);
        cfReport->addColumn("Coupon_Scen", double(), 10);
        cfReport->addColumn("Coupon_Diff", double(), 10);
        cfReport->addColumn("Accrual", double(), 10);
        cfReport->addColumn("AccrualStartDate", Date(), 4);
        cfReport->addColumn("AccrualEndDate", Date(), 4);
        cfReport->addColumn("AccruedAmount_Base", double(), 4);
        cfReport->addColumn("AccruedAmount_Scen", double(), 4);
        cfReport->addColumn("AccruedAmount_Diff", double(), 4);
        cfReport->addColumn("fixingDate", Date());
        cfReport->addColumn("fixingValue_Base", double(), 10);
        cfReport->addColumn("fixingValue_Scen", double(), 10);
        cfReport->addColumn("fixingValue_Diff", double(), 10);
        cfReport->addColumn("Notional_Base", double(), 4);
        cfReport->addColumn("Notional_Scen", double(), 4);
        cfReport->addColumn("Notional_Diff", double(), 4);
        cfReport->addColumn("DiscountFactor_Base", double(), 10);
        cfReport->addColumn("DiscountFactor_Scen", double(), 10);
        cfReport->addColumn("PresentValue_Base", double(), 10);
        cfReport->addColumn("PresentValue_Scen", double(), 10);
        cfReport->addColumn("PresentValue_Diff", double(), 10);
        cfReport->addColumn("FXRate(Local-Base)_Base", double(), 10);
        cfReport->addColumn("FXRate(Local-Base)_Scen", double(), 10);
        cfReport->addColumn("PresentValue(Base)_Base", double(), 10);
        cfReport->addColumn("PresentValue(Base)_Scen", double(), 10);
        cfReport->addColumn("PresentValue(Base)_Diff", double(), 10);
        cfReport->addColumn("BaseCurrency", std::string());
        cfReport->addColumn("FloorStrike", double(), 6);
        cfReport->addColumn("CapStrike", double(), 6);
        cfReport->addColumn("FloorVolatility_Base", double(), 6);
        cfReport->addColumn("FloorVolatility_Scen", double(), 6);
        cfReport->addColumn("CapVolatility_Base", double(), 6);
        cfReport->addColumn("CapVolatility_Scen", double(), 6);
        cfReport->addColumn("EffectiveFloorVolatility_Base", double(), 6);
        cfReport->addColumn("EffectiveFloorVolatility_Scen", double(), 6);
        cfReport->addColumn("EffectiveCapVolatility_Base", double(), 6);
        cfReport->addColumn("EffectiveCapVolatility_Scen", double(), 6);

        std::vector<CashflowResultData> allCfResults;
        for (Size i = 0; i < eff_nThreads; ++i) {
            allCfResults.insert(allCfResults.end(), miniCfResults[i].begin(), miniCfResults[i].end());
        }
        std::sort(allCfResults.begin(), allCfResults.end(),
                  [](const CashflowResultData& a, const CashflowResultData& b) {
            if (a.tradeId != b.tradeId)
                return a.tradeId < b.tradeId;
            return a.scenarioLabel < b.scenarioLabel;
        });

        for (const auto& cf : allCfResults) {
            cfReport->next();
            cfReport->add(cf.tradeId);
            cfReport->add(cf.scenarioLabel);
            cfReport->add(cf.tradeType);
            cfReport->add(cf.cashflowNo);
            cfReport->add(cf.legNo);
            cfReport->add(cf.payDate);
            cfReport->add(cf.flowType);
            cfReport->add(cf.amountBase);
            cfReport->add(cf.amountScen);
            cfReport->add(diffWithNull(cf.amountScen, cf.amountBase));
            cfReport->add(cf.currency);
            cfReport->add(cf.couponBase);
            cfReport->add(cf.couponScen);
            cfReport->add(diffWithNull(cf.couponScen, cf.couponBase));
            cfReport->add(cf.accrual);
            cfReport->add(cf.accrualStartDate);
            cfReport->add(cf.accrualEndDate);
            cfReport->add(cf.accruedAmountBase);
            cfReport->add(cf.accruedAmountScen);
            cfReport->add(diffWithNull(cf.accruedAmountScen, cf.accruedAmountBase));
            cfReport->add(cf.fixingDate);
            cfReport->add(cf.fixingValueBase);
            cfReport->add(cf.fixingValueScen);
            cfReport->add(diffWithNull(cf.fixingValueScen, cf.fixingValueBase));
            cfReport->add(cf.notionalBase);
            cfReport->add(cf.notionalScen);
            cfReport->add(diffWithNull(cf.notionalScen, cf.notionalBase));
            cfReport->add(cf.discountFactorBase);
            cfReport->add(cf.discountFactorScen);
            cfReport->add(cf.presentValueBase);
            cfReport->add(cf.presentValueScen);
            cfReport->add(diffWithNull(cf.presentValueScen, cf.presentValueBase));
            cfReport->add(cf.fxRateLocalBaseBase);
            cfReport->add(cf.fxRateLocalBaseScen);
            cfReport->add(cf.presentValueBaseBase);
            cfReport->add(cf.presentValueBaseScen);
            cfReport->add(diffWithNull(cf.presentValueBaseScen, cf.presentValueBaseBase));
            cfReport->add(cf.baseCurrency);
            cfReport->add(cf.floorStrike);
            cfReport->add(cf.capStrike);
            cfReport->add(cf.floorVolatilityBase);
            cfReport->add(cf.floorVolatilityScen);
            cfReport->add(cf.capVolatilityBase);
            cfReport->add(cf.capVolatilityScen);
            cfReport->add(cf.effectiveFloorVolatilityBase);
            cfReport->add(cf.effectiveFloorVolatilityScen);
            cfReport->add(cf.effectiveCapVolatilityBase);
            cfReport->add(cf.effectiveCapVolatilityScen);
        }

        cfReport->end();
        LOG("Stressed cashflow report completed");
    }

    LOG("MultiThreadedStressTest::runStressTest() successfully finished, timings: "
        << static_cast<double>(timer.elapsed().wall) / 1.0E9 << "s Wall, "
        << static_cast<double>(timer.elapsed().user) / 1.0E9 << "s User, "
        << static_cast<double>(timer.elapsed().system) / 1.0E9 << "s System.");

    LOG("Stress testing done");
}

} // namespace analytics
} // namespace ore
