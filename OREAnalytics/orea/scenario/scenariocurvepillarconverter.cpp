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

#include <orea/scenario/scenariocurvepillarconverter.hpp>
#include <ored/configuration/conventions.hpp>

namespace ore {
namespace analytics {
using ore::data::InstrumentConventions;
using ore::data::FutureConvention;
using ore::data::Convention;

QuantLib::Period ScenarioCurvePillarConverter::convertFutureExpiryToPeriod(
    const QuantLib::Date& asof, const IrFutureExpiryYearMonth* futureExpiry, const std::string& name,
    const QuantLib::ext::shared_ptr<SensitivityScenarioData::CurveShiftData>& sensitivityData, int instrumentIdx) const {
    if (futureConventions.count(name) == 0) {
        auto parData = QuantLib::ext::dynamic_pointer_cast<SensitivityScenarioData::CurveShiftParData>(sensitivityData);
        QL_REQUIRE(parData, "Curve shift data for "
                                << name
                                << " must contain par conversion data to use IR Future expiries as shift tenors");
        QL_REQUIRE(parData->parInstruments.size() > instrumentIdx && parData->parInstruments[instrumentIdx] == "FUT",
                   "Shift tenor #" << instrumentIdx << " for " << name
                                   << " is an IR Future expiry, but par instrument is not FUT");
        auto it = parData->parInstrumentConventions.find("FUT");
        QL_REQUIRE(it != parData->parInstrumentConventions.end(),
                   "Future convention for " << name << " not found in curve shift data");
        auto [found, conv] = InstrumentConventions::instance().conventions()->get(it->second, Convention::Type::Future);
        QL_REQUIRE(found, "Future convention '" << it->second << "'not found.");
        futureConventions.insert(make_pair(name, QuantLib::ext::dynamic_pointer_cast<FutureConvention>(conv)));
    }
    auto conv = futureConventions.at(name);
    QL_REQUIRE(conv, "Future convention for " << name << " not found");
    return futureExpiry->toPeriod(asof, conv);
}

std::vector<QuantLib::Period> ScenarioCurvePillarConverter::convertToPeriodVector(
    const QuantLib::Date& asof, const std::vector<ScenarioCurvePillar>& pillars, const std::string& name,
    const QuantLib::ext::shared_ptr<SensitivityScenarioData::CurveShiftData>& sensitivityData, bool allowFutureExpiries,
    bool parConversionEnabled) const {
    std::vector<QuantLib::Period> result;
    for (size_t i = 0; i < pillars.size(); ++i) {
        if (auto p = std::get_if<QuantLib::Period>(&pillars[i])) {
            result.push_back(*p);
        } else if (auto p = std::get_if<IrFutureExpiryYearMonth>(&pillars[i])) {
            QL_REQUIRE(allowFutureExpiries, "ExpiryMonthYear shift tenors are not allowed for " << name);
            QL_REQUIRE(parConversionEnabled, "To use ExpiryMonthYear shift tenors enable par conversion in "
                                             "configuration and provide future conventions");
            result.push_back(convertFutureExpiryToPeriod(asof, p, name, sensitivityData, i));
        } else {
            QL_FAIL("unsupported tenor type in shift tenors for " << name);
        }
    }
    return result;
}
} // namespace analytics
} // namespace ore