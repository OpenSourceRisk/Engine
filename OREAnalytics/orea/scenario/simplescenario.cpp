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

#include <orea/scenario/simplescenario.hpp>
#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>
#include <ql/utilities/null.hpp>

#include <boost/make_shared.hpp>

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

    if (data_.size() <= dataIndex)
        data_.resize(dataIndex + 1, QuantLib::Null<Real>());

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

void SimpleScenario::setAbsolute(const bool isAbsolute) { isAbsolute_ = isAbsolute; }

void SimpleScenario::setCoordinates(const RiskFactorKey::KeyType type, const std::string& name,
                                    const std::vector<std::vector<Real>>& coordinates) {
    sharedData_->coordinates[std::make_pair(type, name)] = coordinates;
}

} // namespace analytics
} // namespace ore
