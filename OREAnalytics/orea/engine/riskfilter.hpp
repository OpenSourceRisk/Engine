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

class MarketRiskConfiguration {
public:
    virtual ~MarketRiskConfiguration() {}

    /*! Risk class types in VaR plus an All type for convenience

        \warning The ordering here matters. It is used in indexing in to
                 correlation matrices for the correlation between risk classes.

        \warning Internal methods rely on the first element being 'All'
    */
    enum class RiskClass { All, InterestRate, Inflation, Credit, Equity, FX, Commodity};

    /*! Risk Type types in VaR plus an All type for convenience
        \warning Internal methods rely on the first element being 'All'
    */
    enum class RiskType { All, DeltaGamma, Vega, BaseCorrelation };

    //! Give back a set containing the RiskClass values optionally excluding 'All'
    static std::set<RiskClass> riskClasses(bool includeAll = false);

    //! Give back a set containing the RiskType values optionally excluding 'All'
    static std::set<RiskType> riskTypes(bool includeAll = false);
};

std::ostream& operator<<(std::ostream& out, const MarketRiskConfiguration::RiskClass& rc);
std::ostream& operator<<(std::ostream& out, const MarketRiskConfiguration::RiskType& rt);

MarketRiskConfiguration::RiskClass parseVarRiskClass(const std::string& rc);
MarketRiskConfiguration::RiskType parseVarRiskType(const std::string& rt);

//! Risk Filter
/*! The risk filter class groups risk factor keys w.r.t. a risk class (IR, FX, EQ...) and a risk type (delta-gamma,
 * vega, ...). It can e.g. be used to break down a var report.
 */
class RiskFilter : public ScenarioFilter {
public:
    RiskFilter(const MarketRiskConfiguration::RiskClass& riskClass, const MarketRiskConfiguration::RiskType& riskType);
    bool allow(const RiskFactorKey& t) const override;

private:
    std::set<RiskFactorKey::KeyType> allowed_;
    bool neg_;
};

} // namespace analytics
} // namespace ore
