/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <orea/cube/sensitivitycube.hpp>

#include <ored/utilities/log.hpp>

using std::make_pair;

// Ease the notation below
using bm_type = boost::bimap<ore::analytics::RiskFactorKey, QuantLib::Size>;
typedef bm_type::left_map left_map_type;

namespace {

// Utility method for lookup in the various maps
template<class KeyType, class ValueType>
ValueType index(const KeyType& k, const map<KeyType, ValueType>& m) {

    auto it = m.find(k);
    QL_REQUIRE(it != m.end(), "Key, " << k << ", was not found in the sensitivity cube.");
    return it->second;
}

QuantLib::Size index(const ore::analytics::RiskFactorKey& k, const left_map_type& m) {

    auto it = m.find(k);
    QL_REQUIRE(it != m.end(), "Key, " << k << ", was not found in the sensitivity cube.");
    return it->second;
}

// Utility method for lookup in the cross map
typedef ore::analytics::SensitivityCube::crossPair crossPair;

Size crossIndex(const crossPair& k, const map<crossPair, Size>& m) {

    auto it = m.find(k);
    
    // Try the other way around if not found, just in case
    if (it == m.end()) {
        it = m.find(std::make_pair(k.second, k.first));
    }
    
    QL_REQUIRE(it != m.end(), "Key pair, [" << k.first << "," << k.second << 
        "], was not found in the sensitivity cube.");
    
    return it->second;
}

}

