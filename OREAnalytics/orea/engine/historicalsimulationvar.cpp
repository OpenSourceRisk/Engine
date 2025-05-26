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
    const string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const string& portfolioFilter, const vector<Real>& p, boost::optional<TimePeriod> period,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen, std::unique_ptr<FullRevalArgs> fullRevalArgs,
    const bool breakdown, const bool includeExpectedShortfall)
    : VarReport(baseCurrency, portfolio, portfolioFilter, p, period, hisScenGen, nullptr, std::move(fullRevalArgs)),
      includeExpectedShortfall_(includeExpectedShortfall) {
    fullReval_ = true;
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
}

void HistoricalSimulationVarReport::handleFullRevalResults(const ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                                           const ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                                                           const ext::shared_ptr<TradeGroupBase>& tradeGroup) {
    pnls_ = histPnlGen_->pnl(period_.get(), tradeIdIdxPairs_);
}

void HistoricalSimulationVarReport::writeAdditionalReports(
    const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
    const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
    const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) {
    QL_REQUIRE(reports->reports().size()== 2, "HistoricalSimulationVarReport::writeAdditionalReports - 2 reports expected for HistoricalSimulationVar");
    QuantLib::ext::shared_ptr<Report> report = reports->reports().at(1);
    auto rg = ext::dynamic_pointer_cast<MarketRiskGroup>(riskGroup);
    auto tg = ext::dynamic_pointer_cast<TradeGroup>(tradeGroup);

    // Loop through all samples
    for (Size s = 0; s < histPnlGen_->cube()->samples(); ++s) {
        report->next();
        report->add(tg->portfolioId());
        report->add(to_string(rg->riskClass()));
        report->add(to_string(rg->riskType()));
        report->add(hisScenGen_->startDates()[s]);
        report->add(hisScenGen_->endDates()[s]);
        report->add(pnls_.at(s));
    }
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

} // namespace analytics
} // namespace ore
