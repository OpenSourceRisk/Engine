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

/*! \file orea/app/analytics/correlationanalytic.hpp
    \brief Generate correlations from a set of historical scenarios
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/analyticfactory.hpp>
#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/app/inputvariables.hpp>
#include <orea/engine/correlationreport.hpp>

namespace ore {
namespace analytics {

class InputParameters;

struct CorrelationVariables : public InputVariables {
    void loadVariablesImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) override;
        
    QuantLib::ext::shared_ptr<ScenarioReader> scenarioReader_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData_;
    string lookbackPeriod_;
    // Pearson, Kendall-Rank
    std::string correlationMethod_ = "Pearson";
    QuantLib::Size horizonDays_ = 10;
    QuantLib::Calendar horizonCalendar_;
    bool horizonOverlappingPeriods_ = true;
    bool allowPartialScenarios_ = false;
};

class CorrelationAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "CORRELATION";
    CorrelationAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic::Impl(inputs, QuantLib::ext::make_shared<CorrelationVariables>()) {
        setLabel(LABEL);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;
    void buildDependencies() override;

    const QuantLib::ext::shared_ptr<CorrelationReport>& correlationReport() {return correlationReport_;}
        
protected:
    QuantLib::ext::shared_ptr<CorrelationReport> correlationReport_;
    void setCorrelationReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);
};


class CorrelationAnalytic : public Analytic {
public:
    CorrelationAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                        const QuantLib::ext::weak_ptr<ore::analytics::AnalyticsManager>& analyticsManager,
                        bool simulationConfig = false, bool sensitivityConfig = false)
        : Analytic(std::make_unique<CorrelationAnalyticImpl>(inputs), {"CORRELATION"}, inputs, analyticsManager,
                   simulationConfig, sensitivityConfig, false, false) {}
};

} // namespace analytics
} // namespace ore
