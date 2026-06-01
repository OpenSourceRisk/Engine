/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef orea_riskfilter_i
#define orea_riskfilter_i

%include std_set.i
%include stl.i
%include orea_scenario_ext.i

%{
#include <orea/engine/riskfilter.hpp>
%}

%shared_ptr(ore::analytics::RiskFilter)

%template(RiskClassSet) std::set<ore::analytics::MarketRiskConfiguration::RiskClass>;
%template(RiskTypeSet)  std::set<ore::analytics::MarketRiskConfiguration::RiskType>;

namespace ore {
namespace analytics {

class MarketRiskConfiguration {
public:
    virtual ~MarketRiskConfiguration() {}

    enum class RiskClass { All, InterestRate, Inflation, Credit, Equity, FX, Commodity };
    enum class RiskType  { All, DeltaGamma, Vega, BaseCorrelation };

    static std::set<RiskClass> riskClasses(bool includeAll = false);
    static std::set<RiskType>  riskTypes(bool includeAll = false);
};

MarketRiskConfiguration::RiskClass parseVarRiskClass(const std::string& rc);
MarketRiskConfiguration::RiskType  parseVarRiskType(const std::string& rt);

class RiskFilter : public ScenarioFilter {
public:
    RiskFilter(const MarketRiskConfiguration::RiskClass& riskClass,
               const MarketRiskConfiguration::RiskType& riskType);
    bool allow(const QuantExt::RiskFactorKey& t) const override;
};

} // namespace analytics
} // namespace ore

#endif
