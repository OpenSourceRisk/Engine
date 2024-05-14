/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/scenario/scenarioshiftcalculator.hpp>

#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/daycounters/actual360.hpp>

using ore::analytics::parseShiftType;
using ore::analytics::RiskFactorKey;
using ore::analytics::Scenario;
using ore::analytics::ScenarioSimMarketParameters;
using ore::analytics::SensitivityScenarioData;
using ore::analytics::ShiftScenarioGenerator;
using ore::data::parseDayCounter;
using namespace QuantLib;
using std::log;

namespace ore {
namespace analytics {

using RFType = RiskFactorKey::KeyType;
using ShiftData = SensitivityScenarioData::ShiftData;

ScenarioShiftCalculator::ScenarioShiftCalculator(const QuantLib::ext::shared_ptr<SensitivityScenarioData>& sensitivityConfig,
                                                 const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketConfig,
						 const QuantLib::ext::shared_ptr<ore::analytics::ScenarioSimMarket>& simMarket)
    : sensitivityConfig_(sensitivityConfig), simMarketConfig_(simMarketConfig), simMarket_(simMarket) {}

Real ScenarioShiftCalculator::shift(const RiskFactorKey& key, const Scenario& s_1, const Scenario& s_2) const {

    // Get the respective (transformed) scenario values
    Real v_1 = transform(key, s_1.get(key), s_1.asof());
    Real v_2 = transform(key, s_2.get(key), s_2.asof());

    // If for any reason v_1 or v_2 are not finite or nan, log an alert and return 0
    if (!std::isfinite(v_1) || std::isnan(v_1)) {
        ALOG("The scenario value v_1 for key '" << key << "' is " << v_1 << " and is not usable so we are returning 0");
        return 0.0;
    }
    if (!std::isfinite(v_2) || std::isnan(v_2)) {
        ALOG("The scenario value v_2 for key '" << key << "' is " << v_2 << " and is not usable so we are returning 0");
        return 0.0;
    }

    // Get the shift size and type from the sensitivity configuration
    const ShiftData& shiftData = sensitivityConfig_->shiftData(key.keytype, key.name);
    Real shiftSize = shiftData.shiftSize;
    ShiftType shiftType = shiftData.shiftType;

    // If shiftSize is zero, log an alert and return 0 early
    if (close(shiftSize, 0.0)) {
        ALOG("The shift size for key '" << key << "' in sensitivity config is zero");
        return 0.0;
    }

    // Get the multiple of the sensitivity shift size in moving from scenario 1 to 2
    Real result = 0.0;
    if (shiftType == ShiftType::Absolute) {
        result = v_2 - v_1;
    } else {
        if (close(v_1, 0.0)) {
            ALOG("The reference scenario value for key '"
                 << key << "' is zero and the shift is relative so must return a shift of zero");
        } else {
            result = v_2 / v_1 - 1.0;
        }
    }
    result /= shiftSize;

    return result;
}

Real ScenarioShiftCalculator::transform(const RiskFactorKey& key, Real value, const Date& asof) const {

    Period p;
    DayCounter dc = Actual365Fixed();

    switch (key.keytype) {
    case RFType::DiscountCurve:
    case RFType::YieldCurve:
    case RFType::IndexCurve:
        p = simMarketConfig_->yieldCurveTenors(key.name).at(key.index);
        if (simMarket_)
            dc = simMarket_->iborIndex(key.name)->forwardingTermStructure()->dayCounter();
        break;
    case RFType::DividendYield:
        p = simMarketConfig_->equityDividendTenors(key.name).at(key.index);
        if (simMarket_)
            dc = simMarket_->equityDividendCurve(key.name)->dayCounter();
        break;
    case RFType::SurvivalProbability:
        p = simMarketConfig_->defaultTenors(key.name).at(key.index);
        if (simMarket_)
            dc = simMarket_->defaultCurve(key.name)->curve()->dayCounter();
        break;
    default:
        // If we get to here, return untransformed value
        return value;
        break;
    }

    // If we get to here, calculate transformed value
    Time t = dc.yearFraction(asof, asof + p);

    // The way that this is used above should be ok i.e. will always be 0 - 0 when t = 0
    if (close(t, 0.0)) {
        ALOG("The time needed in the denominator of the transform for key '"
             << key << "' is zero so we return a transformed value of zero");
        return 0.0;
    }

    return -log(value) / t;
}

} // namespace analytics
} // namespace ore
