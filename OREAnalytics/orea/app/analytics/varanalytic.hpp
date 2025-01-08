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
#include <orea/app/analytics/pricinganalytic.hpp>
#include <orea/engine/varcalculator.hpp>

namespace ore {
namespace analytics {
  
class VarAnalyticImpl : public Analytic::Impl {
public:
    VarAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs, const string& label)
        : Analytic::Impl(inputs) {
        setLabel(label);
    }
    virtual void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    virtual void setUpConfigurations() override;

protected:
    QuantLib::ext::shared_ptr<VarReport> varReport_;

    virtual void setVarReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader){};
};

class VarAnalytic : public Analytic {
public:
    VarAnalytic(std::unique_ptr<Analytic::Impl> impl, const std::set<std::string>& analyticTypes,
                const QuantLib::ext::shared_ptr<InputParameters>& inputs, bool simulationConfig = false,
                bool sensitivityConfig = false)
        : Analytic(std::move(impl), analyticTypes, inputs, simulationConfig, sensitivityConfig, false, false) {}
};

class ParametricVarAnalyticImpl : public VarAnalyticImpl {
public:
    static constexpr const char* LABEL = "PARAMETRIC_VAR";

    ParametricVarAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : VarAnalyticImpl(inputs, LABEL) {};

    virtual void setUpConfigurations() override;
    virtual QuantLib::ext::shared_ptr<SensitivityStream>
    sensiStream(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) {
        return inputs_->sensitivityStream();
    };

protected:
    void setVarReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) override;
};

class ParametricVarAnalytic : public VarAnalytic {
public:
    ParametricVarAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                          const QuantLib::ext::shared_ptr<Scenario>& offSetScenario = nullptr,
                          const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams = nullptr)
        : VarAnalytic(std::make_unique<ParametricVarAnalyticImpl>(inputs), {"PARAMETRIC_VAR"}, inputs) {}
};

class HistoricalSimulationVarAnalyticImpl : public VarAnalyticImpl {
public:
    static constexpr const char* LABEL = "HISTSIM_VAR";

    HistoricalSimulationVarAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs)
        : VarAnalyticImpl(inputs, LABEL) {}
    void setUpConfigurations() override;

protected:
    void setVarReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader) override;
};

class HistoricalSimulationVarAnalytic : public VarAnalytic {
public:
    HistoricalSimulationVarAnalytic(
        const QuantLib::ext::shared_ptr<InputParameters>& inputs,
        const QuantLib::ext::shared_ptr<Scenario>& offSetScenario = nullptr,
        const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& offsetSimMarketParams = nullptr)
        : VarAnalytic(std::make_unique<HistoricalSimulationVarAnalyticImpl>(inputs), {"HISTSIM_VAR"}, inputs, true) {}
};

} // namespace analytics
} // namespace oreplus
