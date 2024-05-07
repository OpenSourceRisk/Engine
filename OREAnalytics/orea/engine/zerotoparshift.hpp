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

/*! \file orea/engine/zerotoparshift.hpp
    \brief applies a zero scenario and return the par instrument shifts
    \ingroup simulation
*/

#pragma once
#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/engine/parsensitivityutilities.hpp>

namespace ore {
namespace analytics {

class ZeroToParShiftConverter {
public:
    ZeroToParShiftConverter(
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

    ZeroToParShiftConverter(const ParSensitivityInstrumentBuilder::Instruments& instruments_,
                            const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket)
        : instruments_(instruments_), simMarket_(simMarket){};

    std::unordered_map<RiskFactorKey, double> parShifts(QuantLib::ext::shared_ptr<Scenario> scenario) const{
        class SimMarketReseter{
        public:
            SimMarketReseter(const ext::shared_ptr<ScenarioSimMarket>& simMarket) : simMarket_(simMarket){
                simMarket_->reset();
            }
            ~SimMarketReseter() { simMarket_->reset(); }
            const ext::shared_ptr<ScenarioSimMarket>& market() const { return simMarket_; }

        private:
            ext::shared_ptr<ScenarioSimMarket> simMarket_;
        };
        QL_REQUIRE(simMarket_ != nullptr, "ZeroToParShiftConverter: need a simmarket");
        SimMarketReseter market(simMarket_);
        auto baseValues = parRates();
        market.market()->applyScenario(scenario);
        auto scenarioValues = parRates();
        // Check if both maps have same keys;
        QL_REQUIRE(baseValues.size() == scenarioValues.size(), "ZeroToParShiftConverter: internal error, both maps should have the same entries");
        bool sameKeys = std::equal(baseValues.begin(), baseValues.end(), scenarioValues.begin(),
                   [](const auto& a, const auto& b) { return a.first == b.first; });
        QL_REQUIRE(sameKeys, "ZeroToParShiftConverter: internal error, both maps should have the same entries");
        std::unordered_map<RiskFactorKey, double> shifts;
        for (const auto& [key, amount] : baseValues) {
            shifts[key] = scenarioValues[key] - amount;
        }
        return shifts;
    }

private:
    std::unordered_map<RiskFactorKey, double> parRates() const { 
        std::unordered_map<RiskFactorKey, double> results; 
        for(const auto& [key, parInstrument] : instruments_.parHelpers_){
            results[key] = impliedQuote(parInstrument);
        }
        for(const auto& [key, cap] : instruments_.parCaps_){
            results[key] = impliedVolatility(key, instruments_);
        }
        for (const auto& [key, cap] : instruments_.parYoYCaps_) {
            results[key] = impliedVolatility(key, instruments_);
        }
        return results;
    }

    ParSensitivityInstrumentBuilder::Instruments instruments_;
    ext::shared_ptr<ScenarioSimMarket> simMarket_;
};

} // namespace analytics
} // namespace ore