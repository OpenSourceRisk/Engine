/*
 Copyright (C) 2026 AcadiaSoft Inc.
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

/*! \file orea/app/analytics/stressedsimmanalytic.hpp
    \brief stressed SIMM analytic
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
#include <ored/report/inmemoryreport.hpp>

namespace ore {
namespace analytics {

class InputParameters;

struct StressedSimmVariables : public InputVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;

    QuantLib::ext::shared_ptr<StressTestScenarioData> stressedSimmScenarioData_;
};

class StressedSimmAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SIMM_STRESS";

    explicit StressedSimmAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs);

    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    void buildDependencies() override;
    void setStressScenarios(const QuantLib::ext::shared_ptr<StressTestScenarioData>& stressScenarios) {
        stressScenarios_ = stressScenarios;
    }

private:
    void runStressTest(const QuantLib::ext::shared_ptr<ore::analytics::StressScenarioGenerator>& scenarioGenerator,
                       const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);
    void concatReports(const std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>&
                           sensitivityReports);

    QuantLib::ext::shared_ptr<StressTestScenarioData> stressScenarios_;
};

class StressedSimmAnalytic : public Analytic {
public:
    static constexpr const char* simmLookupKey = "SIMM";
    explicit StressedSimmAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                                  const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : Analytic(std::make_unique<StressedSimmAnalyticImpl>(inputs), {"SIMM_STRESS"}, inputs, analyticsManager, true,
                   false, true, false) {
        LOG("Constructed ore::StressedSimmAnalytic");
    }
};

} // namespace analytics
} // namespace ore
