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

#include <ored/utilities/to_string.hpp>
#include <orea/engine/riskfilter.hpp>

#include <ql/errors.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>

namespace ore {
namespace analytics {
    
using std::string;
using std::ostream;
using boost::assign::list_of;

// custom comparator for bimaps
struct string_cmp {
    bool operator()(const string& lhs, const string& rhs) const {
        return ((boost::to_lower_copy(lhs)) < (boost::to_lower_copy(rhs)));
    }
};

// Ease the notation below
template <typename T> using bm = boost::bimap<T, boost::bimaps::set_of<string, string_cmp>>;

// Initialise the bimaps
const bm<MarketRiskConfiguration::RiskClass> riskClassMap = list_of<bm<MarketRiskConfiguration::RiskClass>::value_type>(
    MarketRiskConfiguration::RiskClass::All, "All")
        (MarketRiskConfiguration::RiskClass::InterestRate, "InterestRate")
        (MarketRiskConfiguration::RiskClass::Inflation, "Inflation")
        (MarketRiskConfiguration::RiskClass::Credit, "Credit")
        (MarketRiskConfiguration::RiskClass::Equity, "Equity")
        (MarketRiskConfiguration::RiskClass::FX, "FX");

const bm<MarketRiskConfiguration::RiskType> riskTypeMap = list_of<bm<MarketRiskConfiguration::RiskType>::value_type>(
    MarketRiskConfiguration::RiskType::All, "All")(
    MarketRiskConfiguration::RiskType::DeltaGamma, "DeltaGamma")(MarketRiskConfiguration::RiskType::Vega, "Vega")(
    MarketRiskConfiguration::RiskType::BaseCorrelation, "BaseCorrelation");

ostream& operator<<(ostream& out, const MarketRiskConfiguration::RiskClass& rc) {
    QL_REQUIRE(riskClassMap.left.count(rc) > 0,
               "Risk class (" << static_cast<int>(rc) << ") not a valid MarketRiskConfiguration::RiskClass");
    return out << riskClassMap.left.at(rc);
}

ostream& operator<<(ostream& out, const MarketRiskConfiguration::RiskType& mt) {
    QL_REQUIRE(riskTypeMap.left.count(mt) > 0,
               "Risk type (" << static_cast<int>(mt) << ") not a valid MarketRiskConfiguration::RiskType");
    return out << riskTypeMap.left.at(mt);
}

MarketRiskConfiguration::RiskClass parseVarRiskClass(const string& rc) {
    QL_REQUIRE(riskClassMap.right.count(rc) > 0,
               "Risk class string " << rc << " does not correspond to a valid MarketRiskConfiguration::RiskClass");
    return riskClassMap.right.at(rc);
}

MarketRiskConfiguration::RiskType parseVarMarginType(const string& mt) {
    QL_REQUIRE(riskTypeMap.right.count(mt) > 0,
               "Risk type string " << mt << " does not correspond to a valid MarketRiskConfiguration::RiskType");
    return riskTypeMap.right.at(mt);
}

//! Give back a set containing the RiskClass values optionally excluding 'All'
std::set<MarketRiskConfiguration::RiskClass> MarketRiskConfiguration::riskClasses(bool includeAll) {
    Size numberOfRiskClasses = riskClassMap.size();

    // Return the set of values
    set<RiskClass> result;
    for (Size i = includeAll ? 0 : 1; i < numberOfRiskClasses; ++i)
        result.insert(RiskClass(i));
    
    return result;
}

//! Give back a set containing the RiskType values optionally excluding 'All'
std::set<MarketRiskConfiguration::RiskType> MarketRiskConfiguration::riskTypes(bool includeAll) {
    Size numberOfRiskTypes = riskTypeMap.size();

    // Return the set of values
    set<RiskType> result;
    for (Size i = includeAll ? 0 : 1; i < numberOfRiskTypes; ++i)
        result.insert(RiskType(i));
    
    return result;
}

RiskFilter::RiskFilter(const MarketRiskConfiguration::RiskClass& riskClass, const MarketRiskConfiguration::RiskType& riskType) {

    static const std::set<RiskFactorKey::KeyType> all = {RiskFactorKey::KeyType::DiscountCurve,
                                                         RiskFactorKey::KeyType::YieldCurve,
                                                         RiskFactorKey::KeyType::IndexCurve,
                                                         RiskFactorKey::KeyType::SwaptionVolatility,
                                                         RiskFactorKey::KeyType::OptionletVolatility,
                                                         RiskFactorKey::KeyType::FXSpot,
                                                         RiskFactorKey::KeyType::FXVolatility,
                                                         RiskFactorKey::KeyType::EquitySpot,
                                                         RiskFactorKey::KeyType::EquityVolatility,
                                                         RiskFactorKey::KeyType::DividendYield,
                                                         RiskFactorKey::KeyType::SurvivalProbability,
                                                         RiskFactorKey::KeyType::RecoveryRate,
                                                         RiskFactorKey::KeyType::CDSVolatility,
                                                         RiskFactorKey::KeyType::BaseCorrelation,
                                                         RiskFactorKey::KeyType::CPIIndex,
                                                         RiskFactorKey::KeyType::ZeroInflationCurve,
                                                         RiskFactorKey::KeyType::YoYInflationCurve,
                                                         RiskFactorKey::KeyType::YoYInflationCapFloorVolatility,
                                                         RiskFactorKey::KeyType::SecuritySpread,
                                                         RiskFactorKey::KeyType::YieldVolatility};

    std::set<RiskFactorKey::KeyType> allowed_type, allowed;

    if (riskType == MarketRiskConfiguration::RiskType::All) {
        allowed_type = all;
    } else {
        switch (riskType) {
        case MarketRiskConfiguration::RiskType::DeltaGamma:
            allowed_type = {RiskFactorKey::KeyType::DiscountCurve,
                            RiskFactorKey::KeyType::YieldCurve,
                            RiskFactorKey::KeyType::IndexCurve,
                            RiskFactorKey::KeyType::FXSpot,
                            RiskFactorKey::KeyType::EquitySpot,
                            RiskFactorKey::KeyType::DividendYield,
                            RiskFactorKey::KeyType::SurvivalProbability, 
                            RiskFactorKey::KeyType::CommodityCurve,
                            RiskFactorKey::KeyType::RecoveryRate,
                            RiskFactorKey::KeyType::CPIIndex,
                            RiskFactorKey::KeyType::ZeroInflationCurve,
                            RiskFactorKey::KeyType::YoYInflationCurve,
                            RiskFactorKey::KeyType::SecuritySpread};
            break;
        case MarketRiskConfiguration::RiskType::Vega:
            allowed_type = {RiskFactorKey::KeyType::SwaptionVolatility,
                            RiskFactorKey::KeyType::OptionletVolatility,
                            RiskFactorKey::KeyType::FXVolatility,
                            RiskFactorKey::KeyType::EquityVolatility,
                            RiskFactorKey::KeyType::CDSVolatility,
                            RiskFactorKey::KeyType::CommodityVolatility,
                            RiskFactorKey::KeyType::YieldVolatility,
                            RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility,
                            RiskFactorKey::KeyType::YoYInflationCapFloorVolatility};
            break;
        case MarketRiskConfiguration::RiskType::BaseCorrelation:
            allowed_type = {RiskFactorKey::KeyType::BaseCorrelation};
            break;
        default:
            QL_FAIL("unexpected riskTypeIndex " << riskType);
        }
    }

    if (riskClass == MarketRiskConfiguration::RiskClass::All) {
        allowed = allowed_type;
    } else {
        std::set<RiskFactorKey::KeyType> allowed_class;
        switch (riskClass) {
        case MarketRiskConfiguration::RiskClass::InterestRate:
            allowed_class = {
                RiskFactorKey::KeyType::DiscountCurve,       RiskFactorKey::KeyType::YieldCurve,
                RiskFactorKey::KeyType::IndexCurve,          RiskFactorKey::KeyType::SwaptionVolatility,
                RiskFactorKey::KeyType::OptionletVolatility, RiskFactorKey::KeyType::SecuritySpread,
                RiskFactorKey::KeyType::YieldVolatility,     RiskFactorKey::KeyType::YoYInflationCapFloorVolatility};
            break;
        case MarketRiskConfiguration::RiskClass::Inflation:
            allowed_class = {RiskFactorKey::KeyType::CPIIndex, RiskFactorKey::KeyType::ZeroInflationCurve,
                             RiskFactorKey::KeyType::YoYInflationCurve};
            break;
        case MarketRiskConfiguration::RiskClass::Credit:
            allowed_class = {RiskFactorKey::KeyType::SurvivalProbability, RiskFactorKey::KeyType::RecoveryRate,
                             RiskFactorKey::KeyType::CDSVolatility, RiskFactorKey::KeyType::BaseCorrelation};
            break;
        case MarketRiskConfiguration::RiskClass::Equity:
            allowed_class = {RiskFactorKey::KeyType::EquitySpot, RiskFactorKey::KeyType::EquityVolatility,
                             RiskFactorKey::KeyType::DividendYield};
            break;
        case MarketRiskConfiguration::RiskClass::FX:
            allowed_class = {RiskFactorKey::KeyType::FXSpot, RiskFactorKey::KeyType::FXVolatility};
            break;
        default:
            QL_FAIL("unexpected riskClassIndex " << riskClass);
        }
        std::set_intersection(allowed_type.begin(), allowed_type.end(), allowed_class.begin(), allowed_class.end(),
                              std::inserter(allowed, allowed.begin()));
    }
    // use the complement if smaller, the check is called frequently
    if (allowed.size() > all.size() / 2) {
        std::set_difference(all.begin(), all.end(), allowed.begin(), allowed.end(),
                            std::inserter(allowed_, allowed_.begin()));
        neg_ = true;
    } else {
        allowed_ = allowed;
        neg_ = false;
    }
}

bool RiskFilter::allow(const RiskFactorKey& t) const {
    bool found = std::find(allowed_.begin(), allowed_.end(), t.keytype) != allowed_.end();
    return neg_ ? !found : found;
}

} // namespace analytics
} // namespace ore
