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

/*! \file orea/app/analytics/scenariogeneratorsanalytic.hpp
    \brief ORE Scenario Generator Analytics
*/

#pragma once

#include <orea/app/analytic.hpp>

namespace ore {
namespace analytics {

class ScenarioGenerationAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SCENARIO_GENERATION";

    enum class Type { stress, sensitivity, exposure};
    ScenarioGenerationAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator() { return scenarioGenerator_; }

protected:
    void buildScenarioSimMarket();
    void buildCrossAssetModel(bool continueOnError, bool allowModelFallbacks);
    void buildScenarioGenerator(bool continueOnError, bool allowModelFallbacks);

    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<CrossAssetModel> model_;
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator_;

    QuantLib::ext::shared_ptr<DateGrid> grid_;
    Size samples_ = 0;
    

private:
    Type type_ = Type::stress;
    QuantLib::ext::shared_ptr<StressTestScenarioData> stressTestScenarioData_;
    Size scenarioDistributionSteps_ = 20;
    bool scenarioOutputZeroRate_ = false;
    bool scenarioOutputStatistics_ = true;
    bool scenarioOutputDistributions_ = true;
    std::string amcPathDataOutput_;
};

ScenarioGenerationAnalyticImpl::Type parseScenarioGenerationType(const std::string& s);
std::ostream& operator<<(std::ostream& out, ScenarioGenerationAnalyticImpl::Type t);

class ScenarioGenerationAnalytic : public Analytic {
public:
    ScenarioGenerationAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                               const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager)
        : Analytic(std::make_unique<ScenarioGenerationAnalyticImpl>(inputs), {"SCENARIO_GENERATOR"}, inputs,
                   analyticsManager, true, false, true, true) {}
};

} // namespace analytics
} // namespace ore
