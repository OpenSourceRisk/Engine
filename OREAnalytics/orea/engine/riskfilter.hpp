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

#include <orea/scenario/scenario.hpp>

#include <ql/types.hpp>

#include <set>
#include <string>
#include <vector>

using QuantLib::Size;

namespace ore {
namespace analytics {

class RiskFilter {
public:
    //! for both risk class and risk type, index 0 stands for "all"
    RiskFilter(const Size riskClassIndex, const Size riskTypeIndex);

    bool allowed(const RiskFactorKey::KeyType& t) const;
    const std::string& riskClassLabel() const { return riskClassLabel_[riskClassIndex_ - 1]; }
    const std::string& riskTypeLabel() const { return riskTypeLabel_[riskTypeIndex_ - 1]; }

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
