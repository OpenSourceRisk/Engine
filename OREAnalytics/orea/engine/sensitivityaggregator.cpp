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

#include <orea/engine/sensitivityaggregator.hpp>

#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

using ore::analytics::ScenarioFilter;
using std::function;
using std::map;
using std::set;
using std::string;

namespace ore {
namespace analytics {

SensitivityAggregator::SensitivityAggregator(const map<string, set<pair<string, Size>>>& categories)
    : setCategories_(categories) {

    // Initialise the category functions
    for (const auto& kv : setCategories_) {
        categories_[kv.first] = bind(&SensitivityAggregator::inCategory, this, std::placeholders::_1, kv.first);
    }

    // Initialise the categorised records
    init();
}

SensitivityAggregator::SensitivityAggregator(const map<string, function<bool(string)>>& categories)
    : categories_(categories) {

    // Initialise the categorised records
    init();
}

void SensitivityAggregator::aggregate(SensitivityStream& ss, const boost::shared_ptr<ScenarioFilter>& filter) {
    // Ensure at start of stream
    ss.reset();

    // Loop over stream's records
    while (SensitivityRecord sr = ss.next()) {
        // Skip this record if the risk factor is not in the filter
        if (!sr.isCrossGamma() && !filter->allow(sr.key_1))
            continue;
        if (sr.isCrossGamma() && (!filter->allow(sr.key_1) || !filter->allow(sr.key_2)))
            continue;

        // "Blank out" trade ID before adding
        string tradeId = sr.tradeId;
        sr.tradeId = "";

        // Update aggRecords_ for each category
        for (const auto& kv : categories_) {
            // Check if the sensitivity record's trade ID is in the category
            if (kv.second(tradeId)) {
                DLOG("Updating aggregated sensitivities for category " << kv.first << " with record: " << sr);
                add(sr, aggRecords_[kv.first]);
            }
        }
    }
}

void SensitivityAggregator::reset() {
    // Clear the aggregated sensitivities
    aggRecords_.clear();

    // Initialise the categorised records
    init();
}

const set<SensitivityRecord>& SensitivityAggregator::sensitivities(const string& category) const {

    auto it = aggRecords_.find(category);
    QL_REQUIRE(it != aggRecords_.end(),
               "The category " << category << " was not used in the construction of the SensitivityAggregator");

    return it->second;
}

void SensitivityAggregator::init() {
    // Add an empty set for each of the categories
    for (const auto& kv : categories_) {
        aggRecords_[kv.first] = {};
    }
}

void SensitivityAggregator::add(SensitivityRecord& sr, set<SensitivityRecord>& records) {
    // Try to insert sr. This will only pass if sr is not there already.
    auto p = records.insert(sr);
    if (!p.second) {
        // If sr is already in the set, update it.
        p.first->baseNpv += sr.baseNpv;
        p.first->delta += sr.delta;
        p.first->gamma += sr.gamma;
    }
}

bool SensitivityAggregator::inCategory(const string& tradeId, const string& category) const {
    QL_REQUIRE(setCategories_.count(category), "The category " << category << " is not valid");
    auto tradeIds = setCategories_.at(category);
    for (auto it = tradeIds.begin(); it != tradeIds.end(); ++it) {
        if (it->first == tradeId)
            return true;
    }
    return false;
}

} // namespace analytics
} // namespace ore
