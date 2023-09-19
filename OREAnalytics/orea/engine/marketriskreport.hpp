/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file engine/marketriskreport.hpp
    \brief Base class for a market risk report
    \ingroup engine
*/

#pragma once

#include <qle/math/covariancesalvage.hpp>
#include <ored/report/report.hpp>
#include <orea/engine/historicalsensipnlcalculator.hpp>
#include <orea/scenario/scenariofilter.hpp>

#include <vector>

namespace ore {
namespace analytics {

using TradePnLStore = std::vector<std::vector<QuantLib::Real>>;

class MarketRiskGroup {
public:
    MarketRiskGroup() {}
    virtual ~MarketRiskGroup() {}

    virtual std::string to_string() = 0;
    virtual bool allLevel() const = 0;
};

std::ostream& operator<<(std::ostream& out, const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup);

class MarketRiskGroupContainer {
public:
    MarketRiskGroupContainer() {}
    virtual ~MarketRiskGroupContainer() {}

    virtual QuantLib::ext::shared_ptr<MarketRiskGroup> next() = 0;
    virtual void add(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup) = 0;
    virtual void reset() = 0;
    virtual QuantLib::Size size() = 0;
};

class TradeGroup {
public:
    TradeGroup() {}
    virtual ~TradeGroup() {}

    virtual std::string to_string() = 0;
    virtual bool allLevel() const = 0;
};

std::ostream& operator<<(std::ostream& out, const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup);

class TradeGroupContainer {
public:
    TradeGroupContainer() {}
    virtual ~TradeGroupContainer() {}

    virtual QuantLib::ext::shared_ptr<TradeGroup> next() = 0;
    virtual void add(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) = 0;
    virtual void reset() = 0;
};

class MarketRiskReport {
public:
    struct SensiRunArgs {
        //! Stream of sensitivity records used for the sensitivity based backtest
        QuantLib::ext::shared_ptr<SensitivityStream> sensitivityStream_;
        //! Calculates the shift multiple in moving from one scenario to another
        QuantLib::ext::shared_ptr<ScenarioShiftCalculator> shiftCalculator_;
        //! Amount by which absolute P&L value must exceed 0 to be written to P&L contribution report
        QuantLib::Real pnlWriteThreshold_;
        //! Optional input of covariance matrix
        std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covarianceInput_;

        SensiRunArgs(const QuantLib::ext::shared_ptr<SensitivityStream>& ss,
                     const QuantLib::ext::shared_ptr<ScenarioShiftCalculator>& sc,
                     QuantLib::Real pnlThres = 0.01, std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> ci = {})
            : sensitivityStream_(ss), shiftCalculator_(sc), pnlWriteThreshold_(pnlThres), covarianceInput_(ci) {}
    };

    class Reports {
    public:
        Reports() {}
        void add(const QuantLib::ext::shared_ptr<ore::data::Report>& report) { reports_.push_back(report); }
        const std::vector<QuantLib::ext::shared_ptr<ore::data::Report>>& reports() const { return reports_; };

    protected: 
        std::vector<QuantLib::ext::shared_ptr<ore::data::Report>> reports_;
    };

    MarketRiskReport(boost::optional<ore::data::TimePeriod> period,
                     std::unique_ptr<SensiRunArgs> sensiArgs = nullptr, const bool requireTradePnl = false)
        : period_(period), sensiArgs_(std::move(sensiArgs)), requireTradePnl_(requireTradePnl) {}
    virtual ~MarketRiskReport() {}
        
    //! VAR types used as a benchmark against which SIMM can be compared
    enum class VarType { HistSim, HistSimTaylor, Parametric, Lch };

    virtual void calculate(QuantLib::ext::shared_ptr<Reports>& report);

protected:
    bool sensiBased_ = false;
    bool fullReval_ = false;

    boost::optional<ore::data::TimePeriod> period_;
    std::unique_ptr<SensiRunArgs> sensiArgs_;
    // Whether we require trade-level PnLs to use for later calculations
    bool requireTradePnl_ = false;

    QuantLib::ext::shared_ptr<MarketRiskGroupContainer> riskGroups_;
    QuantLib::ext::shared_ptr<TradeGroupContainer> tradeGroups_;
    
    /*! Partition of portfolio's trade IDs (and index as a pair), into groups.
    */
    std::map<std::string, std::set<std::pair<std::string, QuantLib::Size>>> tradeIdGroups_;
    std::set<std::pair<std::string, QuantLib::Size>> tradeIdIdxPairs_;
        
    std::map<RiskFactorKey, QuantLib::Real> deltas_;
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, QuantLib::Real> gammas_;
    QuantLib::Matrix covarianceMatrix_;
    QuantLib::ext::shared_ptr<QuantExt::CovarianceSalvage> salvage_ =
        QuantLib::ext::make_shared<QuantExt::SpectralCovarianceSalvage>();
    bool includeDeltaMargin_ = true;
    bool includeGammaMargin_ = true;

    QuantLib::ext::shared_ptr<HistoricalSensiPnlCalculator> sensiPnlCalculator_;
    
    virtual QuantLib::ext::shared_ptr<ScenarioFilter>
    createScenarioFilter(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup) = 0;
    
    virtual void reset(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup);

    /*! Check if the given scenario \p filter turns off all risk factors in the
        historical scenario generator
    */
    virtual bool disablesAll(const QuantLib::ext::shared_ptr<ScenarioFilter>& filter) const { return false; };

    //! update any filters required
    virtual void updateFilter(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup,
                              const QuantLib::ext::shared_ptr<ScenarioFilter>& filter) {}
    virtual std::string portfolioId(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) const = 0;
    virtual std::string tradeGroupKey(const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) const = 0;
    void addPnlCalculators(std::vector<QuantLib::ext::shared_ptr<PNLCalculator>>& pnlCalculators){};
    virtual void handleSensiResults(QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& report,
                                    const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup,
                                    const QuantLib::ext::shared_ptr<TradeGroup>& tradeGroup) = 0;

    virtual bool includeDeltaMargin(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup) const { return true; }
    virtual bool includeGammaMargin(const QuantLib::ext::shared_ptr<MarketRiskGroup>& riskGroup) const { return true; }
};

} // namespace analytics
} // namespace ore
