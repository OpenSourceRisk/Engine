/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>
#include <orea/simm/crifrecord.hpp>


namespace ore {
namespace analytics {

typedef CrifRecord::Regulation Regulation;
typedef CrifRecord::RiskType RiskType;

using std::ostream;
using std::string;
using std::vector;

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

// clang-format off
// Initialise the bimaps
const bm<RiskType> riskTypeMap = boost::assign::list_of<bm<RiskType>::value_type>(
    RiskType::Commodity, "Risk_Commodity")
    (RiskType::CommodityVol, "Risk_CommodityVol")
    (RiskType::CreditNonQ, "Risk_CreditNonQ")
    (RiskType::CreditQ, "Risk_CreditQ")
    (RiskType::CreditVol, "Risk_CreditVol")
    (RiskType::CreditVolNonQ, "Risk_CreditVolNonQ")
    (RiskType::Equity, "Risk_Equity")
    (RiskType::EquityVol, "Risk_EquityVol")
    (RiskType::FX, "Risk_FX")
    (RiskType::FXVol, "Risk_FXVol")
    (RiskType::Inflation, "Risk_Inflation")
    (RiskType::IRCurve, "Risk_IRCurve")
    (RiskType::IRVol, "Risk_IRVol")
    (RiskType::InflationVol, "Risk_InflationVol")
    (RiskType::BaseCorr, "Risk_BaseCorr")
    (RiskType::XCcyBasis, "Risk_XCcyBasis")
    (RiskType::ProductClassMultiplier, "Param_ProductClassMultiplier")
    (RiskType::AddOnNotionalFactor, "Param_AddOnNotionalFactor")
    (RiskType::Notional, "Notional")
    (RiskType::AddOnFixedAmount, "Param_AddOnFixedAmount")
    (RiskType::PV,"PV") // IM Schedule
    (RiskType::GIRR_DELTA, "GIRR_DELTA")
    (RiskType::GIRR_VEGA, "GIRR_VEGA")
    (RiskType::GIRR_CURV, "GIRR_CURV")
    (RiskType::CSR_NS_DELTA, "CSR_NS_DELTA")
    (RiskType::CSR_NS_VEGA, "CSR_NS_VEGA")
    (RiskType::CSR_NS_CURV, "CSR_NS_CURV")
    (RiskType::CSR_SNC_DELTA, "CSR_SNC_DELTA")
    (RiskType::CSR_SNC_VEGA, "CSR_SNC_VEGA")
    (RiskType::CSR_SNC_CURV, "CSR_SNC_CURV")
    (RiskType::CSR_SC_DELTA, "CSR_SC_DELTA")
    (RiskType::CSR_SC_VEGA, "CSR_SC_VEGA")
    (RiskType::CSR_SC_CURV, "CSR_SC_CURV")
    (RiskType::EQ_DELTA, "EQ_DELTA")
    (RiskType::EQ_VEGA, "EQ_VEGA")
    (RiskType::EQ_CURV, "EQ_CURV")
    (RiskType::COMM_DELTA, "COMM_DELTA")
    (RiskType::COMM_VEGA, "COMM_VEGA")
    (RiskType::COMM_CURV, "COMM_CURV")
    (RiskType::FX_DELTA, "FX_DELTA")
    (RiskType::FX_VEGA, "FX_VEGA")
    (RiskType::FX_CURV, "FX_CURV")
    (RiskType::DRC_NS, "DRC_NS")
    (RiskType::DRC_SNC, "DRC_SNC")
    (RiskType::DRC_SC, "DRC_SC")
    (RiskType::RRAO_1_PERCENT, "RRAO_1_PERCENT")
    (RiskType::RRAO_01_PERCENT, "RRAO_01_PERCENT")
    (RiskType::Empty, "")
    (RiskType::All, "All");

const bm<CrifRecord::ProductClass> productClassMap = boost::assign::list_of<bm<CrifRecord::ProductClass>::value_type>(
    CrifRecord::ProductClass::RatesFX, "RatesFX")(CrifRecord::ProductClass::Rates, "Rates")(
    CrifRecord::ProductClass::FX, "FX")(CrifRecord::ProductClass::Credit, "Credit")(
    CrifRecord::ProductClass::Equity, "Equity")(CrifRecord::ProductClass::Commodity, "Commodity")(
    CrifRecord::ProductClass::Other, "Other")(CrifRecord::ProductClass::Empty, "")(
    CrifRecord::ProductClass::All, "All")(CrifRecord::ProductClass::AddOnNotionalFactor, "AddOnNotionalFactor")(
    CrifRecord::ProductClass::AddOnFixedAmount, "AddOnFixedAmount");

const bm<CrifRecord::IMModel> imModelMap = boost::assign::list_of<bm<CrifRecord::IMModel>::value_type>(
    CrifRecord::IMModel::Schedule, "Schedule")(CrifRecord::IMModel::SIMM, "SIMM")(
    CrifRecord::IMModel::SIMM_R, "SIMM-R")(CrifRecord::IMModel::SIMM_P, "SIMM-P")(
    CrifRecord::IMModel::Empty, "");

const bm<Regulation> regulationsMap = boost::assign::list_of<bm<Regulation>::value_type>(
    Regulation::APRA, "APRA")(Regulation::CFTC, "CFTC")(
    Regulation::ESA, "ESA")(Regulation::FINMA, "FINMA")(
    Regulation::KFSC, "KFSC")(Regulation::HKMA, "HKMA")(
    Regulation::JFSA, "JFSA")(Regulation::MAS, "MAS")(
    Regulation::OSFI, "OSFI")(Regulation::RBI, "RBI")(
    Regulation::SEC, "SEC")(Regulation::SEC_unseg, "SEC-unseg")(
    Regulation::USPR, "USPR")(Regulation::NONREG, "NONREG")(
    Regulation::BACEN, "BACEN")(Regulation::SANT, "SANT")(
    Regulation::SFC, "SFC")(Regulation::UK, "UK")(
    Regulation::AMFQ, "AMFQ")(Regulation::BANX, "BANX")(
    Regulation::OJK, "OJK")(
    Regulation::Included, "Included")(Regulation::Unspecified, "Unspecified")(
    Regulation::Excluded, "Excluded")(Regulation::Invalid, "Invalid");
// clang-format on

ostream& operator<<(ostream& out, const RiskType& rt) {
    QL_REQUIRE(riskTypeMap.left.count(rt) > 0,
               "Risk type (" << static_cast<int>(rt) << ") not a valid CrifRecord::RiskType");
    return out << riskTypeMap.left.at(rt);
}

ostream& operator<<(ostream& out, const CrifRecord::ProductClass& pc) {
    QL_REQUIRE(productClassMap.left.count(pc) > 0,
               "Product class (" << static_cast<int>(pc) << ") not a valid CrifRecord::ProductClass");
    return out << productClassMap.left.at(pc);
}

ostream& operator<<(ostream& out, const CrifRecord::IMModel& model) {
    QL_REQUIRE(imModelMap.left.count(model) > 0, "Product class not a valid CrifRecord::IMModel");
    if (model == CrifRecord::IMModel::SIMM_P || model == CrifRecord::IMModel::SIMM_R ||
        model == CrifRecord::IMModel::SIMM)
        return out << "SIMM";
    else
        return out << imModelMap.left.at(model);
}

ostream& operator<<(ostream& out, const Regulation& regulation) {
    QL_REQUIRE(regulationsMap.left.count(regulation) > 0, "Product class not a valid Regulation");
    return out << regulationsMap.left.at(regulation);
}

CrifRecord::IMModel parseIMModel(const string& model) {
    for (auto it = imModelMap.right.begin(); it != imModelMap.right.end(); it++) {
        if (boost::to_lower_copy(model) == boost::to_lower_copy(it->first))
            return it->second;
    }

    // If we reach this point, then the IM modelprovided was not found
    QL_FAIL("IM model string " << model << " does not correspond to a valid CrifRecord::IMModel");
}

Regulation parseRegulation(const string& regulation) {
    if (regulationsMap.right.count(regulation) == 0) {
        return Regulation::Invalid;
    } else {
        return regulationsMap.right.at(regulation);
    }
}

set<Regulation> parseRegulationString(const string& regsString,
                                                  const set<Regulation>& valueIfEmpty) {
    set<Regulation> regs;

    // "," is a delimiter; "[" and "]" are possible characters, but are not needed in processing
    set<string> regNamesStr;
    boost::split(regNamesStr, regsString, boost::is_any_of(",[] "));

    // Remove any resultant blank strings
    for (auto it = regNamesStr.begin(); it != regNamesStr.end();) {
        if (it->empty())
            it = regNamesStr.erase(it);
        else
            it++;
    }

    // Trim whitepsaces then parse into Regulation
    std::transform(regNamesStr.begin(), regNamesStr.end(), std::inserter(regs, regs.end()),
                   [](const string& reg) { return parseRegulation(boost::trim_copy(reg)); });

    // If no (valid) regulations provided, we fall back to the default regulation
    if (regs.empty())
        return valueIfEmpty;
    else
        return regs;
}

set<Regulation> removeRegulations(const set<Regulation>& regs, const set<Regulation>& regsToRemove) {

    set<Regulation> newRegs;

    std::copy_if(regs.begin(), regs.end(), std::inserter(newRegs, newRegs.end()),
                 [&regsToRemove](const Regulation& reg) { return regsToRemove.find(reg) == regsToRemove.end(); });

    return newRegs;
}

set<Regulation> filterRegulations(const set<Regulation>& regs, const set<Regulation>& regsToFilter) {
    set<Regulation> newRegs;

    std::copy_if(regs.begin(), regs.end(), std::inserter(newRegs, newRegs.end()),
                 [&regsToFilter](const Regulation& reg) { return regsToFilter.find(reg) != regsToFilter.end(); });

    return newRegs;
}

string regulationsToString(const set<Regulation>& regs) {
    set<string> regsStrSet;

    // Transforming into a set first lets us maintain lexicographical ordering in our regulation lists,
    // instead of the default ordering defined by the Regulation enum
    std::transform(regs.begin(), regs.end(), std::inserter(regsStrSet, regsStrSet.end()),
                   [](const Regulation& reg) { return ore::data::to_string(reg); });

    return boost::algorithm::join(regsStrSet, ",");
}

Regulation getWinningRegulation(const set<Regulation>& winningRegulations) {
    QL_REQUIRE(!winningRegulations.empty(), "getWinningRegulation(): Input set is empty.");
    return *winningRegulations.begin();
}

RiskType parseRiskType(const string& rt) {
    for (auto it = riskTypeMap.right.begin(); it != riskTypeMap.right.end(); it++) {
        if (boost::to_lower_copy(rt) == boost::to_lower_copy(it->first))
            return it->second;
    }

    // If we reach this point, then the risk type provided was not found
    QL_FAIL("Risk type string " << rt << " does not correspond to a valid CrifRecord::RiskType");
}

CrifRecord::ProductClass parseProductClass(const string& pc) {
    for (auto it = productClassMap.right.begin(); it != productClassMap.right.end(); it++) {
        if (boost::to_lower_copy(pc) == boost::to_lower_copy(it->first))
            return it->second;
    }

    // If we reach this point, then the product class provided was not found
    QL_FAIL("Product class string " << pc << " does not correspond to a valid CrifRecord::ProductClass");
}

std::ostream& operator<<(std::ostream& out, const CrifRecord::CurvatureScenario& scenario){
    switch(scenario){
        case CrifRecord::CurvatureScenario::Down:
            out << "CurvatureDown";
            break;
        case CrifRecord::CurvatureScenario::Up:
            out << "CurvatureUp";
            break;
        default:
            out << "";
            break;
    }
    return out;
}

CrifRecord::CurvatureScenario parseFrtbCurvatureScenario(const std::string& scenario) {
    if (scenario == "CurvatureDown") {
        return CrifRecord::CurvatureScenario::Down;
    } else if (scenario == "CurvatureUp") {
        return CrifRecord::CurvatureScenario::Up;
    } else {
        return CrifRecord::CurvatureScenario::Empty;
    }
}

vector<std::set<std::string>> CrifRecord::additionalHeaders = {};

ostream& operator<<(ostream& out, const CrifRecord& cr) {
    const NettingSetDetails& n = cr.nettingSetDetails;
    if (n.empty()) {
        out << "[" << cr.tradeId << ", " << cr.nettingSetDetails.nettingSetId() << ", " << cr.productClass << ", " << cr.riskType
            << ", " << cr.qualifier << ", " << cr.bucket << ", " << cr.label1 << ", " << cr.label2 << ", "
            << cr.amountCurrency << ", " << cr.amount << ", " << cr.amountUsd;
    } else {
        out << "[" << cr.tradeId << ", [" << n << "], " << cr.productClass << ", " << cr.riskType << ", "
            << cr.qualifier << ", " << cr.bucket << ", " << cr.label1 << ", " << cr.label2 << ", "
            << cr.amountCurrency << ", " << cr.amount << ", " << cr.amountUsd;
    }

    if (!cr.collectRegulations.empty())
        out << ", collect_regulations=" << cr.collectRegulations;
    if (!cr.postRegulations.empty())
        out << ", post_regulations=" << cr.postRegulations;

    out << "]";

    return out;
}

ostream& operator<<(ostream& out, const set<Regulation>& regs) { return out << regulationsToString(regs); }

CrifRecord::RecordType CrifRecord::type() const {
    switch (riskType) {
    case RiskType::Commodity:
    case RiskType::CommodityVol:
    case RiskType::CreditNonQ:
    case RiskType::CreditQ:
    case RiskType::CreditVol:
    case RiskType::CreditVolNonQ:
    case RiskType::Equity:
    case RiskType::EquityVol:
    case RiskType::FX:
    case RiskType::FXVol:
    case RiskType::Inflation:
    case RiskType::IRCurve:
    case RiskType::IRVol:
    case RiskType::InflationVol:
    case RiskType::BaseCorr:
    case RiskType::XCcyBasis:
    case RiskType::ProductClassMultiplier:
    case RiskType::AddOnNotionalFactor:
    case RiskType::Notional:
    case RiskType::AddOnFixedAmount:
    case RiskType::PV:
        return RecordType::SIMM;
    case RiskType::GIRR_DELTA:
    case RiskType::GIRR_VEGA:
    case RiskType::GIRR_CURV:
    case RiskType::CSR_NS_DELTA:
    case RiskType::CSR_NS_VEGA:
    case RiskType::CSR_NS_CURV:
    case RiskType::CSR_SNC_DELTA:
    case RiskType::CSR_SNC_VEGA:
    case RiskType::CSR_SNC_CURV:
    case RiskType::CSR_SC_DELTA:
    case RiskType::CSR_SC_VEGA:
    case RiskType::CSR_SC_CURV:
    case RiskType::EQ_DELTA:
    case RiskType::EQ_VEGA:
    case RiskType::EQ_CURV:
    case RiskType::COMM_DELTA:
    case RiskType::COMM_VEGA:
    case RiskType::COMM_CURV:
    case RiskType::FX_DELTA:
    case RiskType::FX_VEGA:
    case RiskType::FX_CURV:
    case RiskType::DRC_NS:
    case RiskType::DRC_SNC:
    case RiskType::DRC_SC:
    case RiskType::RRAO_1_PERCENT:
    case RiskType::RRAO_01_PERCENT:
        return RecordType::FRTB;
    case RiskType::All:
    case RiskType::Empty:
        return RecordType::Generic;
    default:
        QL_FAIL("CrifRecord::Type(): Unexpected CrifRecord::RiskType " << riskType);
    }
}

vector<Regulation> standardRegulations() {
    return vector<Regulation>({Regulation::APRA,       Regulation::CFTC,   Regulation::ESA,   Regulation::FINMA,
                               Regulation::KFSC,       Regulation::HKMA,   Regulation::JFSA,  Regulation::MAS,
                               Regulation::OSFI,       Regulation::RBI,    Regulation::SEC,   Regulation::SEC_unseg,
                               Regulation::USPR,       Regulation::NONREG, Regulation::BACEN, Regulation::SANT,
                               Regulation::SFC,        Regulation::UK,     Regulation::AMFQ});
}

} // namespace analytics
} // namespace ore
