/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file orea/app/analytics/sensitivitystressanalytic.hpp
    \brief sensitivity stress analytic
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <orea/app/inputvariables.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/inmemoryreport.hpp>
namespace ore {
namespace analytics {

class InputParameters;

struct SensitivityStressVariables : public InputVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;
    QuantLib::ext::shared_ptr<StressTestScenarioData> sensitivityStressScenarioData_;
    bool calcBaseScenario_ = false;
};

class SensitivityStressAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SENSITIVITY_STRESS";
    explicit SensitivityStressAnalyticImpl(
        const QuantLib::ext::shared_ptr<InputParameters>& inputs,
        const QuantLib::ext::optional<QuantLib::ext::shared_ptr<StressTestScenarioData>>& scenarios = std::nullopt);
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    void buildDependencies() override;

private:
    void runStressTest(const QuantLib::ext::shared_ptr<ore::analytics::StressScenarioGenerator>& scenarioGenerator,
                       const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                       bool calcBaseScenario);
    void concatReports(const std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>&
                           sensitivityReports);

    std::optional<QuantLib::ext::shared_ptr<StressTestScenarioData>> stressScenarios_;
};

class SensitivityStressAnalytic : public Analytic {
public:
    explicit SensitivityStressAnalytic(
        const QuantLib::ext::shared_ptr<InputParameters>& inputs,
        const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager,
        const QuantLib::ext::optional<QuantLib::ext::shared_ptr<StressTestScenarioData>>& scenarios = std::nullopt)
        : Analytic(std::make_unique<SensitivityStressAnalyticImpl>(inputs, scenarios), {"SENSITIVITY_STRESS"}, inputs,
                   analyticsManager, true, true, false, false) {}
};

} // namespace analytics
} // namespace ore
