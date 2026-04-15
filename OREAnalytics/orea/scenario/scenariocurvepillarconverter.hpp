/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

/*! \file orea/scenario/scenariocurvepillar.hpp
    \brief classes for converting IR Future expiries to periods
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariocurvepillar.hpp>
#include <orea/scenario/sensitivityscenariodata.hpp>
#include <ored/configuration/conventions.hpp>

namespace ore {
namespace analytics {

class ScenarioCurvePillarConverter {
public:
    ScenarioCurvePillarConverter() {}

    QuantLib::Period convertFutureExpiryToPeriod(
        const QuantLib::Date& asof, const IrFutureExpiryYearMonth* futureExpiry, const std::string& name,
        const QuantLib::ext::shared_ptr<SensitivityScenarioData::CurveShiftData>& sensitivityData,
        int instrumentIdx) const;

    std::vector<QuantLib::Period>
    convertToPeriodVector(const QuantLib::Date& asof, const std::vector<ScenarioCurvePillar>& pillars, const std::string& name,
                          const QuantLib::ext::shared_ptr<SensitivityScenarioData::CurveShiftData>& sensitivityData,
                          bool allowFutureExpiries, bool parConversionEnabled) const;

private:
    mutable std::map<std::string, QuantLib::ext::shared_ptr<ore::data::FutureConvention>> futureConventions;
};
} // namespace analytics
} // namespace ore