/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/engine/marketriskbacktest.hpp>
#include <qle/math/stoplightbounds.hpp>
#include <ored/marketdata/todaysmarket.hpp>
#include <ored/report/report.hpp>
#include <ored/utilities/to_string.hpp>
#include <orea/cube/cubewriter.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <orea/engine/sensitivityaggregator.hpp>

using namespace QuantLib;
using namespace ore::data;
using namespace ore::analytics;

namespace ore {
namespace analytics {

const QuantLib::ext::shared_ptr<ore::data::Report>& MarketRiskBacktest::BacktestReports::get(ReportType type) {
    QL_REQUIRE(types_.size() == reports_.size(), "types and reports must be the same length");
    auto it = std::find(types_.begin(), types_.end(), type);
    if (it == types_.end()) {
        QL_FAIL("Cannot find report");
    } else {
        auto index = std::distance(types_.begin(), it);
        return reports_.at(index);
    }
}

MarketRiskBacktest::MarketRiskBacktest(
    const std::string& calculationCurrency,
    const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const std::string& portfolioFilter,
    std::unique_ptr<BacktestArgs> btArgs, 
    std::unique_ptr<SensiRunArgs> sensiArgs,
    std::unique_ptr<FullRevalArgs> revalArgs,
    std::unique_ptr<MultiThreadArgs> mtArgs,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
    const bool breakdown,
    const bool requireTradePnl)
    : MarketRiskReport(calculationCurrency, portfolio, portfolioFilter, btArgs->backtestPeriod_, hisScenGen, std::move(sensiArgs), std::move(revalArgs),
                       std::move(mtArgs), breakdown, requireTradePnl),
      btArgs_(std::move(btArgs)) {
}

void MarketRiskBacktest::initialise() {
    callTradeIds_ = btArgs_->callTradeIds_;
    postTradeIds_ = btArgs_->postTradeIds_;
    
    // If there is a mismatch between call and post, then we will have to exclude trade-level PnLs from the total (scenario) PnL
    requireTradePnl_ = callTradeIds_ != postTradeIds_;

    MarketRiskReport::initialise();
}


std::vector<ore::data::TimePeriod> MarketRiskBacktest::timePeriods() { 
    std::vector<TimePeriod> tps{btArgs_->benchmarkPeriod_, btArgs_->backtestPeriod_};
    return tps;
}

bool MarketRiskBacktest::runTradeDetail(const ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    auto rpts = ext::dynamic_pointer_cast<BacktestReports>(reports);
    bool trdDetail = false;
    if (rpts->has(BacktestReports::ReportType::DetailTrade)) {
        auto detailTrade = rpts->get(BacktestReports::ReportType::DetailTrade);
        trdDetail = detailTrade != nullptr;
    }
    return requireTradePnl_ || trdDetail;
}

void MarketRiskBacktest::addPnlCalculators(const ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    pnlCalculators_.push_back(
        QuantLib::ext::make_shared<PNLCalculator>(btArgs_->benchmarkPeriod_));
    auto btRpts = ext::dynamic_pointer_cast<BacktestReports>(reports);
    pnlCalculators_.push_back(
        QuantLib::ext::make_shared<BacktestPNLCalculator>(btArgs_->backtestPeriod_, writePnl_, this, btRpts));
}


void MarketRiskBacktest::handleSensiResults(const ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                            const ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                                            const ext::shared_ptr<TradeGroupBase>& tradeGroup) {
    QL_REQUIRE(pnlCalculators_.size() == 2, "Expecting 2 PNL Calculators for Backtest");
    bmSensiPnls_ = pnlCalculators_[0]->pnls();
    bmFoSensiPnls_ = pnlCalculators_[0]->foPnls();
    sensiPnls_ = pnlCalculators_[1]->pnls();
    foSensiPnls_ = pnlCalculators_[1]->foPnls();

    auto backtestPnlCalc = ext::dynamic_pointer_cast < BacktestPNLCalculator>(pnlCalculators_[1]);
    QL_REQUIRE(backtestPnlCalc, "We must have a BacktestPnLCalculator");
    if (runTradeDetail(reports)) {
        foTradePnls_ = backtestPnlCalc->foTradePnls();
        sensiTradePnls_ = backtestPnlCalc->tradePnls();
    }

    // Calculate benchmarks
    calculateBenchmarks(sensiCallBenchmarks_, btArgs_->confidence_, true, riskGroup, tradeIdIdxPairs_);
    calculateBenchmarks(sensiPostBenchmarks_, btArgs_->confidence_, false, riskGroup, tradeIdIdxPairs_);
}

void MarketRiskBacktest::handleFullRevalResults(const ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                                const ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup,
                                                const ext::shared_ptr<ore::analytics::TradeGroupBase>& tradeGroup) {

    QL_REQUIRE(histPnlGen_, "Must have a Historical PNL Generator");
    pnls_ = histPnlGen_->pnl(btArgs_->backtestPeriod_, tradeIdIdxPairs_);
    bmPnls_ = histPnlGen_->pnl(btArgs_->benchmarkPeriod_, tradeIdIdxPairs_);

    if (runTradeDetail(reports))
        tradePnls_ = histPnlGen_->tradeLevelPnl(btArgs_->backtestPeriod_, tradeIdIdxPairs_);

    adjustFullRevalPnls(pnls_, bmPnls_, tradePnls_, foSensiPnls_, bmFoSensiPnls_, foTradePnls_, riskGroup);

    calculateBenchmarks(fullRevalCallBenchmarks_, btArgs_->confidence_, true, riskGroup, tradeIdIdxPairs_);
    calculateBenchmarks(fullRevalPostBenchmarks_, btArgs_->confidence_, false, riskGroup, tradeIdIdxPairs_);
}

void MarketRiskBacktest::writeReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                      const ext::shared_ptr<ore::analytics::MarketRiskGroupBase>& riskGroup,
                                      const ext::shared_ptr<ore::analytics::TradeGroupBase>& tradeGroup) {

    // Data for the current backtest
    std::string cpty = tradeIdIdxPairs_.empty() ? "INVALID" : counterparty(tradeIdIdxPairs_.begin()->first);
    Data data = {cpty, tradeGroup, riskGroup};

    auto backtestRpts = ext::dynamic_pointer_cast<BacktestReports>(reports);
    QL_REQUIRE(backtestRpts, "We must have backtest reports");

    SummaryResults srSensi;
    bool runSensi = sensiPnls_.size() > 0;
    if (runSensi)
        // Give back summary results for sensitivity backtest
        srSensi = calculateSummary(backtestRpts, data, false, sensiPnls_, tradeIds_, sensiTradePnls_);
    
    if (runFullReval(riskGroup)) {
        // Give back summary results for full revaluation backtest
        SummaryResults srFull = calculateSummary(backtestRpts, data, true, pnls_, tradeIds_, tradePnls_);

        // Write the rows in the summary report
        addSummaryRow(backtestRpts, data, true, srFull.callValue, srFull.observations, true, srFull.callExceptions,
                      srFull.bounds, sensiCallBenchmarks_, fullRevalCallBenchmarks_);
        addSummaryRow(backtestRpts, data, false, srFull.postValue, srFull.observations, true, srFull.postExceptions,
                      srFull.bounds, sensiPostBenchmarks_, fullRevalPostBenchmarks_);
    }

     if (runSensi) {
        addSummaryRow(backtestRpts, data, true, srSensi.callValue, srSensi.observations, false, srSensi.callExceptions,
                      srSensi.bounds, sensiCallBenchmarks_, fullRevalCallBenchmarks_);
        addSummaryRow(backtestRpts, data, false, srSensi.postValue, srSensi.observations, false, srSensi.postExceptions,
                      srSensi.bounds, sensiPostBenchmarks_, fullRevalPostBenchmarks_);
    }
}

bool MarketRiskBacktest::disablesAll(const QuantLib::ext::shared_ptr<ScenarioFilter>& filter) const {
    // Return false if we hit any risk factor that is "allowed" i.e. enabled
    for (const auto& key : hisScenGen_->baseScenario()->keys()) {
        if (filter->allow(key)) {
            return false;
        }
    }
    // If we get to here, all risk factors are "not allowed" i.e. disabled
    return true;
}

MarketRiskBacktest::SummaryResults MarketRiskBacktest::calculateSummary(
    const ext::shared_ptr<BacktestReports>& reports, const Data& data, bool isFull, const vector<Real>& pnls, 
    const vector<string>& tradeIds, const PNLCalculator::TradePnLStore& tradePnls) {

    SummaryResults sr = {pnls.size(), 0.0, 0, 0.0, 0, {}};

    sr.callValue = callValue(data);
    sr.postValue = postValue(data);    

    auto pnlScenDates = hisScenGen_->filteredScenarioDates(btArgs_->backtestPeriod_);
    QL_REQUIRE(pnlScenDates.size() == pnls.size(), "Backtest::calculateSummary(): internal error, pnlScenDates ("
                                                       << pnlScenDates.size() << ") do not match pnls (" << pnls.size()
                                                       << ")");

    // If a trade level backtest detail report has been requested.
    bool detailTrd = runTradeDetail(reports);
    if (detailTrd)
        QL_REQUIRE(pnls.size() == tradePnls.size(), "For trade level backtest detail report,"
                                                        << " expect the number of aggregate P&Ls (" << pnls.size()
                                                        << ") to equal the number"
                                                        << " of trade P&Ls (" << tradePnls.size() << ").");

    // List of trades to exclude in the PnL calcs.
    // This would happen when a trade is under a winning reg in the call side but not on the post side, or vice versa.
    vector<Size> callTradesToSkip;
    vector<Size> postTradesToSkip;
    if (requireTradePnl_) {
        for (Size t = 0; t < tradeIds.size(); t++) {
            const string& tid = tradeIds[t];
            if (callTradeIds_.find(tid) == callTradeIds_.end())
                callTradesToSkip.push_back(t);
            if (postTradeIds_.find(tid) == postTradeIds_.end())
                postTradesToSkip.push_back(t);
        }
    }

    Real callScenPnl;
    Real postScenPnl;
    vector<Real> scenTradePnls;
    string cPassFail;
    string pPassFail;
    for (Size i = 0; i < pnls.size(); i++) {
        const auto& start = pnlScenDates[i].first;
        const auto& end = pnlScenDates[i].second;

        // Deal with call and write report row
        callScenPnl = pnls[i];
        if (!callTradesToSkip.empty()) {
            scenTradePnls = tradePnls[i];
            for (const Size& t : callTradesToSkip) {
                callScenPnl -= scenTradePnls[t];
            }
        }
        if (callScenPnl > std::max(sr.callValue, btArgs_->exceptionThreshold_))
            sr.callExceptions++;

        cPassFail = callScenPnl > std::max(sr.callValue, btArgs_->exceptionThreshold_) ? "fail" : "pass";
        addDetailRow(reports, data, true, sr.callValue, start, end, isFull, callScenPnl, cPassFail);

        // Deal with post and write report row
        postScenPnl = pnls[i];
        if (!postTradesToSkip.empty()) {
            scenTradePnls = tradePnls[i];
            for (const Size& t : postTradesToSkip) {
                postScenPnl -= scenTradePnls[t];
            }
        }
        if (-postScenPnl > std::max(sr.postValue, btArgs_->exceptionThreshold_))
            sr.postExceptions++;
        pPassFail = -postScenPnl > std::max(sr.postValue, btArgs_->exceptionThreshold_) ? "fail" : "pass";
        addDetailRow(reports, data, false, sr.postValue, start, end, isFull, -postScenPnl, pPassFail);

        // Add the trade level breakdown if requested. Note that we are clearly not recomputing the IM for the
        // trade - all we are doing is adding the P&L for each trade and the trade ID.
        if (detailTrd && !data.tradeGroup->allLevel()) {
            const auto& scenTradePnls = tradePnls[i];
            QL_REQUIRE(tradeIds.size() == scenTradePnls.size(), "For trade level backtest detail report,"
                                                                    << " the number of trades (" << tradeIds.size()
                                                                    << ") does not equal the size of the trade level P&L"
                                                                    << " container (" << scenTradePnls.size()
                                                                    << ") on scenario date " << io::iso_date(start)
                                                                    << ".");
            for (Size j = 0; j < scenTradePnls.size(); ++j) {
                if (std::find(callTradesToSkip.begin(), callTradesToSkip.end(), j) == callTradesToSkip.end())
                    addDetailRow(reports, data, true, sr.callValue, start, end, isFull, scenTradePnls[j], cPassFail, tradeIds[j]);
                if (std::find(postTradesToSkip.begin(), postTradesToSkip.end(), j) == postTradesToSkip.end())
                    addDetailRow(reports, data, false, sr.postValue, start, end, isFull, -scenTradePnls[j], pPassFail, tradeIds[j]);
            }
        }
    }

    LOG("Got " << sr.callExceptions << " Call exceptions from " << sr.observations << " observations.");
    LOG("Got " << sr.postExceptions << " Post exceptions from " << sr.observations << " observations.");

    // Now calculate the [red, amber] and [amber, green] bounds
    if (hisScenGen_->mporDays() != 10) {
        ALOG("SimmBacktest: MPOR days is " << hisScenGen_->mporDays());
    } else {
        sr.bounds = hisScenGen_->overlapping() ? QuantExt::stopLightBoundsTabulated(btArgs_->ragLevels_, sr.observations,
                                                   hisScenGen_->mporDays(), btArgs_->confidence_)
                        : QuantExt::stopLightBounds(btArgs_->ragLevels_, sr.observations, btArgs_->confidence_);
    }

    return sr;
}

void MarketRiskBacktest::createReports(const ext::shared_ptr<MarketRiskReport::Reports>& reports) {

    auto rpts = ext::dynamic_pointer_cast<BacktestReports>(reports);
    QL_REQUIRE(rpts, "Reports must be of type BacktestReports");

    if (rpts->has(BacktestReports::ReportType::Summary)) {
        auto summary = rpts->get(BacktestReports::ReportType::Summary);
        if (summary) {
            for (const auto& t : summaryColumns())
                summary->addColumn(std::get<0>(t), std::get<1>(t), std::get<2>(t));
        }
    }

    if (rpts->has(BacktestReports::ReportType::Detail)) {
        auto detail = rpts->get(BacktestReports::ReportType::Detail);
        if (detail) {
            for (const auto& t : detailColumns())
                detail->addColumn(std::get<0>(t), std::get<1>(t), std::get<2>(t));
        }
    }

    if (rpts->has(BacktestReports::ReportType::DetailTrade)) {
        auto detailTrade = rpts->get(BacktestReports::ReportType::DetailTrade);
        if (detailTrade) {
            detailTrade->addColumn("TradeId", string());
            for (const auto& t : detailColumns()) {
                if (btArgs_->tradeDetailIncludeAllColumns_ || std::get<3>(t))
                    detailTrade->addColumn(std::get<0>(t), std::get<1>(t), std::get<2>(t));
            }
        }
    }

    if (rpts->has(BacktestReports::ReportType::PnlContribution)) {
        auto pnl = rpts->get(BacktestReports::ReportType::PnlContribution);
        if (pnl) {
            for (const auto& t : pnlColumns())
                pnl->addColumn(std::get<0>(t), std::get<1>(t), std::get<2>(t));
        }
    }

    if (rpts->has(BacktestReports::ReportType::PnlContributionTrade)) {
        auto pnlTrade = rpts->get(BacktestReports::ReportType::PnlContributionTrade);
        if (pnlTrade) {
            pnlTrade->addColumn("TradeId", string());
            for (const auto& t : pnlColumns())
                pnlTrade->addColumn(std::get<0>(t), std::get<1>(t), std::get<2>(t));
        }
    }
}

void MarketRiskBacktest::calculateBenchmarks(
    VarBenchmarks& benchmarks, Real confidence, const bool isCall,
                                             const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    std::set<std::pair<std::string, QuantLib::Size>>&
        tradeIdIdxPairs) {    
    for (auto& [type, value] : benchmarks) {
        if (value.first)
            value.second = value.first->var(confidence, isCall, tradeIdIdxPairs);
    }
}

void MarketRiskBacktest::reset(const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup) {
    MarketRiskReport::reset(riskGroup);
    bmSensiPnls_.clear();
    pnls_.clear();
    bmPnls_.clear();
    foSensiPnls_.clear();
    bmFoSensiPnls_.clear();
    sensiPnls_.clear();
    tradePnls_.clear();
    sensiTradePnls_.clear();
    foTradePnls_.clear();
    for (auto& [type, value] : sensiCallBenchmarks_)
        value.second = 0.0;
    for (auto& [type, value] : sensiPostBenchmarks_)
        value.second = 0.0;
    for (auto& [type, value] : fullRevalCallBenchmarks_)
        value.second = 0.0;
    for (auto& [type, value] : fullRevalPostBenchmarks_)
        value.second = 0.0;
}

void MarketRiskBacktest::addPnlRow(const QuantLib::ext::shared_ptr<BacktestReports>& reports, Size scenarioIdx, 
    bool isCall, const RiskFactorKey& key_1, Real shift_1, Real delta, Real gamma, Real deltaPnl, Real gammaPnl, 
    const RiskFactorKey& key_2, Real shift_2, const string& tradeId, const string& currency, Real fxSpot) {

    // Do we have a report to write to?
    // Is this too slow to do on every call to addPnlRow? Need to find the report each time in any case.
    QuantLib::ext::shared_ptr<ore::data::Report> rpt;
    if (tradeId.empty()) {
        if (reports->has(BacktestReports::ReportType::PnlContribution))
            rpt = reports->get(BacktestReports::ReportType::PnlContribution);
    } else {
        if (reports->has(BacktestReports::ReportType::PnlContributionTrade))
            rpt = reports->get(BacktestReports::ReportType::PnlContributionTrade);
    }

    // If we have no report or below threshold, do nothing.
    if (!rpt ||
        (std::abs(deltaPnl) < sensiArgs_->pnlWriteThreshold_ && std::abs(gammaPnl) < sensiArgs_->pnlWriteThreshold_))
        return;

    // Add the trade ID if we are writing trade level entries.
    auto& report = tradeId.empty() ? rpt->next() : rpt->next().add(tradeId);

    // Add the fields common to the trade level and aggregate reports.
    report.add(hisScenGen_->startDates()[scenarioIdx])
        .add(hisScenGen_->endDates()[scenarioIdx])
        .add(isCall ? "call" : "post")
        .add(to_string(key_1))
        .add(to_string(key_2))
        .add(delta)
        .add(gamma)
        .add(shift_1)
        .add(shift_2)
        .add(currency.empty() || currency == calculationCurrency_ ? deltaPnl : deltaPnl / fxSpot)
        .add(currency.empty() || currency == calculationCurrency_ ? gammaPnl : gammaPnl / fxSpot)
        .add(currency.empty() ? calculationCurrency_ : currency);
}

void BacktestPNLCalculator::writePNL(Size scenarioIdx, bool isCall, const RiskFactorKey& key_1, Real shift_1,
                                         Real delta, Real gamma, Real deltaPnl, Real gammaPnl,
                                         const RiskFactorKey& key_2, Real shift_2, const string& tradeId) {
    if (writePnl_)
        backtest_->addPnlRow(reports_, scenarioIdx, isCall, key_1, shift_1, delta, gamma, deltaPnl, gammaPnl, key_2, shift_2,
                             tradeId);
}

} // namespace analytics
} // namespace ore
