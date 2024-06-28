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

#include <orea/scenario/lgmscenariogenerator.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

LgmScenarioGenerator::LgmScenarioGenerator(QuantLib::ext::shared_ptr<QuantExt::LGM> model,
                                           QuantLib::ext::shared_ptr<QuantExt::MultiPathGeneratorBase> pathGenerator,
                                           QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory,
                                           QuantLib::ext::shared_ptr<ScenarioSimMarketParameters> simMarketConfig, Date today,
                                           DateGrid grid)
    : ScenarioPathGenerator(today, grid.dates(), grid.timeGrid()), model_(model), pathGenerator_(pathGenerator),
      scenarioFactory_(scenarioFactory), simMarketConfig_(simMarketConfig) {
    QL_REQUIRE(timeGrid_.size() == dates_.size() + 1, "date/time grid size mismatch");
}

std::vector<QuantLib::ext::shared_ptr<Scenario>> LgmScenarioGenerator::nextPath() {
    std::vector<QuantLib::ext::shared_ptr<Scenario>> scenarios(dates_.size());
    Sample<MultiPath> sample = pathGenerator_->next();

    DayCounter dc = model_->parametrization()->termStructure()->dayCounter();

    std::string ccy = model_->parametrization()->currency().code();
    vector<RiskFactorKey> keys;
    for (Size k = 0; k < simMarketConfig_->yieldCurveTenors(ccy).size(); k++)
        keys.emplace_back(RiskFactorKey::KeyType::DiscountCurve, ccy, k);

    for (Size i = 0; i < dates_.size(); i++) {
        Real t = timeGrid_[i + 1]; // recall: time grid has inserted t=0

        scenarios[i] = scenarioFactory_->buildScenario(dates_[i], true);

        // Set numeraire, numeraire currency and the (deterministic) domestic discount
        // Asset index 0 in sample.value[0][i+1] refers to the domestic currency process,
        // the only one here.
        Real z0 = sample.value[0][i + 1]; // second index = 0 holds initial values
        scenarios[i]->setNumeraire(model_->numeraire(t, z0));

        Real z = sample.value[0][i + 1]; // LGM factor value, second index = 0 holds initial values
        for (Size k = 0; k < simMarketConfig_->yieldCurveTenors(ccy).size(); k++) {
            Date d = dates_[i] + simMarketConfig_->yieldCurveTenors(ccy)[k];
            Real T = dc.yearFraction(dates_[i], d);
            Real discount = model_->discountBond(t, t + T, z);
            scenarios[i]->add(keys[k], discount);
        }
    }
    return scenarios;
}
} // namespace analytics
} // namespace ore
