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

/*! \file orea/engine/zerotoparshift.cpp
    \brief applies a zero scenario and return the par instrument shifts
    \ingroup simulation
*/

#include <orea/engine/observationmode.hpp>
#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/engine/parsensitivityutilities.hpp>
#include <orea/engine/zerotoparshift.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace analytics {

ZeroToParShiftConverter::ZeroToParShiftConverter(
    const QuantLib::Date& asof,
    const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
    const ore::analytics::SensitivityScenarioData& sensitivityData,
    const std::set<ore::analytics::RiskFactorKey::KeyType>& typesDisabled,
    const std::set<ore::analytics::RiskFactorKey::KeyType>& parTypes,
    const std::set<ore::analytics::RiskFactorKey>& relevantRiskFactors, const bool continueOnError,
    const std::string& marketConfiguration, const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket)
    : simMarket_(simMarket) {
    ParSensitivityInstrumentBuilder().createParInstruments(instruments_, asof, simMarketParams, sensitivityData,
                                                           typesDisabled, parTypes, relevantRiskFactors,
                                                           continueOnError, marketConfiguration, simMarket);
};

ZeroToParShiftConverter::ZeroToParShiftConverter(const ParSensitivityInstrumentBuilder::Instruments& instruments_,
                                                 const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket)
    : instruments_(instruments_), simMarket_(simMarket) {
    QL_REQUIRE(simMarket_ != nullptr, "ZeroToParShiftConverter: need a simmarket");
    simMarket_->reset();

    if (ObservationMode::instance().mode() == ObservationMode::Mode::Disable) {
        for (auto it : instruments_.parHelpers_)
            it.second->deepUpdate();
        for (auto it : instruments_.parCaps_)
            it.second->deepUpdate();
        for (auto it : instruments_.parYoYCaps_)
            it.second->deepUpdate();
    }
    baseValues_ = parRates();
};

class SimMarketReseter {
public:
    SimMarketReseter(const ext::shared_ptr<ScenarioSimMarket>& simMarket) : simMarket_(simMarket) {
        simMarket_->reset();
    }
    ~SimMarketReseter() { simMarket_->reset(); }
    const ext::shared_ptr<ScenarioSimMarket>& market() const { return simMarket_; }

private:
    ext::shared_ptr<ScenarioSimMarket> simMarket_;
};

std::unordered_map<RiskFactorKey, double>
ZeroToParShiftConverter::parShifts(QuantLib::ext::shared_ptr<Scenario> scenario) const {
    QL_REQUIRE(simMarket_ != nullptr, "ZeroToParShiftConverter: need a simmarket");
    SimMarketReseter market(simMarket_);

    market.market()->applyScenario(scenario);
    
    if (ObservationMode::instance().mode() == ObservationMode::Mode::Disable) {
        for (auto it : instruments_.parHelpers_)
            it.second->deepUpdate();
        for (auto it : instruments_.parCaps_)
            it.second->deepUpdate();
        for (auto it : instruments_.parYoYCaps_)
            it.second->deepUpdate();
    }

    auto scenarioValues = parRates();
    // Check if both maps have same keys;
    QL_REQUIRE(baseValues_.size() == scenarioValues.size(),
               "ZeroToParShiftConverter: internal error, both maps should have the same entries");
    bool sameKeys = std::equal(baseValues_.begin(), baseValues_.end(), scenarioValues.begin(),
                               [](const auto& a, const auto& b) { return a.first == b.first; });
    QL_REQUIRE(sameKeys, "ZeroToParShiftConverter: internal error, both maps should have the same entries");
    std::unordered_map<RiskFactorKey, double> shifts;
    for (const auto& [key, amount] : baseValues_) {
        shifts[key] = scenarioValues[key] - amount;
    }
    return shifts;
}

std::unordered_map<RiskFactorKey, double> ZeroToParShiftConverter::parRates() const {
    std::unordered_map<RiskFactorKey, double> results;
    for (const auto& [key, parInstrument] : instruments_.parHelpers_) {
        results[key] = impliedQuote(parInstrument);
    }
    for (const auto& [key, cap] : instruments_.parCaps_) {
        results[key] = impliedVolatility(key, instruments_);
    }
    for (const auto& [key, cap] : instruments_.parYoYCaps_) {
        results[key] = impliedVolatility(key, instruments_);
    }
    return results;
}

} // namespace analytics
} // namespace ore