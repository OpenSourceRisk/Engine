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
using std::pair;
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
template <typename T> using bm = boost::bimap<T, boost::bimaps::set_of<string, string_cmp>>;

// Initialise the bimaps
const bm<SimmConfiguration::RiskClass> riskClassMap =
    list_of<bm<SimmConfiguration::RiskClass>::value_type>(SimmConfiguration::RiskClass::InterestRate, "InterestRate")(
        SimmConfiguration::RiskClass::CreditQualifying,
        "CreditQualifying")(SimmConfiguration::RiskClass::CreditNonQualifying, "CreditNonQualifying")(
        SimmConfiguration::RiskClass::Equity, "Equity")(SimmConfiguration::RiskClass::Commodity, "Commodity")(
        SimmConfiguration::RiskClass::FX, "FX")(SimmConfiguration::RiskClass::All, "All");

const bm<SimmConfiguration::MarginType> marginTypeMap =
    list_of<bm<SimmConfiguration::MarginType>::value_type>(SimmConfiguration::MarginType::Delta, "Delta")(
        SimmConfiguration::MarginType::Vega, "Vega")(SimmConfiguration::MarginType::Curvature, "Curvature")(
        SimmConfiguration::MarginType::BaseCorr, "BaseCorr")(SimmConfiguration::MarginType::AdditionalIM, "AdditionalIM")(
        SimmConfiguration::MarginType::All, "All");

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
const Size SimmConfiguration::numberOfMarginTypes = marginTypeMap.size();
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

