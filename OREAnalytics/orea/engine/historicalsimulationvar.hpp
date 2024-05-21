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

/*! \file engine/historicalsimulationvar.hpp
    \brief Perform historical simulation var calculation for a given portfolio
    \ingroup engine
*/

#pragma once

#include <orea/engine/historicalpnlgenerator.hpp>
#include <orea/engine/sensitivityaggregator.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <orea/engine/varcalculator.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>

#include <ored/report/report.hpp>
#include <ored/utilities/timeperiod.hpp>

#include <qle/math/covariancesalvage.hpp>

#include <ql/math/array.hpp>
#include <ql/math/matrix.hpp>

#include <map>
#include <set>

namespace ore {
namespace analytics {
using QuantLib::Array;
using QuantLib::Matrix;

class HistoricalSimulationVarCalculator : public VarCalculator {
public:
    HistoricalSimulationVarCalculator(const std::vector<QuantLib::Real>& pnls) : pnls_(pnls) {}

    QuantLib::Real var(QuantLib::Real confidence, const bool isCall = true,
        const std::set<std::pair<std::string, QuantLib::Size>>& tradeIds = {}) override;

private:
    const std::vector<QuantLib::Real>& pnls_;
};

//! HistoricalSimulation VaR Calculator
/*! This class takes sensitivity data and a covariance matrix as an input and computes a Historical Simulation value at risk. The
 * output can be broken down by portfolios, risk classes (IR, FX, EQ, ...) and risk types (delta-gamma, vega, ...). */
class HistoricalSimulationVarReport : public VarReport {
public:
    virtual ~HistoricalSimulationVarReport() {}

    HistoricalSimulationVarReport(const std::string& baseCurrency,
                                  const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                                  const std::string& portfolioFilter, 
        const vector<Real>& p, boost::optional<ore::data::TimePeriod> period,
        const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hisScenGen = nullptr, 
        std::unique_ptr<FullRevalArgs> fullRevalArgs = nullptr, const bool breakdown = false);

protected:
    void createVarCalculator() override;
    void handleFullRevalResults(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports,
        const QuantLib::ext::shared_ptr<MarketRiskGroupBase>& riskGroup, 
        const QuantLib::ext::shared_ptr<TradeGroupBase>& tradeGroup) override;

private:
    std::vector<QuantLib::Real> pnls_;
};

} // namespace analytics
} // namespace ore