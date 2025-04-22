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
#include <ored/report/inmemoryreport.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
namespace ore {
namespace analytics {

class SensitivityStressAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SENSITIVITY_STRESS";
    explicit SensitivityStressAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs, const boost::optional<QuantLib::ext::shared_ptr<StressTestScenarioData>>& scenarios = {});
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    void buildDependencies() override;
    void setStressScenarios(const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressScenarios) { stressScenarios_ = stressScenarios; }

private:
    void runStressTest(const QuantLib::ext::shared_ptr<ore::analytics::StressScenarioGenerator>& scenarioGenerator,
                       const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);
    void concatReports(const std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>& sensitivityReports);

    QuantLib::ext::shared_ptr<StressTestScenarioData> stressScenarios_;
};

class SensitivityStressAnalytic : public Analytic {
public:
    explicit SensitivityStressAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                               const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager,
        const boost::optional<QuantLib::ext::shared_ptr<StressTestScenarioData>>& scenarios = {})
        : Analytic(std::make_unique<SensitivityStressAnalyticImpl>(inputs, scenarios), {"SENSITIVITY_STRESS"}, inputs, analyticsManager,
                   true, false, false, false) {}
};

} // namespace analytics
} // namespace ore
