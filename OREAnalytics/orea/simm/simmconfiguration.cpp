/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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
#include <orea/simm/simmconfiguration.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>

#include <map>

namespace ore {
namespace analytics {

using boost::assign::list_of;
using std::map;
using std::ostream;
using std::set;
using std::string;
using std::vector;

using namespace QuantLib;
using ore::data::to_string;

// Following bimaps ease the conversion from enum to string and back
// It also puts in one place a mapping from string to enum value

// custom comparator for bimaps
struct string_cmp {
    bool operator()(const string& lhs, const string& rhs) const {
        return ((boost::to_lower_copy(lhs)) < (boost::to_lower_copy(rhs)));
    }
};

// Ease the notation below
template <typename T> using bm = boost::bimap<T, boost::bimaps::set_of<std::string, string_cmp>>;

// Initialise the bimaps
const bm<SimmConfiguration::RiskClass> riskClassMap =
    list_of<bm<SimmConfiguration::RiskClass>::value_type>(SimmConfiguration::RiskClass::InterestRate, "InterestRate")(
        SimmConfiguration::RiskClass::CreditQualifying,
        "CreditQualifying")(SimmConfiguration::RiskClass::CreditNonQualifying, "CreditNonQualifying")(
        SimmConfiguration::RiskClass::Equity, "Equity")(SimmConfiguration::RiskClass::Commodity, "Commodity")(
        SimmConfiguration::RiskClass::FX, "FX")(SimmConfiguration::RiskClass::All, "All");

const bm<SimmConfiguration::RiskType> riskTypeMap = list_of<bm<SimmConfiguration::RiskType>::value_type>(
    SimmConfiguration::RiskType::Commodity, "Risk_Commodity")(SimmConfiguration::RiskType::CommodityVol,
                                                              "Risk_CommodityVol")(
    SimmConfiguration::RiskType::CreditNonQ, "Risk_CreditNonQ")(SimmConfiguration::RiskType::CreditQ, "Risk_CreditQ")(
    SimmConfiguration::RiskType::CreditVol, "Risk_CreditVol")(SimmConfiguration::RiskType::CreditVolNonQ,
                                                              "Risk_CreditVolNonQ")(
    SimmConfiguration::RiskType::Equity, "Risk_Equity")(SimmConfiguration::RiskType::EquityVol, "Risk_EquityVol")(
    SimmConfiguration::RiskType::FX, "Risk_FX")(SimmConfiguration::RiskType::FXVol, "Risk_FXVol")(
    SimmConfiguration::RiskType::Inflation, "Risk_Inflation")(SimmConfiguration::RiskType::IRCurve, "Risk_IRCurve")(
    SimmConfiguration::RiskType::IRVol, "Risk_IRVol")(SimmConfiguration::RiskType::InflationVol, "Risk_InflationVol")(
    SimmConfiguration::RiskType::BaseCorr, "Risk_BaseCorr")(SimmConfiguration::RiskType::XCcyBasis, "Risk_XCcyBasis")(
    SimmConfiguration::RiskType::ProductClassMultiplier,
    "Param_ProductClassMultiplier")(SimmConfiguration::RiskType::AddOnNotionalFactor, "Param_AddOnNotionalFactor")(
    SimmConfiguration::RiskType::Notional, "Notional")(SimmConfiguration::RiskType::AddOnFixedAmount,
                                                       "Param_AddOnFixedAmount")(SimmConfiguration::RiskType::PV,
                                                                                 "PV") // IM Schedule
    (SimmConfiguration::RiskType::All, "All");

const bm<SimmConfiguration::MarginType> marginTypeMap =
    list_of<bm<SimmConfiguration::MarginType>::value_type>(SimmConfiguration::MarginType::Delta, "Delta")(
        SimmConfiguration::MarginType::Vega, "Vega")(SimmConfiguration::MarginType::Curvature, "Curvature")(
        SimmConfiguration::MarginType::BaseCorr, "BaseCorr")(SimmConfiguration::MarginType::AdditionalIM, "AdditionalIM")(
        SimmConfiguration::MarginType::All, "All");

const bm<SimmConfiguration::ProductClass> productClassMap =
    list_of<bm<SimmConfiguration::ProductClass>::value_type>(SimmConfiguration::ProductClass::RatesFX, "RatesFX")(
        SimmConfiguration::ProductClass::Rates, "Rates")(SimmConfiguration::ProductClass::FX, "FX")(
        SimmConfiguration::ProductClass::Credit, "Credit")(SimmConfiguration::ProductClass::Equity, "Equity")(
        SimmConfiguration::ProductClass::Commodity, "Commodity")(SimmConfiguration::ProductClass::Other, "Other")(
        SimmConfiguration::ProductClass::Empty, "")(SimmConfiguration::ProductClass::All, "All")(
        SimmConfiguration::ProductClass::AddOnNotionalFactor, "AddOnNotionalFactor")(
        SimmConfiguration::ProductClass::AddOnFixedAmount, "AddOnFixedAmount");

const bm<SimmConfiguration::IMModel> imModelMap =
    list_of<bm<SimmConfiguration::IMModel>::value_type>(SimmConfiguration::IMModel::Schedule, "Schedule")(
        SimmConfiguration::IMModel::SIMM, "SIMM")(SimmConfiguration::IMModel::SIMM_P, "SIMM-P")(
        SimmConfiguration::IMModel::SIMM_R, "SIMM-R");

const bm<SimmConfiguration::Regulation> regulationsMap = list_of<bm<SimmConfiguration::Regulation>::value_type>(SimmConfiguration::Regulation::APRA, "APRA")
        (SimmConfiguration::Regulation::CFTC, "CFTC")(SimmConfiguration::Regulation::ESA, "ESA")(SimmConfiguration::Regulation::FINMA, "FINMA")
        (SimmConfiguration::Regulation::KFSC, "KFSC")(SimmConfiguration::Regulation::HKMA, "HKMA")(SimmConfiguration::Regulation::JFSA, "JFSA")
        (SimmConfiguration::Regulation::MAS, "MAS")(SimmConfiguration::Regulation::OSFI, "OSFI")(SimmConfiguration::Regulation::RBI, "RBI")
        (SimmConfiguration::Regulation::SEC, "SEC")(SimmConfiguration::Regulation::SEC_unseg, "SEC-unseg")(SimmConfiguration::Regulation::USPR, "USPR")
        (SimmConfiguration::Regulation::NONREG, "NONREG")(SimmConfiguration::Regulation::BACEN, "BACEN")(SimmConfiguration::Regulation::SANT, "SANT")
        (SimmConfiguration::Regulation::SFC, "SFC")(SimmConfiguration::Regulation::UK, "UK")(SimmConfiguration::Regulation::AMFQ, "AMFQ")
        (SimmConfiguration::Regulation::Included, "Included")(SimmConfiguration::Regulation::Unspecified, "Unspecified")(SimmConfiguration::Regulation::Invalid, "Invalid");

// Initialise the counts
const Size SimmConfiguration::numberOfRiskClasses = riskClassMap.size();
const Size SimmConfiguration::numberOfRiskTypes = riskTypeMap.size();
const Size SimmConfiguration::numberOfMarginTypes = marginTypeMap.size();
const Size SimmConfiguration::numberOfProductClasses = productClassMap.size();
const Size SimmConfiguration::numberOfRegulations = regulationsMap.size();

set<SimmConfiguration::RiskClass> SimmConfiguration::riskClasses(bool includeAll) {

    // This only works if 'All' is the last enum value
    Size bound = includeAll ? numberOfRiskClasses : numberOfRiskClasses - 1;

    // Return the set of values
    set<RiskClass> result;
    for (Size i = 0; i < bound; ++i) {
        result.insert(RiskClass(i));
    }
    return result;
}

set<SimmConfiguration::RiskType> SimmConfiguration::riskTypes(bool includeAll) {

    // This only works if 'All' is the last enum value
    Size bound = includeAll ? numberOfRiskTypes : numberOfRiskTypes - 1;

    // Return the set of values
    set<RiskType> result;
    for (Size i = 0; i < bound; ++i) {
        result.insert(RiskType(i));
    }
    return result;
}

set<SimmConfiguration::MarginType> SimmConfiguration::marginTypes(bool includeAll) {

    // This only works if 'All' is the last enum value
    Size bound = includeAll ? numberOfMarginTypes : numberOfMarginTypes - 1;

    // Return the set of values
    set<MarginType> result;
    for (Size i = 0; i < bound; ++i) {
        result.insert(MarginType(i));
    }
    return result;
}

set<SimmConfiguration::ProductClass> SimmConfiguration::productClasses(bool includeAll) {

    // This only works if 'All' is the last enum value
    Size bound = includeAll ? numberOfProductClasses : numberOfProductClasses - 1;

    // Return the set of values
    set<ProductClass> result;
    for (Size i = 0; i < bound; ++i) {
        result.insert(ProductClass(i));
    }
    return result;
}

ostream& operator<<(ostream& out, const SimmConfiguration::SimmSide& s) {
    string side = s == SimmConfiguration::SimmSide::Call ? "Call" : "Post";
    return out << side;
}

ostream& operator<<(ostream& out, const SimmConfiguration::RiskClass& rc) {
    QL_REQUIRE(riskClassMap.left.count(rc) > 0,
               "Risk class (" << static_cast<int>(rc) << ") not a valid SimmConfiguration::RiskClass");
    return out << riskClassMap.left.at(rc);
}

ostream& operator<<(ostream& out, const SimmConfiguration::RiskType& rt) {
    QL_REQUIRE(riskTypeMap.left.count(rt) > 0,
               "Risk type (" << static_cast<int>(rt) << ") not a valid SimmConfiguration::RiskType");
    return out << riskTypeMap.left.at(rt);
}

ostream& operator<<(ostream& out, const SimmConfiguration::MarginType& mt) {
    QL_REQUIRE(marginTypeMap.left.count(mt) > 0,
               "Margin type (" << static_cast<int>(mt) << ") not a valid SimmConfiguration::MarginType");
    return out << marginTypeMap.left.at(mt);
}

ostream& operator<<(ostream& out, const SimmConfiguration::ProductClass& pc) {
    QL_REQUIRE(productClassMap.left.count(pc) > 0,
               "Product class (" << static_cast<int>(pc) << ") not a valid SimmConfiguration::ProductClass");
    return out << productClassMap.left.at(pc);
}

ostream& operator<<(ostream& out, const SimmConfiguration::IMModel& model) {
    QL_REQUIRE(imModelMap.left.count(model) > 0, "Product class not a valid SimmConfiguration::IMModel");
    if (model == SimmConfiguration::IMModel::SIMM_P || model == SimmConfiguration::IMModel::SIMM_R)
        return out << "SIMM";
    else
        return out << imModelMap.left.at(model);
}

ostream& operator<<(ostream& out, const SimmConfiguration::Regulation& regulation) {
    QL_REQUIRE(regulationsMap.left.count(regulation) > 0, "Product class not a valid SimmConfiguration::Regulation");
    return out << regulationsMap.left.at(regulation);
}

SimmConfiguration::SimmSide parseSimmSide(const string& side) {
    if (side == "Call") {
        return SimmConfiguration::SimmSide::Call;
    } else if (side == "Post") {
        return SimmConfiguration::SimmSide::Post;
    } else {
        QL_FAIL("Could not parse the string '" << side << "' to a SimmSide");
    }
}

SimmConfiguration::RiskClass parseSimmRiskClass(const string& rc) {
    QL_REQUIRE(riskClassMap.right.count(rc) > 0,
               "Risk class string " << rc << " does not correspond to a valid SimmConfiguration::RiskClass");
    return riskClassMap.right.at(rc);
}

SimmConfiguration::RiskType parseSimmRiskType(const string& rt) {
    for (auto it = riskTypeMap.right.begin(); it != riskTypeMap.right.end(); it++) {
        if (boost::to_lower_copy(rt) == boost::to_lower_copy(it->first))
            return it->second;
    }

    // If we reach this point, then the risk type provided was not found
    QL_FAIL("Risk type string " << rt << " does not correspond to a valid SimmConfiguration::RiskType");
}

SimmConfiguration::MarginType parseSimmMarginType(const string& mt) {
    QL_REQUIRE(marginTypeMap.right.count(mt) > 0,
               "Margin type string " << mt << " does not correspond to a valid SimmConfiguration::MarginType");
    return marginTypeMap.right.at(mt);
}

SimmConfiguration::ProductClass parseSimmProductClass(const string& pc) {
    for (auto it = productClassMap.right.begin(); it != productClassMap.right.end(); it++) {
        if (boost::to_lower_copy(pc) == boost::to_lower_copy(it->first))
            return it->second;
    }

    // If we reach this point, then the product class provided was not found
    QL_FAIL("Product class string " << pc << " does not correspond to a valid SimmConfiguration::ProductClass");
}

SimmConfiguration::IMModel parseIMModel(const string& model) {
    for (auto it = imModelMap.right.begin(); it != imModelMap.right.end(); it++) {
        if (boost::to_lower_copy(model) == boost::to_lower_copy(it->first))
            return it->second;
    }

    // If we reach this point, then the IM modelprovided was not found
    QL_FAIL("IM model string " << model << " does not correspond to a valid SimmConfiguration::IMModel");
}

SimmConfiguration::Regulation parseRegulation(const string& regulation) {
    if (regulationsMap.right.count(regulation) == 0) {
        return SimmConfiguration::Regulation::Invalid;
    } else {
        return regulationsMap.right.at(regulation);
    }
}

string combineRegulations(const string& regs1, const string& regs2) {
    if (regs1.empty())
        return regs2;
    if (regs2.empty())
        return regs1;

    return regs1 + ',' + regs2;
}

set<string> parseRegulationString(const string& regsString, const set<string>& valueIfEmpty) {
    set<string> uniqueRegNames;

    // "," is a delimiter; "[" and "]" are possible characters, but are not needed in processing
    vector<string> regNames;
    boost::split(regNames, regsString, boost::is_any_of(",[] "));

    // Remove any resultant blank strings
    regNames.erase(std::remove(regNames.begin(), regNames.end(), ""), regNames.end());
    
    // One last trim for good measure
    for (string& r : regNames)
        boost::trim(r);

    // Sort the elements to prevent different permutations of the same regulations being treated differently
    // e.g. "APRA,USPR" and "USPR,APRA" should ultimately be treated as the same.
    std::sort(regNames.begin(), regNames.end());

    uniqueRegNames = set<string>(regNames.begin(), regNames.end());

    // If no (valid) regulations provided, we consider the regulation to be unspecified
    if (uniqueRegNames.empty())
        return valueIfEmpty;
    else
        return uniqueRegNames;
}

string sortRegulationString(const string& regsString) {
    // This method will sort the regulations
    set<string> uniqueRegNames = parseRegulationString(regsString);

    if (uniqueRegNames.size() == 0 ||
        (uniqueRegNames.size() == 1 && uniqueRegNames.count("Unspecified") > 0)) {
        return string();
    } else {
        return boost::algorithm::join(uniqueRegNames, ",");
    }
}

string removeRegulations(const string& regsString, const vector<string>& regsToRemove) {
    set<string> uniqueRegNames = parseRegulationString(regsString);

    for (const string& r : regsToRemove) {
        auto it = uniqueRegNames.find(r);
        if (it != uniqueRegNames.end())
            uniqueRegNames.erase(it);
    }

    if (!uniqueRegNames.empty())
        return boost::algorithm::join(uniqueRegNames, ",");
    else
        return string();
}

string filterRegulations(const string& regsString, const vector<string>& regsToFilter) {
    set<string> uniqueRegNames = parseRegulationString(regsString);
    set<string> filteredRegs;

    for (const string& r : uniqueRegNames) {
        auto it = std::find(regsToFilter.begin(), regsToFilter.end(), r);
        if (it != regsToFilter.end())
            filteredRegs.insert(*it);
    }

    if (!filteredRegs.empty())
        return boost::algorithm::join(filteredRegs, ",");
    else
        return string();
}

SimmConfiguration::Regulation getWinningRegulation(const std::vector<string>& winningRegulations) {

    vector<SimmConfiguration::Regulation> mappedRegulations;

    for (const string& reg : winningRegulations)
        mappedRegulations.push_back(parseRegulation(reg));

    SimmConfiguration::Regulation winningRegulation = mappedRegulations.front();
    for (const auto& reg : mappedRegulations) {
        if (reg < winningRegulation)
            winningRegulation = reg;
    }

    return winningRegulation;
}

//! Define ordering for ProductClass
bool SimmConfiguration::less_than(const SimmConfiguration::ProductClass& lhs,
                                  const SimmConfiguration::ProductClass& rhs) {
    QL_REQUIRE(lhs != SimmConfiguration::ProductClass::All && rhs != SimmConfiguration::ProductClass::All,
               "Cannot compare the \"All\" ProductClass.");
    QL_REQUIRE(static_cast<int>(SimmConfiguration::ProductClass::All) == 10,
               "Number of SIMM Product classes is not 11. Some order comparisons are not handled.");

    // all branches end in returns so no breaks are included.
    switch (lhs) {
    case SimmConfiguration::ProductClass::AddOnFixedAmount:
    case SimmConfiguration::ProductClass::AddOnNotionalFactor:
    case SimmConfiguration::ProductClass::Empty:
    case SimmConfiguration::ProductClass::Other:
        switch (rhs) {
        case SimmConfiguration::ProductClass::AddOnFixedAmount:
        case SimmConfiguration::ProductClass::AddOnNotionalFactor:
        case SimmConfiguration::ProductClass::Empty:
        case SimmConfiguration::ProductClass::Other:
            return false;
        default:
            return true;
        }
    case SimmConfiguration::ProductClass::RatesFX:
    case SimmConfiguration::ProductClass::Rates:
    case SimmConfiguration::ProductClass::FX:
        switch (rhs) {
        case SimmConfiguration::ProductClass::Empty:
        case SimmConfiguration::ProductClass::Other:
        case SimmConfiguration::ProductClass::RatesFX:
        case SimmConfiguration::ProductClass::Rates:
        case SimmConfiguration::ProductClass::FX:
            return false;
        default:
            return true;
        }
    case SimmConfiguration::ProductClass::Equity:
        switch (rhs) {
        case SimmConfiguration::ProductClass::Empty:
        case SimmConfiguration::ProductClass::Other:
        case SimmConfiguration::ProductClass::RatesFX:
        case SimmConfiguration::ProductClass::Rates:
        case SimmConfiguration::ProductClass::FX:
        case SimmConfiguration::ProductClass::Equity:
            return false;
        default:
            return true;
        }
    case SimmConfiguration::ProductClass::Commodity:
        if (rhs != SimmConfiguration::ProductClass::Credit) {
            return false;
        } else {
            return true;
        }
    case SimmConfiguration::ProductClass::Credit:
        return false;
    case SimmConfiguration::ProductClass::All:
        // not handled, fall through to failure
        break;
    }
    QL_FAIL("Unhandled SIMM Product class in waterfall logic.");
}

bool SimmConfiguration::greater_than(const SimmConfiguration::ProductClass& lhs,
                                     const SimmConfiguration::ProductClass& rhs) {
    return SimmConfiguration::less_than(rhs, lhs);
}
bool SimmConfiguration::less_than_or_equal_to(const SimmConfiguration::ProductClass& lhs,
                                              const SimmConfiguration::ProductClass& rhs) {
    return !SimmConfiguration::greater_than(lhs, rhs);
}
bool SimmConfiguration::greater_than_or_equal_to(const SimmConfiguration::ProductClass& lhs,
                                                 const SimmConfiguration::ProductClass& rhs) {
    return !SimmConfiguration::less_than(lhs, rhs);
}
} // namespace analytics
} // namespace ore
