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

/*! \file orea/app/analytics/scenarioanalytic.hpp
    \brief ORE Scenario Analytic
*/
#pragma once

#include <orea/app/analytic.hpp>

namespace ore {
namespace analytics {
    
class ScenarioAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "SCENARIO";

    ScenarioAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : 
        Analytic::Impl(inputs) {
        setLabel(LABEL);
    }
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                             const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

    const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& scenario() const { return scenario_; };
    void setScenario(const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& scenario) { scenario_ = scenario; }
    void setUseSpreadedTermStructures(const bool useSpreadedTermStructures) {
        useSpreadedTermStructures_ = useSpreadedTermStructures;
    }

    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& scenarioSimMarket() const {
        return scenarioSimMarket_;
    };
    void setScenarioSimMarket(const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& ssm) {
        scenarioSimMarket_ = ssm;
    }

private:
    QuantLib::ext::shared_ptr<ore::analytics::Scenario> scenario_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket> scenarioSimMarket_;
    bool useSpreadedTermStructures_ = false;
};

class ScenarioAnalytic : public Analytic {
public:
    ScenarioAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : Analytic(std::make_unique<ScenarioAnalyticImpl>(inputs), {"SCENARIO"}, inputs,
                   true, false, false, false) {}
};

} // namespace analytics
} // namespace ore
