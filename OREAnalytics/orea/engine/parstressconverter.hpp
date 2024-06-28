/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

/*! \file orea/engine/parstressconverter.hpp
    \brief Convert all par shifts in a stress test to a zero shifts
    \ingroup engine
*/

#pragma once
#include <orea/engine/parsensitivityanalysis.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariosimmarket.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <orea/scenario/stressscenariodata.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/iborfallbackconfig.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>

namespace ore {
namespace analytics {

class ParStressTestConverter {
public:
    ParStressTestConverter(
        const QuantLib::Date& asof, QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>& todaysMarketParams,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
        const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData,
        const QuantLib::ext::shared_ptr<ore::data::CurveConfigurations>& curveConfigs,
        const QuantLib::ext::shared_ptr<ore::data::Market>& todaysMarket,
        const QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig>& iborFallbackConfig);

    //! Convert all par shifts to zero shifts for all scenarios defined in the stresstest
    QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData> convertStressScenarioData(
        const QuantLib::ext::shared_ptr<ore::analytics::StressTestScenarioData>& scenarioData) const;

    //! Creates a SimMarket, aligns the pillars and strikes of sim and sensitivity scenario market, computes par
    //! sensitivites
    std::pair<QuantLib::ext::shared_ptr<ScenarioSimMarket>, QuantLib::ext::shared_ptr<ParSensitivityAnalysis>>
    computeParSensitivity(const std::set<RiskFactorKey::KeyType>& typesDisabled) const;

private:
    //! get a set of risk factors which will be interpreted as zero rate shifts
    std::set<RiskFactorKey::KeyType> zeroRateRiskFactors(bool irCurveParRates, bool irCapFloorParRates,
                                                         bool creditParRates) const;
                                                         
    QuantLib::Date asof_;
    QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters> todaysMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData_;
    QuantLib::ext::shared_ptr<ore::data::CurveConfigurations> curveConfigs_;
    QuantLib::ext::shared_ptr<ore::data::Market> todaysMarket_;
    QuantLib::ext::shared_ptr<ore::data::IborFallbackConfig> iborFallbackConfig_;
};

} // namespace analytics
} // namespace ore
