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

/*! \file orea/engine/parstressscenarioconverter.hpp
    \brief Convert all par shifts in a single stress test scenario to a zero shifts
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

// Utility function
std::set<RiskFactorKey::KeyType> disabledParRates(bool irCurveParRates, bool irCapFloorParRates, bool creditParRates);

//! Convert all par shifts in a single stress test scenario to zero shifts
class ParStressScenarioConverter {
public:
    ParStressScenarioConverter(
        const QuantLib::Date& asof, const std::vector<RiskFactorKey>& sortedParInstrumentRiskFactorKeys,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters>& simMarketParams,
        const QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData>& sensiScenarioData,
        const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket,
        const ore::analytics::ParSensitivityInstrumentBuilder::Instruments& parInstruments,
        bool useSpreadedTermStructure);

    //! Convert par shifts in a stress scenario to zero shifts
    ore::analytics::StressTestScenarioData::StressTestData
    convertScenario(const StressTestScenarioData::StressTestData& scenario) const;

private:
    //! check if the scenario can be converted
    bool scenarioCanBeConverted(const StressTestScenarioData::StressTestData& parStressScenario) const;
    //! compute the time to tenor of the risk factor key
    double maturityTime(const RiskFactorKey& key) const;

    //! get the strike and tenor from a optionlet riskfactor key
    std::pair<size_t, size_t> getCapFloorTenorAndStrikeIds(const RiskFactorKey& rfKey) const;

    //! convert the scenario value to the corresponding zero shift size for the stress test data
    double shiftsSizeForScenario(const RiskFactorKey rfKey, double targetValue, double baseValue) const;

    //! add zero shifts in the stress test data
    void updateTargetStressTestScenarioData(StressTestScenarioData::StressTestData& stressScenario,
                                            const RiskFactorKey& key, const double zeroShift) const;

    //! Compute the implied fair rate of the par instrument
    double impliedParRate(const RiskFactorKey& key) const;

    //! get the par stress shift size from stress test data
    double getStressShift(const RiskFactorKey& key, const StressTestScenarioData::StressTestData& stressScenario) const;

    double lowerBound(const RiskFactorKey key) const;
    double upperBound(const RiskFactorKey key) const;

    QuantLib::Date asof_;
    const std::vector<RiskFactorKey> sortedParInstrumentRiskFactorKeys_;
    QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarketParameters> simMarketParams_;
    QuantLib::ext::shared_ptr<ore::analytics::SensitivityScenarioData> sensiScenarioData_;
    mutable QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket> simMarket_;
    const ore::analytics::ParSensitivityInstrumentBuilder::Instruments& parInstruments_;
    bool useSpreadedTermStructure_ = true;

    double minVol_ = 1e-8;
    double maxVol_ = 10.0;
    double minDiscountFactor_ = 1e-8;
    double maxDiscountFactor_ = 10.0;
    double accuracy_ = 1e-8;
};
} // namespace analytics
} // namespace ore