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
    const std::string& baseCurrency, const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
    const string& portfolioFilter, const vector<Real>& p, boost::optional<TimePeriod> period,
    const ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen, std::unique_ptr<FullRevalArgs> fullRevalArgs,
    const bool breakdown)
    : VarReport(baseCurrency, portfolio, portfolioFilter, p, period, hisScenGen, nullptr, std::move(fullRevalArgs)) {
    fullReval_ = true;
}

void HistoricalSimulationVarReport::createVarCalculator() {
    varCalculator_ = QuantLib::ext::make_shared<HistoricalSimulationVarCalculator>(pnls_);
}

void HistoricalSimulationVarReport::handleFullRevalResults(const ext::shared_ptr<MarketRiskReport::Reports>& reports,
                                                           const ext::shared_ptr<MarketRiskGroupBase>& riskGroup,
                                                           const ext::shared_ptr<TradeGroupBase>& tradeGroup) {
    pnls_ = histPnlGen_->pnl(period_.get(), tradeIdIdxPairs_);
}

Real HistoricalSimulationVarCalculator::var(Real confidence, const bool isCall, 
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
