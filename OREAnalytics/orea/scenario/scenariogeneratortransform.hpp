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

/*! \file scenario/scenariogeneratortransform.hpp
    \brief Transformer class used for transform discount factors in the scenario into zero rates
    \ingroup scenario
*/

#pragma once

#include <vector>

#include <ql/shared_ptr.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariogenerator.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <ql/time/date.hpp>

#include <iostream>

namespace ore {
namespace analytics {
using QuantLib::TimeGrid;
using std::vector;

class ScenarioGeneratorTransform : public ScenarioGenerator {
public:
    ScenarioGeneratorTransform(QuantLib::ext::shared_ptr<ScenarioGenerator>& scenarioGenerator,
                               const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
                               const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketConfig)
        : scenarioGenerator_(scenarioGenerator), simMarket_(simMarket), simMarketConfig_(simMarketConfig){}

    QuantLib::ext::shared_ptr<Scenario> next(const QuantLib::Date& d) override;

    void reset() override;

private:
    QuantLib::ext::shared_ptr<ScenarioGenerator> scenarioGenerator_;
    QuantLib::ext::shared_ptr<ScenarioSimMarket> simMarket_;
    QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig_;
};

} // namespace analytics
} // namespace ore
