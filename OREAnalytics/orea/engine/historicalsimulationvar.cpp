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
    const string& baseCurrency,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen,
    const ext::shared_ptr<Portfolio>& portfolio, const string& portfolioFilter,
    const std::vector<Real>& p, const boost::optional<TimePeriod>& period,
    const ext::shared_ptr<ScenarioSimMarket>& simMarket,
    const ext::shared_ptr<EngineData>& engineData,
    const ext::shared_ptr<ReferenceDataManager>& referenceData,
    const ext::shared_ptr<IborFallbackConfig>& iborFallbackConfig)
    : VarReport(portfolio, portfolioFilter, p, period), baseCurrency_(baseCurrency), hisScenGen_(hisScenGen),
      simMarket_(simMarket), engineData_(engineData), referenceData_(referenceData), iborFallbackConfig_(iborFallbackConfig) {}

void HistoricalSimulationVarReport::calculate(ext::shared_ptr<MarketRiskReport::Reports>& report) {
    // set run type in engine data, make a copy of this before
    engineData_ = ext::make_shared<EngineData>(*engineData_);
    engineData_->globalParameters()["RunType"] = std::string("HistoricalPnL");

    // check the benchmark period
    if (period_) {
        auto p = period_.get();
        Date minDate = *std::min_element(p.startDates().begin(), p.startDates().end());
        Date maxDate = *std::max_element(p.endDates().begin(), p.endDates().end());
        QL_REQUIRE(hisScenGen_->startDates().front() <= minDate && maxDate <= hisScenGen_->endDates().back(),
                   "The benchmark period " << p
                                           << " is not covered by the historical scenario generator: Required dates = ["
                                           << ore::data::to_string(minDate) << "," << ore::data::to_string(maxDate)
                                           << "], Covered dates = [" << hisScenGen_->startDates().front() << ","
                                           << hisScenGen_->endDates().back() << "]");

        // Build the filtered historical scenario generator
        hisScenGen_ = ext::make_shared<HistoricalScenarioGeneratorWithFilteredDates>(std::vector<TimePeriod>{p},
                                                                                     hisScenGen_);

        hisScenGen_->baseScenario() = simMarket_->baseScenario();
    }
    auto factory = ext::make_shared<EngineFactory>(engineData_, simMarket_, map<MarketContext, string>(), 
        referenceData_, *iborFallbackConfig_);

   /* DLOG("Building the portfolio");
    portfolio_->build(factory, "historical pnl generation");
    DLOG("Portfolio built");

    ext::shared_ptr<NPVCube> cube = ext::make_shared<DoublePrecisionInMemoryCube>(
        simMarket_->asofDate(), portfolio_->ids(),
        vector<Date>(1, simMarket_->asofDate()), hisScenGen_->numScenarios());

    histPnlGen_ = ext::make_shared<HistoricalPnlGenerator>(baseCurrency_, portfolio_,
        simMarket_, hisScenGen_, cube, factory->modelBuilders(), false);*/


}

QuantLib::Real HistoricalSimulationVarCalculator::var(QuantLib::Real confidence, const bool isCall, 
    const set<pair<string, Size>>& tradeIds) {

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

} // namespace analytics
} // namespace ore