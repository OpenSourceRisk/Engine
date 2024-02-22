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

using std::ostream;
using std::string;

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
const bm<CrifRecord::RiskType> riskTypeMap = boost::assign::list_of<bm<CrifRecord::RiskType>::value_type>(
    CrifRecord::RiskType::Commodity, "Risk_Commodity")
    (CrifRecord::RiskType::CommodityVol, "Risk_CommodityVol")
    (CrifRecord::RiskType::CreditNonQ, "Risk_CreditNonQ")
    (CrifRecord::RiskType::CreditQ, "Risk_CreditQ")
    (CrifRecord::RiskType::CreditVol, "Risk_CreditVol")
    (CrifRecord::RiskType::CreditVolNonQ, "Risk_CreditVolNonQ")
    (CrifRecord::RiskType::Equity, "Risk_Equity")
    (CrifRecord::RiskType::EquityVol, "Risk_EquityVol")
    (CrifRecord::RiskType::FX, "Risk_FX")
    (CrifRecord::RiskType::FXVol, "Risk_FXVol")
    (CrifRecord::RiskType::Inflation, "Risk_Inflation")
    (CrifRecord::RiskType::IRCurve, "Risk_IRCurve")
    (CrifRecord::RiskType::IRVol, "Risk_IRVol")
    (CrifRecord::RiskType::InflationVol, "Risk_InflationVol")
    (CrifRecord::RiskType::BaseCorr, "Risk_BaseCorr")
    (CrifRecord::RiskType::XCcyBasis, "Risk_XCcyBasis")
    (CrifRecord::RiskType::ProductClassMultiplier, "Param_ProductClassMultiplier")
    (CrifRecord::RiskType::AddOnNotionalFactor, "Param_AddOnNotionalFactor")
    (CrifRecord::RiskType::Notional, "Notional")
    (CrifRecord::RiskType::AddOnFixedAmount, "Param_AddOnFixedAmount")
    (CrifRecord::RiskType::PV,"PV") // IM Schedule
    (CrifRecord::RiskType::GIRR_DELTA, "GIRR_DELTA")
    (CrifRecord::RiskType::GIRR_VEGA, "GIRR_VEGA")
    (CrifRecord::RiskType::GIRR_CURV, "GIRR_CURV")
    (CrifRecord::RiskType::CSR_NS_DELTA, "CSR_NS_DELTA")
    (CrifRecord::RiskType::CSR_NS_VEGA, "CSR_NS_VEGA")
    (CrifRecord::RiskType::CSR_NS_CURV, "CSR_NS_CURV")
    (CrifRecord::RiskType::CSR_SNC_DELTA, "CSR_SNC_DELTA")
    (CrifRecord::RiskType::CSR_SNC_VEGA, "CSR_SNC_VEGA")
    (CrifRecord::RiskType::CSR_SNC_CURV, "CSR_SNC_CURV")
    (CrifRecord::RiskType::CSR_SC_DELTA, "CSR_SC_DELTA")
    (CrifRecord::RiskType::CSR_SC_VEGA, "CSR_SC_VEGA")
    (CrifRecord::RiskType::CSR_SC_CURV, "CSR_SC_CURV")
    (CrifRecord::RiskType::EQ_DELTA, "EQ_DELTA")
    (CrifRecord::RiskType::EQ_VEGA, "EQ_VEGA")
    (CrifRecord::RiskType::EQ_CURV, "EQ_CURV")
    (CrifRecord::RiskType::COMM_DELTA, "COMM_DELTA")
    (CrifRecord::RiskType::COMM_VEGA, "COMM_VEGA")
    (CrifRecord::RiskType::COMM_CURV, "COMM_CURV")
    (CrifRecord::RiskType::FX_DELTA, "FX_DELTA")
    (CrifRecord::RiskType::FX_VEGA, "FX_VEGA")
    (CrifRecord::RiskType::FX_CURV, "FX_CURV")
    (CrifRecord::RiskType::DRC_NS, "DRC_NS")
    (CrifRecord::RiskType::DRC_SNC, "DRC_SNC")
    (CrifRecord::RiskType::DRC_SC, "DRC_SC")
    (CrifRecord::RiskType::RRAO_1_PERCENT, "RRAO_1_PERCENT")
    (CrifRecord::RiskType::RRAO_01_PERCENT, "RRAO_01_PERCENT")
    (CrifRecord::RiskType::Empty, "")
    (CrifRecord::RiskType::All, "All");

const bm<CrifRecord::ProductClass> productClassMap = boost::assign::list_of<bm<CrifRecord::ProductClass>::value_type>(
    CrifRecord::ProductClass::RatesFX, "RatesFX")(CrifRecord::ProductClass::Rates, "Rates")(
    CrifRecord::ProductClass::FX, "FX")(CrifRecord::ProductClass::Credit, "Credit")(
    CrifRecord::ProductClass::Equity, "Equity")(CrifRecord::ProductClass::Commodity, "Commodity")(
    CrifRecord::ProductClass::Other, "Other")(CrifRecord::ProductClass::Empty, "")(
    CrifRecord::ProductClass::All, "All")(CrifRecord::ProductClass::AddOnNotionalFactor, "AddOnNotionalFactor")(
    CrifRecord::ProductClass::AddOnFixedAmount, "AddOnFixedAmount");


ostream& operator<<(ostream& out, const CrifRecord::RiskType& rt) {
    QL_REQUIRE(riskTypeMap.left.count(rt) > 0,
               "Risk type (" << static_cast<int>(rt) << ") not a valid CrifRecord::RiskType");
    return out << riskTypeMap.left.at(rt);
}

ostream& operator<<(ostream& out, const CrifRecord::ProductClass& pc) {
    QL_REQUIRE(productClassMap.left.count(pc) > 0,
               "Product class (" << static_cast<int>(pc) << ") not a valid CrifRecord::ProductClass");
    return out << productClassMap.left.at(pc);
}


CrifRecord::RiskType parseRiskType(const string& rt) {
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

std::vector<std::set<std::string>> CrifRecord::additionalHeaders = {};
    
ostream& operator<<(ostream& out, const CrifRecord& cr) {
    const NettingSetDetails& n = cr.nettingSetDetails;
    if (n.empty()) {
        out << "[" << cr.tradeId << ", " << cr.portfolioId << ", " << cr.productClass << ", " << cr.riskType
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
        QL_FAIL("Unexpected RiskType " << riskType);
    }
}

} // namespace analytics
} // namespace ore
