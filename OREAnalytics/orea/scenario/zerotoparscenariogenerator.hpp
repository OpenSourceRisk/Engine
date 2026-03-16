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

/*! \file orea/scenario/zerotoparscenariogenerator.hpp
    \brief scenario generator that builds from historical shifts
    \ingroup scenario
 */

#pragma once

#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/engine/zerotoparshift.hpp>
#include <orea/scenario/historicalscenariogenerator.hpp>

namespace ore {
namespace analytics {

//! Zero To Par Scenario Generator
class ZeroToParScenarioGenerator : public HistoricalScenarioGenerator {
public:
    ZeroToParScenarioGenerator(
        const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& historicalScenarioGenerator,
        const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
        const ParSensitivityInstrumentBuilder::Instruments& parInstruments);

    QuantLib::ext::shared_ptr<Scenario> next(const QuantLib::Date& d) override;
    const QuantLib::ext::shared_ptr<Scenario>& baseScenario() const override { return baseParScenario_; }

private:
    QuantLib::ext::shared_ptr<ZeroToParShiftConverter> shiftConverter_;
    QuantLib::ext::shared_ptr<Scenario> baseParScenario_;
};

} // namespace analytics
} // namespace ore
