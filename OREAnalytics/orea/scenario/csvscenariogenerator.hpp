/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#pragma once

#include <fstream>

#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenariogenerator.hpp>

namespace ore {
namespace analytics {

//! Class for generating scenarios from a csv file assumed to be in a format compatible with ScenarioWriter.
class CSVScenarioGenerator : public ScenarioGenerator {
public:
    CSVScenarioGenerator(const std::string& filename, const QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory,
                         const char sep = ',');

    virtual ~CSVScenarioGenerator();

    virtual QuantLib::ext::shared_ptr<Scenario> next(const Date& d) override;

    virtual void reset() override;

private:
    void readKeys();

    vector<RiskFactorKey> keys_;
    std::ifstream file_;
    const char sep_;
    const std::string& filename_;
    const QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory_;
};
} // namespace analytics
} // namespace ore
