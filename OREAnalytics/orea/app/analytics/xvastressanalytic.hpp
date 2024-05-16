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

/*! \file orea/app/analytics/xvastressanalytic.hpp
    \brief xva stress analytic
*/

#pragma once

#include <orea/app/analytic.hpp>
#include <orea/app/analytics/xvaanalytic.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <orea/scenario/stressscenariogenerator.hpp>
namespace ore {
namespace analytics {

class XvaStressAnalyticImpl : public Analytic::Impl {
public:
    static constexpr const char* LABEL = "XVA_STRESS";
    explicit XvaStressAnalyticImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs);
    void runAnalytic(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                     const std::set<std::string>& runTypes = {}) override;
    void setUpConfigurations() override;

private:
    void runStressTest(const QuantLib::ext::shared_ptr<ore::analytics::StressScenarioGenerator>& scenarioGenerator,
                       const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader);
    void writeCubes(const std::string& label, const QuantLib::ext::shared_ptr<XvaAnalytic>& xvaAnalytic);
    void concatReports(const std::map<std::string, std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>>& xvaReports);
};

class XvaStressAnalytic : public Analytic {
public:
    explicit XvaStressAnalytic(const QuantLib::ext::shared_ptr<InputParameters>& inputs);

};



} // namespace analytics
} // namespace ore
