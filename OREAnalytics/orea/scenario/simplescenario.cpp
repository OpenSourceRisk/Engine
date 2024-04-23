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
    : sharedData_(sharedData == nullptr ? QuantLib::ext::make_shared<SharedData>() : sharedData), asof_(asof),
      label_(label), numeraire_(numeraire) {}

bool SimpleScenario::has(const RiskFactorKey& key) const {
    return sharedData_->keyIndex.find(key) != sharedData_->keyIndex.end();
}

void SimpleScenario::add(const RiskFactorKey& key, Real value) {
    Size dataIndex;
    if (auto i = sharedData_->keyIndex.find(key); i != sharedData_->keyIndex.end()) {
        dataIndex = i->second;
    } else {
        dataIndex = sharedData_->keyIndex[key] = sharedData_->keys.size();
        sharedData_->keys.push_back(key);
        boost::hash_combine(sharedData_->keysHash, key);
    }

    if(data_.size() <= dataIndex)
        data_.resize(dataIndex+1,Null<Real>());
    
    data_[dataIndex] = value;
}

Real SimpleScenario::get(const RiskFactorKey& key) const {
    auto i = sharedData_->keyIndex.find(key);
    QL_REQUIRE(i != sharedData_->keyIndex.end(), "SimpleScenario does not provide data for key " << key);
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

} // namespace analytics
} // namespace ore
