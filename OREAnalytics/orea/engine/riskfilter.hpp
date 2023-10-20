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

/*! \file riskfilter.hpp
    \brief risk class and type filter
*/

#pragma once

#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariosimmarket.hpp>

#include <ql/types.hpp>

#include <set>
#include <string>
#include <vector>

namespace ore {
namespace analytics {
using QuantLib::Size;

//! Risk Filter
/*! The risk filter class groups risk factor keys w.r.t. a risk class (IR, FX, EQ...) and a risk type (delta-gamma,
 * vega, ...). It can e.g. be used to break down a var report.
 */
class RiskFilter : public ScenarioFilter {
public:
    //! for both risk class and risk type, index 0 stands for "all"
    RiskFilter(const Size riskClassIndex, const Size riskTypeIndex);

    bool allow(const RiskFactorKey& t) const override;
    const std::string& riskClassLabel() const { return riskClassLabel_[riskClassIndex_]; }
    const std::string& riskTypeLabel() const { return riskTypeLabel_[riskTypeIndex_]; }

    static Size numberOfRiskClasses() { return riskClassLabel_.size(); }
    static Size numberOfRiskTypes() { return riskTypeLabel_.size(); }

private:
    static const std::vector<std::string> riskClassLabel_;
    static const std::vector<std::string> riskTypeLabel_;
    const Size riskClassIndex_, riskTypeIndex_;
    std::set<RiskFactorKey::KeyType> allowed_;
    bool neg_;
};

} // namespace analytics
} // namespace ore
