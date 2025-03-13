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

set<string> CurveConfig::requiredCurveIds(const CurveSpec::CurveType& curveType) {
    static const set<string> empty;
    auto r = requiredCurveIds_.find(curveType);
    if (r == requiredCurveIds_.end())
        return empty;
    else
        return r->second;
}

map<CurveSpec::CurveType, set<string>> CurveConfig::requiredCurveIds() { return requiredCurveIds_; }

void CurveConfig::setRequiredCurveIds(const CurveSpec::CurveType& curveType, const set<string>& ids) {
    requiredCurveIds_[curveType] = ids;
}

void CurveConfig::setRequiredCurveIds(const map<CurveSpec::CurveType, set<string>>& ids) { requiredCurveIds_ = ids; }

} // namespace data
} // namespace ore
