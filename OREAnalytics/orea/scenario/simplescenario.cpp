/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>
#include <orea/scenario/simplescenario.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace analytics {

SimpleScenario::SimpleScenario(Date asof, const std::string& label, Real numeraire,
                               const boost::shared_ptr<SharedData>& sharedData)
    : constructedWithSharedData_(sharedData != nullptr),
      sharedData_(sharedData == nullptr ? QuantExt::ext::make_shared<SharedData>() : sharedData), asof_(asof),
      label_(label), numeraire_(numeraire) {}

bool SimpleScenario::has(const RiskFactorKey& key) const {
    return sharedData_->keyIndex.find(key) != sharedData_->keyIndex.end();
}

void SimpleScenario::add(const RiskFactorKey& key, Real value) {

    if (constructedWithSharedData_) {
        QL_REQUIRE(data_.size() < sharedData_->keys.size(),
                   "Key " << key << " (" << value
                          << ") can not be added to SimpleScenario, because it was constructed with shared data and "
                          << sharedData_->keys.size() << " keys, which are all set.");
        QL_REQUIRE(sharedData_->keys[data_.size()] == key,
                   "Key " << key << " (" << value
                          << ") can not be added to SimpleScenario, because it was constructued with shared data from "
                             "which we expect the key "
                          << sharedData_->keys[data_.size()] << " to be set next.");
        data_.push_back(value);
        return;
    }

    if (auto i = sharedData_->keyIndex.find(key); i != sharedData_->keyIndex.end()) {
        // overwrite existing value
        data_[i->second] = value;
    } else {
        // create new value
        QL_REQUIRE(
            sharedData_->keysHash == 0,
            "Key " << key << " (" << value
                   << ") can not be added to SimpleScenario, because the hash of the keys is already generated.");

        sharedData_->keys.push_back(key);
        data_.push_back(value);
        sharedData_->keyIndex[key] = data_.size() - 1;
    }
}

Real SimpleScenario::get(const RiskFactorKey& key) const {
    auto i = sharedData_->keyIndex.find(key);
    QL_REQUIRE(i != sharedData_->keyIndex.end(), "SimpleScenario does not provide data for key " << key);
    // scenario might not have been constructed fully, therefore we have to check
    QL_REQUIRE(data_.size() > i->second, "SimpleScenario data does not cover index "
                                             << i->second << " belonging to key " << key
                                             << ", probably because the scenario was constructed with more keys in the "
                                                "shared data block than afterwards added via add().");
    return data_[i->second];
}

QuantLib::ext::shared_ptr<Scenario> SimpleScenario::clone() const {
    return QuantLib::ext::make_shared<SimpleScenario>(*this);
}

Size SimpleScenario::dimensionality(const RiskFactorKey::KeyType type, const std::string& name) const {
    auto i = sharedData_->dimensionality.find(std::make_pair(type, name));
    QL_REQUIRE(i != sharedData_->dimensionality.end(),
               "SimpleScenario does not provide dimensionality for " << type << ", " << name);
    return i->second;
}

const std::vector<Real>& SimpleScenario::coordinates(const RiskFactorKey::KeyType type, const std::string& name,
                                                     const Size dimension) const {
    auto i = sharedData_->coordinates.find(std::make_pair(type, name));
    QL_REQUIRE(i != sharedData_->coordinates.end(),
               "SimpleScenario does not provide coordinates for " << type << ", " << name);
    QL_REQUIRE(i->second.size() > dimension, "SimpleScenario does not provide coordinates for dimension "
                                                 << dimension << " for " << type << ", " << name);
    return i->second[dimension];
}

void SimpleScenario::setAbsolute(const bool isAbsolute) { sharedData_->isAbsolute = isAbsolute; }

void SimpleScenario::setDimensionality(const RiskFactorKey::KeyType type, const std::string& name,
                                       const Size dimensionality) {
    sharedData_->dimensionality[std::make_pair(type, name)] = dimensionality;
}

void SimpleScenario::setCoordinates(const RiskFactorKey::KeyType type, const std::string& name,
                                    const std::vector<std::vector<Real>> coordinates) {
    sharedData_->coordinates[std::make_pair(type, name)] = coordinates;
}

void SimpleScenario::generateKeysHash() {
    QL_REQUIRE(sharedData_->keysHash == 0, "SimpleScenario::generateKeysHash(): keys hash is already generated.");
    sharedData_->keysHash = boost::hash_range(sharedData_->keys.begin(), sharedData_->keys.end());
}

} // namespace analytics
} // namespace ore
