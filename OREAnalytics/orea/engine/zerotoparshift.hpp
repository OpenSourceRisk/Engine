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
        const std::string& marketConfiguration, const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket);

    ZeroToParShiftConverter(const ParSensitivityInstrumentBuilder::Instruments& instruments_,
                            const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket);

    std::unordered_map<RiskFactorKey, double> parShifts(QuantLib::ext::shared_ptr<Scenario> scenario) const;

private:
    std::unordered_map<RiskFactorKey, double> parRates() const;

    ParSensitivityInstrumentBuilder::Instruments instruments_;
    ext::shared_ptr<ScenarioSimMarket> simMarket_;
    std::unordered_map<RiskFactorKey, double> baseValues_;
};

} // namespace analytics
} // namespace ore