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

/*! \file orea/app/analytics/scenariogenerationanalytic.hpp
    \brief ORE Scenario Generation Analytics
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/inputvariables.hpp>

namespace ore {
namespace analytics {

class InputParameters;

enum class ScenarioGenerationType { stress, sensitivity, exposure };
struct ScenarioGenerationVariables : public InputVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;
       
    ScenarioGenerationType type_ = ScenarioGenerationType::exposure;
    QuantLib::ext::shared_ptr<StressTestScenarioData> stressTestScenarioData_;
    QuantLib::Integer scenarioDistributionSteps_ = 20;
    bool scenarioOutputZeroRate_ = false;
    bool scenarioOutputStatistics_ = true;
    bool scenarioOutputDistributions_ = true;
    QuantLib::Integer scenarioPrecision_ = 8;
    std::string amcPathDataOutput_;    
    
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketParams_;
    QuantLib::ext::shared_ptr<ScenarioGeneratorData> scenarioGeneratorData_;
    QuantLib::ext::shared_ptr<CrossAssetModelData> crossAssetModelData_;
    QuantLib::ext::shared_ptr<EngineData> pricingEngine_;
};

class ScenarioGenerationAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SCENARIO_GENERATION";

    ScenarioGenerationAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic::Impl(inputs, QuantLib::ext::make_shared<ScenarioGenerationVariables>()) {
        setLabel(LABEL);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator() { return scenarioGenerator_; }

protected:
    void buildScenarioSimMarket();
    void buildCrossAssetModel(const bool continueOnError, const bool allowModelFallbacks);
    void buildScenarioGenerator(const bool continueOnError, const bool allowModelFallbacks);

    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator_;

    QuantLib::ext::shared_ptr<DateGrid> grid_;
    Size samples_ = 0;
};

ScenarioGenerationType parseScenarioGenerationType(const std::string& s);
std::ostream& operator<<(std::ostream& out, ScenarioGenerationType t);

class ScenarioGenerationAnalytic : public Analytic {
public:
    ScenarioGenerationAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                               const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : Analytic(std::make_unique<ScenarioGenerationAnalyticImpl>(inputs), {"SCENARIO_GENERATOR"}, inputs,
                   analyticsManager, true, false, true, true) {}
};

} // namespace analytics
} // namespace ore