namespace ore {
namespace analytics {

SensitivityCube::SensitivityCube(const boost::shared_ptr<NPVSensiCube>& cube,
    const vector<ShiftScenarioDescription>& scenarioDescriptions, 
    const map<RiskFactorKey, QuantLib::Real>& shiftsizes)
    : cube_(cube), scenarioDescriptions_(scenarioDescriptions), shiftSizes_(shiftsizes) {
    initialise();
}

SensitivityCube::SensitivityCube(const boost::shared_ptr<NPVSensiCube>& cube,
    const vector<string>& scenarioDescriptions, 
    const map<RiskFactorKey, QuantLib::Real>& shiftsizes) 
    : cube_(cube), shiftSizes_(shiftsizes) {

    // Populate scenarioDescriptions_ from string descriptions
    scenarioDescriptions_.reserve(scenarioDescriptions.size());
    for (const auto& des : scenarioDescriptions) {
        scenarioDescriptions_.push_back(ShiftScenarioDescription(des));
    }

    initialise();
}

void SensitivityCube::initialise() {
    QL_REQUIRE(scenarioDescriptions_[0].type() == ShiftScenarioDescription::Type::Base,
        "Expected the first scenario in the sensitivity cube to be of type 'Base'");

    // Populate the trade ID lookup map
    for (Size i = 0; i < cube_->numIds(); i++) {
        tradeIdx_[cube_->ids()[i]] = i;
    }

    // Populate the scenario lookup map
    crossPair factorPair;
    for (Size i = 0; i < scenarioDescriptions_.size(); i++) {
        auto des = scenarioDescriptions_[i];
        scenarioIdx_[des] = i;

        // Populate factors_ = list of factors for which we can calculate a delta/gamma
        switch (des.type()) {
        case ShiftScenarioDescription::Type::Up:
            QL_REQUIRE(upFactors_.left.count(des.key1()) == 0, "Cannot have multiple up factors with "
                "the same risk factor key[" << des.key1() << "]");
            factors_.insert(des.key1());
            upFactors_.insert(bm_type::value_type(des.key1(), i));
            break;
        case ShiftScenarioDescription::Type::Down:
            QL_REQUIRE(downFactors_.count(des.key1()) == 0, "Cannot have multiple down factors with "
                "the same risk factor key [" << des.key1() << "]");
            downFactors_[des.key1()] = i;
            break;
        case ShiftScenarioDescription::Type::Cross:
            factorPair = make_pair(des.key1(), des.key2());
            QL_REQUIRE(crossFactors_.count(factorPair) == 0, "Cannot have multiple cross factors with "
                "the same risk factor key pair [" << des.key1() << ", " << des.key2() << "]");
            crossFactors_[factorPair] = i;
            crossPairs_.insert(factorPair);
            break;
        default:
            // Do nothing
            break;
        }
    }

    // Check that up factors and down factors align
    QL_REQUIRE(upFactors_.size() == downFactors_.size(),
        "The number 'Up' shifts should equal the number of 'Down' shifts");

    auto pred = [](decltype(*upFactors_.left.begin()) a, pair<RiskFactorKey, Size> b) { return a.first == b.first; };
    QL_REQUIRE(equal(upFactors_.left.begin(), upFactors_.left.end(), downFactors_.begin(), pred),
        "The set of risk factor keys with an 'Up' shift and 'Down' shift should match");

    // Log warnings if each factor does not have a shift size entry and that it is not a Null<Real>()
    if (upFactors_.size() != shiftSizes_.size()) {
        WLOG("The number of 'Up' shifts (" << upFactors_.size() <<") does not equal " << 
            "the number of shift sizes (" << shiftSizes_.size() << ") supplied");
    }
    
    for (auto const& kv : upFactors_.left) {
        auto it = shiftSizes_.find(kv.first);
        if (it == shiftSizes_.end()) {
            WLOG("No entry for risk factor " << kv.first << " in shift sizes.");
        }
        if (it->second == Null<Real>()) {
            WLOG("The shift size for risk factor " << kv.first << " is not valid.")
        }
    }
}

bool SensitivityCube::hasTrade(const string& tradeId) const {
    return tradeIdx_.count(tradeId) > 0;
}

RiskFactorKey SensitivityCube::upDownFactor(const Size upDownIndex) const {
    auto it = upFactors_.right.find(upDownIndex);
    if (it == upFactors_.right.end())
        return RiskFactorKey();
    else
        return it->second;
}

bool SensitivityCube::hasScenario(const ShiftScenarioDescription& scenarioDescription) const {
    return scenarioIdx_.count(scenarioDescription) > 0;
}

std::string SensitivityCube::factorDescription(const RiskFactorKey & riskFactorKey) const {
    Size scenarioIdx = index(riskFactorKey, upFactors_.left);
    return scenarioDescriptions_[scenarioIdx].factor1();
}

const set<RiskFactorKey>& SensitivityCube::factors() const {
    return factors_;
}

const set<crossPair>& SensitivityCube::crossFactors() const {
    return crossPairs_;
}

Real SensitivityCube::shiftSize(const RiskFactorKey& riskFactorKey) const {
    auto it = shiftSizes_.find(riskFactorKey);
    QL_REQUIRE(it != shiftSizes_.end(), "Risk factor, " << riskFactorKey << ", was not found in the shift sizes.");
    return it->second;
}

Real SensitivityCube::npv(const string& tradeId) const {
    return cube_->getT0(tradeId, 0);
}

Real SensitivityCube::npv(const string& tradeId, const ShiftScenarioDescription& scenarioDescription) const {
    Size scenarioIdx = index(scenarioDescription, scenarioIdx_);
    return cube_->get(tradeId, scenarioIdx);
}

Real SensitivityCube::delta(const string& tradeId, const RiskFactorKey& riskFactorKey) const {
    Size scenarioIdx = index(riskFactorKey, upFactors_.left);
    return cube_->get(tradeId, scenarioIdx) - cube_->getT0(tradeId, 0);
}

Real SensitivityCube::gamma(const std::string& tradeId, const RiskFactorKey& riskFactorKey) const {
    Size upIdx = index(riskFactorKey, upFactors_.left);
    Size downIdx = index(riskFactorKey, downFactors_);

    Real baseNpv = cube_->getT0(tradeId, 0);
    Real upNpv = cube_->get(tradeId, upIdx);
    Real downNpv = cube_->get(tradeId, downIdx);

    return upNpv - 2.0 * baseNpv + downNpv;
}

Real SensitivityCube::crossGamma(const string& tradeId, const crossPair& riskFactorKeyPair) const {
    Size upIdx_1 = index(riskFactorKeyPair.first, upFactors_.left);
    Size upIdx_2 = index(riskFactorKeyPair.second, upFactors_.left);
    Size crossIdx = crossIndex(riskFactorKeyPair, crossFactors_);

    // Approximate f_{xy}|(x,y) by
    // ([f_{x}|(x,y + dy)] - [f_{x}|(x,y)]) / dy
    // ([f(x + dx,y + dy) - f(x, y + dy)] - [f(x + dx,y) - f(x,y)]) / (dx dy)
    Real baseNpv = cube_->getT0(tradeId, 0);
    Real upNpv_1 = cube_->get(tradeId, upIdx_1);
    Real upNpv_2 = cube_->get(tradeId, upIdx_2);
    Real crossNpv = cube_->get(tradeId, crossIdx);

    return crossNpv - upNpv_1 - upNpv_2 + baseNpv;
}

std::set<RiskFactorKey> SensitivityCube::relevantRiskFactors() {
    std::set<RiskFactorKey> result;
    for (auto const i : cube_->relevantScenarios()) {
        result.insert(scenarioDescriptions_[i].key1());
        if(scenarioDescriptions_[i].type() == ShiftScenarioDescription::Type::Cross)
            result.insert(scenarioDescriptions_[i].key2());
    }
    return result;
}
}
}