set<CrifRecord::RiskType> SimmConfiguration::riskTypes(bool includeAll) {

    static std::set<CrifRecord::RiskType> simmRiskTypes = {
        // SIMM Risk Types
        CrifRecord::RiskType::Commodity,
        CrifRecord::RiskType::CommodityVol,
        CrifRecord::RiskType::CreditNonQ,
        CrifRecord::RiskType::CreditQ,
        CrifRecord::RiskType::CreditVol,
        CrifRecord::RiskType::CreditVolNonQ,
        CrifRecord::RiskType::Equity,
        CrifRecord::RiskType::EquityVol,
        CrifRecord::RiskType::FX,
        CrifRecord::RiskType::FXVol,
        CrifRecord::RiskType::Inflation,
        CrifRecord::RiskType::IRCurve,
        CrifRecord::RiskType::IRVol,
        CrifRecord::RiskType::InflationVol,
        CrifRecord::RiskType::BaseCorr,
        CrifRecord::RiskType::XCcyBasis,
        CrifRecord::RiskType::ProductClassMultiplier,
        CrifRecord::RiskType::AddOnNotionalFactor,
        CrifRecord::RiskType::Notional,
        CrifRecord::RiskType::AddOnFixedAmount,
        CrifRecord::RiskType::PV, // IM Schedule
    };

    // This only works if 'All' is the last enum value
    if (includeAll) {
        simmRiskTypes.insert(CrifRecord::RiskType::All);
    }
    return simmRiskTypes;
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

set<CrifRecord::ProductClass> SimmConfiguration::productClasses(bool includeAll) {

    
    static std::set<CrifRecord::ProductClass> simmProductClasses = {
        CrifRecord::ProductClass::RatesFX,
        CrifRecord::ProductClass::Rates, // extension for IM Schedule
        CrifRecord::ProductClass::FX,    // extension for IM Schedule
        CrifRecord::ProductClass::Credit,
        CrifRecord::ProductClass::Equity,
        CrifRecord::ProductClass::Commodity,
        CrifRecord::ProductClass::Empty,
        CrifRecord::ProductClass::Other,               // extension for IM Schedule
        CrifRecord::ProductClass::AddOnNotionalFactor, // extension for additional IM
        CrifRecord::ProductClass::AddOnFixedAmount};
    
    if (includeAll) {
        simmProductClasses.insert(CrifRecord::ProductClass::All);
    }
    return simmProductClasses;
}

pair<CrifRecord::RiskType, CrifRecord::RiskType>
SimmConfiguration::riskClassToRiskType(const RiskClass& rc) {
    CrifRecord::RiskType deltaRiskType, vegaRiskType;
    switch (rc) {
    case RiskClass::InterestRate:
        deltaRiskType = CrifRecord::RiskType::IRCurve;
        vegaRiskType = CrifRecord::RiskType::IRVol;
        break;
    case RiskClass::CreditQualifying:
        deltaRiskType = CrifRecord::RiskType::CreditQ;
        vegaRiskType = CrifRecord::RiskType::CreditVol;
        break;
    case RiskClass::CreditNonQualifying:
        deltaRiskType = CrifRecord::RiskType::CreditNonQ;
        vegaRiskType = CrifRecord::RiskType::CreditVolNonQ;
        break;
    case RiskClass::Equity:
        deltaRiskType = CrifRecord::RiskType::Equity;
        vegaRiskType = CrifRecord::RiskType::EquityVol;
        break;
    case RiskClass::Commodity:
        deltaRiskType = CrifRecord::RiskType::Commodity;
        vegaRiskType = CrifRecord::RiskType::CommodityVol;
        break;
    case RiskClass::FX:
        deltaRiskType = CrifRecord::RiskType::FX;
        vegaRiskType = CrifRecord::RiskType::FXVol;
        break;
    default:
        QL_FAIL("riskClassToRiskType: Unexpected risk class");
    }

    return std::make_pair(deltaRiskType, vegaRiskType);
}

SimmConfiguration::RiskClass SimmConfiguration::riskTypeToRiskClass(const CrifRecord::RiskType& rt) {
    switch (rt) {
    case CrifRecord::RiskType::Commodity:
    case CrifRecord::RiskType::CommodityVol:
        return SimmConfiguration::RiskClass::Commodity;
    case CrifRecord::RiskType::CreditQ:
    case CrifRecord::RiskType::CreditVol:
    case CrifRecord::RiskType::BaseCorr:
        return SimmConfiguration::RiskClass::CreditQualifying;
    case CrifRecord::RiskType::CreditNonQ:
    case CrifRecord::RiskType::CreditVolNonQ:
        return SimmConfiguration::RiskClass::CreditNonQualifying;
    case CrifRecord::RiskType::Equity:
    case CrifRecord::RiskType::EquityVol:
        return SimmConfiguration::RiskClass::Equity;
    case CrifRecord::RiskType::FX:
    case CrifRecord::RiskType::FXVol:
        return SimmConfiguration::RiskClass::FX;
    case CrifRecord::RiskType::Inflation:
    case CrifRecord::RiskType::InflationVol:
    case CrifRecord::RiskType::IRCurve:
    case CrifRecord::RiskType::IRVol:
    case CrifRecord::RiskType::XCcyBasis:
        return SimmConfiguration::RiskClass::InterestRate;
    default:
        QL_FAIL("riskTypeToRiskClass: Invalid risk type");
    }
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

ostream& operator<<(ostream& out, const SimmConfiguration::MarginType& mt) {
    QL_REQUIRE(marginTypeMap.left.count(mt) > 0,
               "Margin type (" << static_cast<int>(mt) << ") not a valid SimmConfiguration::MarginType");
    return out << marginTypeMap.left.at(mt);
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

SimmConfiguration::MarginType parseSimmMarginType(const string& mt) {
    QL_REQUIRE(marginTypeMap.right.count(mt) > 0,
               "Margin type string " << mt << " does not correspond to a valid SimmConfiguration::MarginType");
    return marginTypeMap.right.at(mt);
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
bool SimmConfiguration::less_than(const CrifRecord::ProductClass& lhs,
                                  const CrifRecord::ProductClass& rhs) {
    QL_REQUIRE(lhs != CrifRecord::ProductClass::All && rhs != CrifRecord::ProductClass::All,
               "Cannot compare the \"All\" ProductClass.");
    QL_REQUIRE(static_cast<int>(CrifRecord::ProductClass::All) == 10,
               "Number of SIMM Product classes is not 11. Some order comparisons are not handled.");

    // all branches end in returns so no breaks are included.
    switch (lhs) {
    case CrifRecord::ProductClass::AddOnFixedAmount:
    case CrifRecord::ProductClass::AddOnNotionalFactor:
    case CrifRecord::ProductClass::Empty:
    case CrifRecord::ProductClass::Other:
        switch (rhs) {
        case CrifRecord::ProductClass::AddOnFixedAmount:
        case CrifRecord::ProductClass::AddOnNotionalFactor:
        case CrifRecord::ProductClass::Empty:
        case CrifRecord::ProductClass::Other:
            return false;
        default:
            return true;
        }
    case CrifRecord::ProductClass::RatesFX:
    case CrifRecord::ProductClass::Rates:
    case CrifRecord::ProductClass::FX:
        switch (rhs) {
        case CrifRecord::ProductClass::Empty:
        case CrifRecord::ProductClass::Other:
        case CrifRecord::ProductClass::RatesFX:
        case CrifRecord::ProductClass::Rates:
        case CrifRecord::ProductClass::FX:
            return false;
        default:
            return true;
        }
    case CrifRecord::ProductClass::Equity:
        switch (rhs) {
        case CrifRecord::ProductClass::Empty:
        case CrifRecord::ProductClass::Other:
        case CrifRecord::ProductClass::RatesFX:
        case CrifRecord::ProductClass::Rates:
        case CrifRecord::ProductClass::FX:
        case CrifRecord::ProductClass::Equity:
            return false;
        default:
            return true;
        }
    case CrifRecord::ProductClass::Commodity:
        if (rhs != CrifRecord::ProductClass::Credit) {
            return false;
        } else {
            return true;
        }
    case CrifRecord::ProductClass::Credit:
        return false;
    case CrifRecord::ProductClass::All:
        // not handled, fall through to failure
        break;
    }
    QL_FAIL("Unhandled SIMM Product class in waterfall logic.");
}

bool SimmConfiguration::greater_than(const CrifRecord::ProductClass& lhs,
                                     const CrifRecord::ProductClass& rhs) {
    return SimmConfiguration::less_than(rhs, lhs);
}
bool SimmConfiguration::less_than_or_equal_to(const CrifRecord::ProductClass& lhs,
                                              const CrifRecord::ProductClass& rhs) {
    return !SimmConfiguration::greater_than(lhs, rhs);
}
bool SimmConfiguration::greater_than_or_equal_to(const CrifRecord::ProductClass& lhs,
                                                 const CrifRecord::ProductClass& rhs) {
    return !SimmConfiguration::less_than(lhs, rhs);
}
} // namespace analytics
} // namespace ore
