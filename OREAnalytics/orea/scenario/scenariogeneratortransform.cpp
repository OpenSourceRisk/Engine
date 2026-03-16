/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/scenario/scenariogeneratortransform.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

QuantLib::ext::shared_ptr<Scenario> ScenarioGeneratorTransform::next(const Date& d) {
    QuantLib::ext::shared_ptr<Scenario> scenario = scenarioGenerator_->next(d)->clone();
    const vector<RiskFactorKey>& keys = simMarket_->baseScenario()->keys();
    Date asof = simMarket_->baseScenario()->asof();
    vector<Period> tenors;
    Date endDate;
    DayCounter dc;

    for (Size k = 0; k < keys.size(); ++k) {
        if ((keys[k].keytype == RiskFactorKey::KeyType::DiscountCurve) ||
            (keys[k].keytype == RiskFactorKey::KeyType::IndexCurve) ||
            (keys[k].keytype == RiskFactorKey::KeyType::YieldCurve)) {
            Real df = scenario->get(keys[k]);
            Real compound = 1 / df;
            if ((keys[k].keytype == RiskFactorKey::KeyType::DiscountCurve)) {
                dc = simMarket_->discountCurve(keys[k].name)->dayCounter();
                tenors = simMarketConfig_->yieldCurveTenors(keys[k].name);
            } else if (keys[k].keytype == RiskFactorKey::KeyType::IndexCurve ||
                       keys[k].keytype == RiskFactorKey::KeyType::YieldCurve) {
                dc = simMarket_->iborIndex(keys[k].name)->dayCounter();
                tenors = simMarketConfig_->yieldCurveTenors(keys[k].name);
            }
            endDate = asof + tenors[keys[k].index];
            Real zero = InterestRate::impliedRate(compound, dc, QuantLib::Compounding::Continuous,
                                                  QuantLib::Frequency::Annual, asof, endDate)
                            .rate();
            scenario->add(keys[k], zero);
        }
    }
    return scenario;
}

void ScenarioGeneratorTransform::reset() { scenarioGenerator_->reset(); }

} // namespace analytics
} // namespace ore
