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

/*! \file orea/scenario/scenariofilter.hpp
    \brief Scenario Filter classes
    \ingroup scenario
*/

#pragma once

#include <orea/scenario/scenariosimmarket.hpp>

namespace ore {
namespace analytics {

//! Filter that will only allow specified RiskFactorKey::KeyTypes
/*! To create an FX only scenario filter:
 *
 *    ScenarioFilter *sf = new RiskFactorTypeScenarioFilter(
 *        { FXSpot, FXVolatility });
 */
class RiskFactorTypeScenarioFilter : public ScenarioFilter {
public:
    //! Include any RiskFactorKey::KeyTypes
    RiskFactorTypeScenarioFilter(const std::vector<RiskFactorKey::KeyType>& factors) : factors_(factors) {}

    bool allow(const RiskFactorKey& key) const override {
        // return true if we are in the set of factors
        return std::find(factors_.begin(), factors_.end(), key.keytype) != factors_.end();
    }

private:
    std::vector<RiskFactorKey::KeyType> factors_;
};

//! Filter that will only allow specified keys
class RiskFactorScenarioFilter : public ScenarioFilter {
public:
    //! Include only this risk factor
    /*! If ignore index is true, we only check the keytype and name
     */
    RiskFactorScenarioFilter(const RiskFactorKey& key, bool ignoreIndex = true)
        : key_(key), ignoreIndex_(ignoreIndex) {}

    bool allow(const RiskFactorKey& key) const override {
        // Only allow key_ in
        return key.keytype == key_.keytype && key.name == key_.name && (ignoreIndex_ || key.index == key_.index);
    }

private:
    RiskFactorKey key_;
    bool ignoreIndex_;
};

//! Filter for combining the above
class CompositeScenarioFilter : public ScenarioFilter {
public:
    CompositeScenarioFilter() {}
    CompositeScenarioFilter(const vector<ScenarioFilter>& filters) : filters_(filters) {}

    // add a filter
    void add(const ScenarioFilter& filter) { filters_.push_back(filter); }

    //! If any one of the filters allows this key, we allow it.
    bool allow(const RiskFactorKey& key) const override {
        for (auto filter : filters_) {
            if (filter.allow(key))
                return true;
        }
        return false;
    }

private:
    std::vector<ScenarioFilter> filters_;
};

} // namespace analytics
} // namespace oreplus
