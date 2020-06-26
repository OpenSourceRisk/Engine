/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/scenario/spreadscenario.hpp>

#include <ored/utilities/log.hpp>

#include <ql/utilities/null.hpp>

namespace ore {
namespace analytics {

using namespace QuantLib;

SpreadScenario::SpreadScenario(const boost::shared_ptr<ore::analytics::Scenario>& absoluteValues,
                               const boost::shared_ptr<ore::analytics::Scenario>& spreadValues)
    : absoluteValues_(absoluteValues), spreadValues_(spreadValues) {}

const Date& SpreadScenario::asof() const { return absoluteValues_->asof(); }

const std::string& SpreadScenario::label() const { return absoluteValues_->label(); }

void SpreadScenario::label(const string& s) { absoluteValues_->label(s); }

Real SpreadScenario::getNumeraire() const { return absoluteValues_->getNumeraire(); }

void SpreadScenario::setNumeraire(Real n) { absoluteValues_->setNumeraire(n); }

bool SpreadScenario::has(const ore::analytics::RiskFactorKey& key) const { return absoluteValues_->has(key); }

const std::vector<ore::analytics::RiskFactorKey>& SpreadScenario::keys() const { return absoluteValues_->keys(); }

void SpreadScenario::add(const ore::analytics::RiskFactorKey& key, Real value) {
    absoluteValues_->add(key, Null<Real>());
}

Real SpreadScenario::get(const ore::analytics::RiskFactorKey& key) const {
    if (spreadValues_->has(key)) {
        return spreadValues_->get(key);
    } else {
        return absoluteValues_->get(key);
    }
}

boost::shared_ptr<Scenario> SpreadScenario::clone() const {
    return boost::make_shared<SpreadScenario>(absoluteValues_->clone(), spreadValues_->clone());
}

bool SpreadScenario::hasSpreadValue(const RiskFactorKey& key) const { return spreadValues_->has(key); }

void SpreadScenario::addSpreadValue(const RiskFactorKey& key, Real value) { spreadValues_->add(key, value); }

Real SpreadScenario::getAbsoluteValue(const RiskFactorKey& key) const { return absoluteValues_->get(key); }

Real SpreadScenario::getSpreadValue(const RiskFactorKey& key) const { return spreadValues_->get(key); }

} // namespace analytics
} // namespace ore
