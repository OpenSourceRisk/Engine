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

#include <orea/scenario/zerotoparscenariogenerator.hpp>
#include <orea/scenario/scenarioutilities.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <orea/scenario/simplescenariofactory.hpp>
#include <orea/engine/parsensitivityanalysis.hpp>

using namespace QuantLib;

namespace ore {
namespace analytics {

ZeroToParScenarioGenerator::ZeroToParScenarioGenerator(
    const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hsg,
    const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
    const ParSensitivityInstrumentBuilder::Instruments& parInstruments)
    : HistoricalScenarioGenerator(hsg->scenarioLoader(), hsg->scenarioFactory(), hsg->returnConfiguration(),
                                  hsg->adjFactors(), hsg->labelPrefix(), hsg->generateDifferenceScenarios()) {

    auto bs = hsg->baseScenario();
    shiftConverter_ = QuantLib::ext::make_shared<ZeroToParShiftConverter>(parInstruments, simMarket);
    auto baseValues = shiftConverter_->baseValues();
    
    // build a base and base par scenario off the zero base scenario, and update with the calculated par rates
    // base scenario must be the minimum of the simulation (hsg baseScenario) and sensitivity configuration

    baseScenario_ = QuantLib::ext::make_shared<SimpleScenario>(bs->asof(), bs->label(), bs->getNumeraire());
    baseParScenario_ = QuantLib::ext::make_shared<SimpleScenario>(bs->asof(), bs->label(), bs->getNumeraire());
    baseParScenario_->setPar(true);
    for (auto& k : bs->keys()) {
        auto bv = bs->get(k);
        if (ParSensitivityAnalysis::isParType(k.keytype)) {
            auto it = baseValues.find(k);
            if (it != baseValues.end()) {
                // found in par rates
                baseScenario_->add(k, bv);
                baseParScenario_->add(k, it->second);
            }
        } else {
            baseScenario_->add(k, bv);
            baseParScenario_->add(k, bv);
        }
    }
}

QuantLib::ext::shared_ptr<Scenario> ZeroToParScenarioGenerator::next(const Date& d) {
    auto zeroScenario = HistoricalScenarioGenerator::next(d);

    // create a par scenario to hold the par shifts
    QuantLib::ext::shared_ptr<Scenario> parScenario =
        addDifferenceToScenario(baseScenario_, zeroScenario, d, baseScenario_->getNumeraire(), true);
    parScenario->setPar(true);
    parScenario->label(to_string(d));

    // get the par shifts and update the par scenario
    auto parShifts = shiftConverter_->parShifts(zeroScenario);
    auto baseRates = shiftConverter_->baseValues();

    for (const auto& kv : parShifts) {
        if (baseScenario_->has(kv.first)) {
            auto base = baseRates[kv.first];
            parScenario->add(kv.first, base + kv.second);
        }
    }

    return parScenario;
}

} // namespace analytics
} // namespace ore
