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

/*! \file orea/app/analytic.hpp
    \brief ORE Analytics Manager
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/inputvariables.hpp>
#include <orea/engine/marketriskreport.hpp>

namespace ore {
namespace analytics {

class InputParameters;
class VarReport;

struct VarVariables : public InputVariables {
    virtual void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;

    std::vector<Real> varQuantiles_;
    bool varBreakDown_ = false;
    std::string portfolioFilter_;
    std::string lookbackPeriod_;
    QuantLib::Size horizonDays_ = 10;
    QuantLib::Calendar horizonCalendar_;
    bool horizonOverlappingPeriods_ = true;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketParams_;
    QuantLib::ext::shared_ptr<SensitivityScenarioData> sensiScenarioData_;
    QuantLib::ext::shared_ptr<ScenarioReader> scenarioReader_;
    bool outputHistoricalScenarios_ = false;
    QuantLib::ext::shared_ptr<ReturnConfiguration> returnConfiguration_;
};
  
class VarAnalyticImpl : public Analytic::Impl {
public:
    VarAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs, const string& label,
                    QuantLib::ext::shared_ptr<InputVariables> vars)
        : Analytic::Impl(inputs, vars) {
        setLabel(label);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    virtual void setUpConfigurations() override;

protected:
    QuantLib::ext::shared_ptr<VarReport> varReport_;

    virtual void setVarReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader){};
    virtual void addAdditionalReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports){};
};

class VarAnalytic : public Analytic {
public:
    VarAnalytic(std::unique_ptr<Analytic::Impl> impl, const std::set<std::string>& analyticTypes,
                const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager,
                bool simulationConfig = false,
                bool sensitivityConfig = false)
        : Analytic(std::move(impl), analyticTypes, inputs, analyticsManager, simulationConfig, sensitivityConfig, false,
                   false) {}
};

struct ParametricVarVariables : public VarVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;

    SalvagingAlgorithm::Type varSalvagingAlgorithm_ = SalvagingAlgorithm::None;
    // Delta, DeltaGammaNormal, MonteCarlo, Cornish-Fisher, Saddlepoint
    std::string varMethod_ = "DeltaGammaNormal";
    Size mcVarSamples_ = 1000000;
    long mcVarSeed_ = 42;
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> covarianceData_;
    QuantLib::ext::shared_ptr<SensitivityStream> sensitivityStream_;

};

class ParametricVarAnalyticImpl : public VarAnalyticImpl {
public:
    static constexpr const char* LABEL = "PARAMETRIC_VAR";

    ParametricVarAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : VarAnalyticImpl(inputs, LABEL, QuantLib::ext::make_shared<ParametricVarVariables>()) {};

    virtual void setUpConfigurations() override;
    virtual QuantLib::ext::shared_ptr<SensitivityStream>
    sensiStream(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);

protected:
    void setVarReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) override;
};

class ParametricVarAnalytic : public VarAnalytic {
public:
    ParametricVarAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                          const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : VarAnalytic(std::make_unique<ParametricVarAnalyticImpl>(inputs), {"PARAMETRIC_VAR"}, inputs,
                      analyticsManager) {}
};

struct HistoricalSimulationVarVariables : public VarVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;
    
    bool tradePnL_ = false;
    bool includeExpectedShortfall_ = false;
    bool riskFactorBreakdown_ = false;
    bool riskClassBreakdown_ = true;
};

class HistoricalSimulationVarAnalyticImpl : public VarAnalyticImpl {
public:
    static constexpr const char* LABEL = "HISTSIM_VAR";

    HistoricalSimulationVarAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : VarAnalyticImpl(inputs, LABEL, QuantLib::ext::make_shared<HistoricalSimulationVarVariables>()) {}
    void setUpConfigurations() override;

protected:
    void setVarReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) override;
    void addAdditionalReports(const QuantLib::ext::shared_ptr<MarketRiskReport::Reports>& reports) override;
    bool riskFactorBreakdown_ = false;
    bool riskClassBreakdown_ = true;
    bool allowPartialScenarios_ = false;
};

class HistoricalSimulationVarAnalytic : public VarAnalytic {
public:
    HistoricalSimulationVarAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                    const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : VarAnalytic(std::make_unique<HistoricalSimulationVarAnalyticImpl>(inputs), {"HISTSIM_VAR"}, inputs,
                      analyticsManager, true) {}
};

} // namespace analytics
} // namespace ore