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

#include <orea/engine/historicalsimulationvar.hpp>
#include <orea/engine/historicalpnlgenerator.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <orea/cube/cube_io.hpp>
#include <orea/cube/inmemorycube.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/tail_quantile.hpp>

using namespace boost::accumulators;
using namespace ore::data;
using namespace QuantLib;

namespace ore {
namespace analytics {

HistoricalSimulationVarReport::HistoricalSimulationVarReport(
    const string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const string& portfolioFilter,
    const vector<Real>& p, QuantLib::ext::optional<TimePeriod> period,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen, std::unique_ptr<FullRevalArgs> fullRevalArgs, std::unique_ptr<MultiThreadArgs> multiThreadArgs,
    const bool breakdown, const bool includeExpectedShortfall, const bool tradePnl, const bool riskFactorBreakdown, const bool useAtParCouponsCurves,
    const bool useAtParCouponsTrades, const bool riskClassBreakdown, const bool includeTheta)
    : VarReport(baseCurrency, portfolio, portfolioFilter, p, period, hisScenGen, nullptr, std::move(fullRevalArgs),
                std::move(multiThreadArgs), false, useAtParCouponsCurves, useAtParCouponsTrades, tradePnl, riskFactorBreakdown,
                riskClassBreakdown),
      includeExpectedShortfall_(includeExpectedShortfall),
      tradePnl_(tradePnl),
      riskFactorBreakdown_(riskFactorBreakdown),
      includeTheta_(includeTheta) {
    fullReval_ = true;
}

HistoricalSimulationVarReport::HistoricalSimulationVarReport(
    const string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const string& portfolioFilter,
    const vector<Real>& p, QuantLib::ext::optional<TimePeriod> period,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen, std::unique_ptr<SensiRunArgs> sensiArgs,
    const bool breakdown, const bool includeExpectedShortfall, const bool tradePnl, const bool riskFactorBreakdown,
    const bool useAtParCouponsCurves, const bool useAtParCouponsTrades, const bool riskClassBreakdown)
    : VarReport(baseCurrency, portfolio, portfolioFilter, p, period, hisScenGen, std::move(sensiArgs), nullptr, nullptr,
                breakdown, useAtParCouponsCurves, useAtParCouponsTrades, tradePnl, riskFactorBreakdown,
                riskClassBreakdown),
      includeExpectedShortfall_(includeExpectedShortfall),
      tradePnl_(tradePnl),
      riskFactorBreakdown_(riskFactorBreakdown),
      includeTheta_(false) {
    fullReval_ = false;
    sensiBased_ = true;
    requireTradePnl_ = tradePnl_;
    requireRiskFactorPnl_ = riskFactorBreakdown_;
}

void HistoricalSimulationVarReport::createVarCalculator() {
    varCalculator_ = QuantLib::ext::make_shared<HistoricalSimulationVarCalculator>(pnls_);
}

void HistoricalSimulationVarReport::createAdditionalReports(
    const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {

    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(1);
    // prepare report
    report->addColumn("Portfolio", string())
        .addColumn("RiskClass", string())
        .addColumn("RiskType", string())
        .addColumn("PLDate1", Date())
        .addColumn("PLDate2", Date())
        .addColumn("PLAmount", double(), 6);

    if(riskFactorBreakdown_){
        // prepare report
        QuantLib::ext::shared_ptr<Report> report2 = reports->reports().at(2);
        report2->addColumn("RiskFactor", string())
            .addColumn("TradeId", string())
            .addColumn("PLDate1", Date())
            .addColumn("PLDate2", Date())
            .addColumn("PLAmount", double(), 6);
    }

}

void HistoricalSimulationVarReport::addPnlCalculators(
    const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) {
    if (!sensiBased_)
        return;
    QL_REQUIRE(period_, "HistoricalSimulationVarReport: period is required for sensi-based run");
    pnlCalculators_.push_back(QuantLib::ext::make_shared<PNLCalculator>(period_.value()));
}

void HistoricalSimulationVarReport::handleSensiResults(
    const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {
    QL_REQUIRE(pnlCalculators_.size() == 1, "HistoricalSimulationVarReport: expecting exactly 1 PNLCalculator");
    pnls_ = pnlCalculators_.front()->pnls();
    if (tradePnl_)
        tradePnls_ = pnlCalculators_.front()->tradePnls();
    if (riskFactorBreakdown_)
        sensiRiskFactorPnls_ = pnlCalculators_.front()->riskFactorTradePnls();
}

void HistoricalSimulationVarReport::handleFullRevalResults(const ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                                           const ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                                                           const ext::shared_ptr<TradeGroupBase>& tradeGroup) {
    // Always compute aggregate PnL for the main VaR report
    pnls_ = histPnlGen_->pnl(period_.value(), tradeIdIdxPairs_);
    if (tradePnl_)
        tradePnls_ = histPnlGen_->tradeLevelPnl(period_.value(), tradeIdIdxPairs_);
    if (riskFactorBreakdown_)
        riskFactorPnls_ = histPnlGen_->riskFactorLevelPnlSeries(period_.value());

    // Add theta adjustment to PnLs if enabled
    if (includeTheta_ && !thetaPerTrade_.empty()) {
        if (!pnls_.empty()) {
            // Compute aggregate theta for the current trade group
            Real totalTheta = 0.0;
            for (const auto& [tradeId, idx] : tradeIdIdxPairs_) {
                auto it = thetaPerTrade_.find(tradeId);
                if (it != thetaPerTrade_.end())
                    totalTheta += it->second;
            }
            for (auto& p : pnls_)
                p += totalTheta;
        }
        if (!tradePnls_.empty()) {
            // Build a vector of per-trade theta in tradeIdIdxPairs_ order
            std::vector<Real> tradeThetas;
            tradeThetas.reserve(tradeIdIdxPairs_.size());
            for (const auto& [tradeId, idx] : tradeIdIdxPairs_) {
                auto it = thetaPerTrade_.find(tradeId);
                tradeThetas.push_back(it != thetaPerTrade_.end() ? it->second : 0.0);
            }
            for (auto& scenarioPnls : tradePnls_) {
                for (Size i = 0; i < scenarioPnls.size() && i < tradeThetas.size(); ++i)
                    scenarioPnls[i] += tradeThetas[i];
            }
        }
        // Distribute theta proportionally across risk factor PnLs so that
        // sum(rf_pnl) per trade equals the total trade PnL (which includes theta).
        // For each trade t in each scenario s:
        //   adjusted_rf_pnl[k][t] = rf_pnl[k][t] * (1 + theta_t / sum_k(rf_pnl[k][t]))
        if (riskFactorBreakdown_ && !riskFactorPnls_.empty()) {
            // Build per-trade theta vector in tradeIdIdxPairs_ order
            std::vector<Real> thetaVec(tradeIdIdxPairs_.size(), 0.0);
            for (const auto& [tradeId, idx] : tradeIdIdxPairs_) {
                auto it = thetaPerTrade_.find(tradeId);
                if (it != thetaPerTrade_.end())
                    thetaVec[idx] = it->second;
            }
            Size numTrades = tradeIdIdxPairs_.size();
            for (Size s = 0; s < riskFactorPnls_.size(); ++s) {
                if (riskFactorPnls_[s].empty())
                    continue;
                // Compute sum of RF PnLs per trade for this scenario
                std::vector<Real> sumRfPnl(numTrades, 0.0);
                for (const auto& [key, vals] : riskFactorPnls_[s]) {
                    for (Size t = 0; t < numTrades && t < vals.size(); ++t) {
                        if (!std::isnan(vals[t]))
                            sumRfPnl[t] += vals[t];
                    }
                }
                // Scale each RF PnL proportionally to absorb theta
                for (auto& [key, vals] : riskFactorPnls_[s]) {
                    for (Size t = 0; t < numTrades && t < vals.size(); ++t) {
                        if (std::isnan(vals[t]) || vals[t] == 0.0)
                            continue;
                        if (sumRfPnl[t] != 0.0)
                            vals[t] *= (1.0 + thetaVec[t] / sumRfPnl[t]);
                    }
                }
            }
        }
    }
}

void HistoricalSimulationVarReport::reset(const ext::shared_ptr<MarketRiskGroupBase>& riskGroup) {
    MarketRiskReport::reset(riskGroup);
    pnls_.clear();
    tradePnls_.clear();
    riskFactorPnls_.clear();
    sensiRiskFactorPnls_.clear();
}

void HistoricalSimulationVarReport::writeReports(
    const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {
    if (sensiBased_ && pnls_.empty() && (!tradePnl_ || tradePnls_.empty()))
        return;
    VarReport::writeReports(reports, riskGroup, tradeGroup);
}

void HistoricalSimulationVarReport::writeAdditionalReports(
    const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {
    QL_REQUIRE(riskFactorBreakdown_?reports->reports().size()== 3:reports->reports().size()== 2, "HistoricalSimulationVarReport::writeAdditionalReports - 2 reports expected for HistoricalSimulationVar, 3 if riskFactorBreakdown==true");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(1);

    auto rg = ext::dynamic_pointer_cast<MarketRiskGroup>(riskGroup);
    auto tg = ext::dynamic_pointer_cast<TradeGroup>(tradeGroup);

    Size samples;
    if (sensiBased_) {
        samples = tradePnl_ ? tradePnls_.size() : pnls_.size();
    } else {
        QL_REQUIRE(histPnlGen_ && histPnlGen_->cube(),
                   "HistoricalSimulationVarReport: historical PnL cube is required for full revaluation");
        samples = histPnlGen_->cube()->samples();
    }
    QL_REQUIRE(hisScenGen_, "HistoricalSimulationVarReport: historical scenario generator is required");
    QL_REQUIRE(hisScenGen_->startDates().size() >= samples && hisScenGen_->endDates().size() >= samples,
               "HistoricalSimulationVarReport: scenario date vectors shorter than PnL vectors");

    // Loop-invariant string representations of risk class and risk type
    const std::string riskClassStr = to_string(rg->riskClass());
    const std::string riskTypeStr = to_string(rg->riskType());

    // Loop-invariant risk factor breakdown flag and report2 pointer
    const bool writeRFBreakdown = riskFactorBreakdown_ && (countRF_ < 1);
    QuantLib::ext::shared_ptr<Report> report2 = writeRFBreakdown ? reports->reports().at(2) : nullptr;

    // Loop through all samples
    for (Size s = 0; s < samples; ++s) {
        if (tradePnl_) {
            for (const auto& t : tradeIdIdxPairs_) {
                report->next();
                report->add(t.first);
                report->add(riskClassStr);
                report->add(riskTypeStr);
                report->add(hisScenGen_->startDates()[s]);
                report->add(hisScenGen_->endDates()[s]);
                report->add(tradePnls_[s][t.second]);
            }          
        } else {
            report->next();
            report->add(tg->portfolioId());
            report->add(riskClassStr);
            report->add(riskTypeStr);
            report->add(hisScenGen_->startDates()[s]);
            report->add(hisScenGen_->endDates()[s]);
            report->add(pnls_[s]);
        }
        if (writeRFBreakdown) {
            // The PnL breakdown on risk factors per scenario
            if (sensiBased_ && s < sensiRiskFactorPnls_.size()) {
                for (const auto& r : sensiRiskFactorPnls_[s]) {
                    const auto& key = r.first;
                    const std::vector<Real>& vals = r.second;
                    for (const auto& t : tradeIdIdxPairs_) {
                        if (t.second < vals.size()) {
                            Real pnl = vals[t.second];
                            if (!std::isnan(pnl) && !close_enough(pnl, 0.0)) {
                                report2->next();
                                report2->add(key);
                                report2->add(t.first);
                                report2->add(hisScenGen_->startDates()[s]);
                                report2->add(hisScenGen_->endDates()[s]);
                                report2->add(pnl);
                            }
                        }
                    }
                }
            } else if (s < riskFactorPnls_.size()) {
                for (const auto& r : riskFactorPnls_[s]) {
                    const auto& key = r.first;
                    const std::vector<Real>& vals = r.second;
                    for (const auto& t : tradeIdIdxPairs_) {
                        if (t.second < vals.size()) {
                            Real pnl = vals[t.second];
                            // Only report if not NaN, i.e. the trade is sensitive to this risk factor.
                            if (!std::isnan(pnl)) {
                                report2->next();
                                report2->add(ore::data::to_string(key));
                                report2->add(t.first);
                                report2->add(hisScenGen_->startDates()[s]);
                                report2->add(hisScenGen_->endDates()[s]);
                                report2->add(pnl);
                            }
                        }
                    }
                }
            }
        }
    }
    countRF_++;
}

void HistoricalSimulationVarReport::writeHeader(const ext::shared_ptr<Report>& report) const {
    report->addColumn("Portfolio", string()).addColumn("RiskClass", string()).addColumn("RiskType", string());
    for (const auto p : p())
        report->addColumn("Quantile_" + std::to_string(p), double(), 6);
    if (includeExpectedShortfall_) {
        for (const auto p : p())
            report->addColumn("ExpectedShortfall_" + std::to_string(p), double(), 6);
    }
}

std::vector<Real> HistoricalSimulationVarReport::calcVarsForQuantiles() const {
    auto histSimVarCalculator = QuantLib::ext::dynamic_pointer_cast<HistoricalSimulationVarCalculator>(varCalculator_);
    QL_REQUIRE(histSimVarCalculator, "Wrong VarCalculator provided");

    std::vector<Real> varRecords;
    for (const auto p : p())
        varRecords.push_back(histSimVarCalculator->var(p));
    if (includeExpectedShortfall_) {
        for (const auto p : p())
            varRecords.push_back(histSimVarCalculator->expectedShortfall(p));
    }
    return varRecords;
}

Real HistoricalSimulationVarCalculator::var(Real confidence, const bool isCall, 
    const set<pair<string, Size>>& tradeIds) const {

    // Use boost to calculate the quantile based on confidence_
    Size c = static_cast<Size>(std::floor(pnls_.size() * (1.0 - confidence) + 0.5)) + 2;
    typedef accumulator_set<double, stats<boost::accumulators::tag::tail_quantile<boost::accumulators::right>>>
        accumulator;
    accumulator acc(boost::accumulators::tag::tail<boost::accumulators::right>::cache_size = c);

    for (const auto& pnl : pnls_) {
        acc(isCall ? pnl : -pnl);
    }

    return quantile(acc, quantile_probability = confidence);
}

QuantLib::Real HistoricalSimulationVarCalculator::expectedShortfall(
    QuantLib::Real confidence, const bool isCall, const set<std::pair<std::string, QuantLib::Size>>& tradeIds) const {

    // calculate the VAR for the expected shortfall
    const auto var = this->var(confidence, isCall, tradeIds);
    if (std::isnan(var)) {
        return var;
    }

    accumulator_set<Real, stats<tag::mean>> accumulator;
    for (const auto pnl : pnls_) {
        const auto adjustedPnl = isCall ? pnl : -pnl;
        if (adjustedPnl <= var) {
            accumulator(adjustedPnl);
        }
    }
    return mean(accumulator);
}

bool HistoricalSimulationVarReport::disablesAll(const QuantLib::ext::shared_ptr<ScenarioFilter>& filter) const {
    // Return false if we hit any risk factor that is "allowed" i.e. enabled
    for (const auto& key : hisScenGen_->baseScenario()->keys()) {
        if (filter->allow(key)) {
            return false;
        }
    }
    // If we get to here, all risk factors are "not allowed" i.e. disabled
    return true;
}

} // namespace analytics
} // namespace ore
