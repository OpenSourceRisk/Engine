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

#include <orea/scenario/scenarioutilities.hpp>

#include <orea/scenario/scenario.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <ored/utilities/log.hpp>
#include <set>
namespace ore {
namespace analytics {

Real getDifferenceScenario(const RiskFactorKey::KeyType keyType, const Real v1, const Real v2) {
    switch (keyType) {
    case RiskFactorKey::KeyType::SwaptionVolatility:
    case RiskFactorKey::KeyType::YieldVolatility:
    case RiskFactorKey::KeyType::OptionletVolatility:
    case RiskFactorKey::KeyType::FXVolatility:
    case RiskFactorKey::KeyType::EquityVolatility:
    case RiskFactorKey::KeyType::CDSVolatility:
    case RiskFactorKey::KeyType::BaseCorrelation:
    case RiskFactorKey::KeyType::ZeroInflationCurve:
    case RiskFactorKey::KeyType::YoYInflationCurve:
    case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility:
    case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility:
    case RiskFactorKey::KeyType::CommodityCurve:
    case RiskFactorKey::KeyType::CommodityVolatility:
    case RiskFactorKey::KeyType::SecuritySpread:
    case RiskFactorKey::KeyType::Correlation:
    case RiskFactorKey::KeyType::CPR:
        return v2 - v1;

    case RiskFactorKey::KeyType::DiscountCurve:
    case RiskFactorKey::KeyType::YieldCurve:
    case RiskFactorKey::KeyType::IndexCurve:
    case RiskFactorKey::KeyType::FXSpot:
    case RiskFactorKey::KeyType::EquitySpot:
    case RiskFactorKey::KeyType::DividendYield:
    case RiskFactorKey::KeyType::SurvivalProbability:
    case RiskFactorKey::KeyType::RecoveryRate:
    case RiskFactorKey::KeyType::CPIIndex:
        return v2 / v1;

    case RiskFactorKey::KeyType::None:
    case RiskFactorKey::KeyType::SurvivalWeight:
    case RiskFactorKey::KeyType::CreditState:
    default:
        QL_FAIL("getDifferenceScenario(): key type "
                << keyType << " not expected, and not covered. This is an internal error, contact dev.");
    };
}

Real addDifferenceToScenario(const RiskFactorKey::KeyType keyType, const Real v, const Real d) {
    switch (keyType) {
    case RiskFactorKey::KeyType::SwaptionVolatility:
    case RiskFactorKey::KeyType::YieldVolatility:
    case RiskFactorKey::KeyType::OptionletVolatility:
    case RiskFactorKey::KeyType::FXVolatility:
    case RiskFactorKey::KeyType::EquityVolatility:
    case RiskFactorKey::KeyType::CDSVolatility:
    case RiskFactorKey::KeyType::BaseCorrelation:
    case RiskFactorKey::KeyType::ZeroInflationCurve:
    case RiskFactorKey::KeyType::YoYInflationCurve:
    case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility:
    case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility:
    case RiskFactorKey::KeyType::CommodityCurve:
    case RiskFactorKey::KeyType::CommodityVolatility:
    case RiskFactorKey::KeyType::SecuritySpread:
    case RiskFactorKey::KeyType::Correlation:
    case RiskFactorKey::KeyType::CPR:
        return v + d;

    case RiskFactorKey::KeyType::DiscountCurve:
    case RiskFactorKey::KeyType::YieldCurve:
    case RiskFactorKey::KeyType::IndexCurve:
    case RiskFactorKey::KeyType::FXSpot:
    case RiskFactorKey::KeyType::EquitySpot:
    case RiskFactorKey::KeyType::DividendYield:
    case RiskFactorKey::KeyType::SurvivalProbability:
    case RiskFactorKey::KeyType::RecoveryRate:
    case RiskFactorKey::KeyType::CPIIndex:
        return v * d;

    case RiskFactorKey::KeyType::None:
    case RiskFactorKey::KeyType::SurvivalWeight:
    case RiskFactorKey::KeyType::CreditState:
    default:
        QL_FAIL("addDifferenceToScenario(): key type "
                << keyType << " not expected, and not covered. This is an internal error, contact dev.");
    };
}

QuantLib::ext::shared_ptr<Scenario> getDifferenceScenario(const QuantLib::ext::shared_ptr<Scenario>& s1,
                                                          const QuantLib::ext::shared_ptr<Scenario>& s2,
                                                          const Date& targetScenarioAsOf,
                                                          const Real targetScenarioNumeraire) {

    QL_REQUIRE(s1->isAbsolute() && s2->isAbsolute(), "getDifferenceScenario(): both scenarios must be absolute ("
                                                         << std::boolalpha << s1->isAbsolute() << ", "
                                                         << s2->isAbsolute());

    QL_REQUIRE(s1->keysHash() == s2->keysHash(),
               "getDifferenceScenario(): both scenarios must have identical key sets");

    Date asof = targetScenarioAsOf;
    if (asof == Date() && s1->asof() == s2->asof())
        asof = s1->asof();

    QL_REQUIRE(asof != Date(), "getDifferenceScenario(): either both scenarios have to have the same asof date ("
                                   << s1->asof() << ", " << s2->asof()
                                   << ") or the target scenario asof date must be given.");

    auto result = s1->clone();
    result->setAsof(asof);
    result->label("differenceScenario(" + s1->label() + "," + s2->label() + ")");
    result->setNumeraire(targetScenarioNumeraire);
    result->setAbsolute(false);

    for (auto const& k : s1->keys()) {
        result->add(k, getDifferenceScenario(k.keytype, s1->get(k), s2->get(k)));
    }

    return result;
}

QuantLib::ext::shared_ptr<Scenario> addDifferenceToScenario(const QuantLib::ext::shared_ptr<Scenario>& s,
                                                            const QuantLib::ext::shared_ptr<Scenario>& d,
                                                            const Date& targetScenarioAsOf,
                                                            const Real targetScenarioNumeraire) {

    QL_REQUIRE(!d->isAbsolute(), "addDifferenceToScenario(): second argument must be difference scenario");
    QL_REQUIRE(s->keysHash() == d->keysHash(),
               "addDifferenceToScenario(): both scenarios must have identical key sets.");

    Date asof = targetScenarioAsOf;
    if (asof == Date() && s->asof() == d->asof())
        asof = s->asof();

    QL_REQUIRE(asof != Date(), "addDifferenceToScenario(): either both scenarios have to have the same asof date ("
                                   << s->asof() << ", " << d->asof()
                                   << ") or the target scenario asof date must be given.");

    auto result = s->clone();
    result->setAsof(asof);
    result->label("sumScenario(" + s->label() + "," + d->label() + ")");
    result->setNumeraire(targetScenarioNumeraire);
    result->setAbsolute(s->isAbsolute());

    for (auto const& k : s->keys()) {
        result->add(k, addDifferenceToScenario(k.keytype, s->get(k), d->get(k)));
    }

    return result;
}

namespace {

Size getKeyIndex(const std::vector<std::vector<Real>>& oldCoordinates, const std::vector<Size>& indices) {
    Size resultIndex = 0;
    Size multiplier = 1;
    int workingIndex = indices.size() - 1;
    do {
        resultIndex += multiplier * indices[workingIndex];
        multiplier *= oldCoordinates[workingIndex].size();
    } while (--workingIndex >= 0);
    return resultIndex;
}

double interpolatedValue(const std::vector<std::vector<Real>>& oldCoordinates,
                         const std::vector<std::vector<Real>>& newCoordinates, const std::vector<Size>& newIndex,
                         const std::pair<RiskFactorKey::KeyType, std::string>& key,
                         const QuantLib::ext::shared_ptr<Scenario>& scenario) {

    Real w0, w1;
    std::vector<Size> oldIndex0(oldCoordinates.size());
    std::vector<Size> oldIndex1(oldCoordinates.size());

    for (Size i = 0; i < oldCoordinates.size(); ++i) {
        Size idx = std::distance(
            oldCoordinates[i].begin(),
            std::upper_bound(oldCoordinates[i].begin(), oldCoordinates[i].end(), newCoordinates[i][newIndex[i]]));
        if (idx == 0) {
            w0 = 1.0;
            w1 = 0.0;
            oldIndex0[i] = oldIndex1[i] = 0;
        } else if (idx == oldCoordinates[i].size()) {
            w0 = 0.0;
            w1 = 1.0;
            oldIndex0[i] = oldIndex1[i] = idx - 1;
        } else {
            oldIndex0[i] = idx - 1;
            oldIndex1[i] = idx;
            w1 = (newCoordinates[i][newIndex[i]] - oldCoordinates[i][idx - 1]) /
                 (oldCoordinates[i][idx] - oldCoordinates[i][idx - 1]);
            w0 = 1.0 - w1;
        }
    }

    Size keyIndex0 = getKeyIndex(oldCoordinates, oldIndex0);
    Size keyIndex1 = getKeyIndex(oldCoordinates, oldIndex1);
    try {
        return w0 * scenario->get(RiskFactorKey(key.first, key.second, keyIndex0)) +
               w1 * scenario->get(RiskFactorKey(key.first, key.second, keyIndex1));
    } catch (const std::exception& e) {
        QL_FAIL("recastScenario(): error while interpolating between "
                << key.first << "/" << key.second << "/[" << keyIndex0 << "," << keyIndex1 << "]: " << e.what());
    }
}

} // namespace

QuantLib::ext::shared_ptr<Scenario> recastScenario(
    const QuantLib::ext::shared_ptr<Scenario>& scenario,
    const std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>& oldCoordinates,
    const std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>& newCoordinates) {

    auto result = QuantLib::ext::make_shared<SimpleScenario>(
        scenario->asof(), scenario->label() + " (mapped to new coordinates)", scenario->getNumeraire());
    result->setAbsolute(scenario->isAbsolute());

    std::set<std::pair<RiskFactorKey::KeyType, std::string>> keys;
    for (auto const& k : scenario->keys()) {
        if(newCoordinates.count({k.keytype, k.name})==1){
            keys.insert(std::make_pair(k.keytype, k.name));
            TLOG("Insert keys " << k.keytype << " " << k.name)
        } else{
            TLOG("Recast skip " << k.keytype << " " << k.name);
        }
    }



    for (auto const& k : keys) {

        auto c0 = oldCoordinates.find(k);
        auto c1 = newCoordinates.find(k);
        QL_REQUIRE(c0 != scenario->coordinates().end(), "recastScenario(): no coordinates for "
                                                            << k.first << "/" << k.second
                                                            << " found in old coordinates. This is unexpected.");
        QL_REQUIRE(c1 != scenario->coordinates().end(), "recastScenario(): no coordinates for "
                                                            << k.first << "/" << k.second
                                                            << " found in new coordinates. This is unexpected.");
        QL_REQUIRE(c0->second.size() == c1->second.size(), "recastScenario(): number of dimensions in old ("
                                                               << c0->second.size() << ") and new ("
                                                               << c1->second.size() << ") coordinates for " << k.first
                                                               << "/" << k.second << " do not match.");
        if (c1->second.size() == 0) {

            // nothing to interpolate, just copy the single value associated to a rf key

            RiskFactorKey key(k.first, k.second, 0);
            result->add(key, scenario->get(key));

        } else {
            // interpolate new values from old values
            Size newKeyIndex = 0;
            std::vector<Size> indices(c0->second.size(), 0);
            int workingIndex;
            do {
                workingIndex = indices.size() - 1;
                while (workingIndex >= 0 && indices[workingIndex] == c1->second[workingIndex].size()) {
                    --workingIndex;
                    if (workingIndex >= 0)
                        indices[workingIndex] = 0;
                }
                if (workingIndex >= 0) {
                    RiskFactorKey key(k.first, k.second, newKeyIndex++);
                    auto iValue = interpolatedValue(c0->second, c1->second, indices, k, scenario);
                    TLOG("Add "<< key <<  " interpolated value = " << iValue);
                    result->add(key, iValue);
                    indices[workingIndex]++;
                }
            } while (workingIndex >= 0);
        }
    }
    return result;
}

QuantLib::ext::shared_ptr<Scenario> recastScenario(
    const QuantLib::ext::shared_ptr<Scenario>& scenario,
    const std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>>& oldCoordinates,
    const std::set<std::tuple<RiskFactorKey::KeyType, std::string, std::vector<std::vector<Real>>>>& newCoordinates) {

    std::map<std::pair<RiskFactorKey::KeyType, std::string>, std::vector<std::vector<Real>>> newCoordinatesMap;
    for (const auto& [key, name, coordinates] : newCoordinates) {
        newCoordinatesMap[{key, name}] = coordinates;
    }
    return recastScenario(scenario, oldCoordinates, newCoordinatesMap);
}

} // namespace analytics
} // namespace ore
