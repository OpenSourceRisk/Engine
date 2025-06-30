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

#include <ored/configuration/curveconfig.hpp>

namespace ore {
namespace data {

set<string> CurveConfig::requiredCurveIds(const CurveSpec::CurveType& curveType) const {

    if (!requiredIdsInitialized_) {
        populateRequiredIds();
        requiredIdsInitialized_ = true;
    }

    static const set<string> empty;
    auto r = requiredCurveIds_.find(curveType);
    if (r == requiredCurveIds_.end())
        return empty;
    else
        return r->second;
}

set<string> CurveConfig::requiredNames(const MarketObject o, const std::string& configuration) const {

    if (!requiredIdsInitialized_) {
        populateRequiredIds();
        requiredIdsInitialized_ = true;
    }

    static const set<string> empty;
    auto r = requiredNames_.find(std::make_pair(o, configuration));
    if (r != requiredNames_.end())
        return r->second;
    r = requiredNames_.find(std::make_pair(o, std::string()));
    if (r != requiredNames_.end())
        return r->second;
    return empty;
}

map<CurveSpec::CurveType, set<string>> CurveConfig::requiredCurveIds() const {

    if (!requiredIdsInitialized_) {
        populateRequiredIds();
        requiredIdsInitialized_ = true;
    }

    return requiredCurveIds_;
}

map<MarketObject, set<string>> CurveConfig::requiredNames(const std::string& configuration) const {

    if (!requiredIdsInitialized_) {
        populateRequiredIds();
        requiredIdsInitialized_ = true;
    }

    map<MarketObject, set<string>> result;
    for (auto const& [m, s] : requiredNames_) {
        if (m.second.empty() || m.second == configuration)
            result[m.first] = s;
    }

    return result;
}

void CurveConfig::setRequiredCurveIds(const CurveSpec::CurveType& curveType, const set<string>& ids) {
    requiredCurveIds_[curveType] = ids;
}

void CurveConfig::setRequiredCurveIds(const map<CurveSpec::CurveType, set<string>>& ids) { requiredCurveIds_ = ids; }

void CurveConfig::setRequiredNames(const MarketObject o, const std::string& configuration, const set<string>& ids) {
    requiredNames_[std::make_pair(o, configuration)] = ids;
}

void CurveConfig::setRequiredNames(const map<std::pair<MarketObject, std::string>, set<string>>& ids) {
    requiredNames_ = ids;
}

} // namespace data
} // namespace ore
