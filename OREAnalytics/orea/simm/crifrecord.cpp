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
    CrifRecord::RiskType::Commodity, "Risk_Commodity")(CrifRecord::RiskType::CommodityVol, "Risk_CommodityVol")(
    CrifRecord::RiskType::CreditNonQ, "Risk_CreditNonQ")(CrifRecord::RiskType::CreditQ, "Risk_CreditQ")(
    CrifRecord::RiskType::CreditVol, "Risk_CreditVol")(CrifRecord::RiskType::CreditVolNonQ, "Risk_CreditVolNonQ")(
    CrifRecord::RiskType::Equity, "Risk_Equity")(CrifRecord::RiskType::EquityVol, "Risk_EquityVol")(
    CrifRecord::RiskType::FX, "Risk_FX")(CrifRecord::RiskType::FXVol, "Risk_FXVol")(
    CrifRecord::RiskType::Inflation, "Risk_Inflation")(CrifRecord::RiskType::IRCurve, "Risk_IRCurve")(
    CrifRecord::RiskType::IRVol, "Risk_IRVol")(CrifRecord::RiskType::InflationVol, "Risk_InflationVol")(
    CrifRecord::RiskType::BaseCorr, "Risk_BaseCorr")(CrifRecord::RiskType::XCcyBasis, "Risk_XCcyBasis")(
    CrifRecord::RiskType::ProductClassMultiplier, "Param_ProductClassMultiplier")(
    CrifRecord::RiskType::AddOnNotionalFactor, "Param_AddOnNotionalFactor")(CrifRecord::RiskType::Notional, "Notional")(
    CrifRecord::RiskType::AddOnFixedAmount, "Param_AddOnFixedAmount")(CrifRecord::RiskType::PV,
                                                                      "PV") // IM Schedule
    (CrifRecord::RiskType::All, "All");

const bm<CrifRecord::ProductClass> productClassMap = boost::assign::list_of<bm<CrifRecord::ProductClass>::value_type>(
    CrifRecord::ProductClass::RatesFX, "RatesFX")(CrifRecord::ProductClass::Rates, "Rates")(
    CrifRecord::ProductClass::FX, "FX")(CrifRecord::ProductClass::Credit, "Credit")(
    CrifRecord::ProductClass::Equity, "Equity")(CrifRecord::ProductClass::Commodity, "Commodity")(
    CrifRecord::ProductClass::Other, "Other")(CrifRecord::ProductClass::Empty, "")(
    CrifRecord::ProductClass::All, "All")(CrifRecord::ProductClass::AddOnNotionalFactor, "AddOnNotionalFactor")(
    CrifRecord::ProductClass::AddOnFixedAmount, "AddOnFixedAmount");


CrifRecord::ProductClass parseSimmProductClass(const string& pc) {
    for (auto it = productClassMap.right.begin(); it != productClassMap.right.end(); it++) {
        if (boost::to_lower_copy(pc) == boost::to_lower_copy(it->first))
            return it->second;
    }

    // If we reach this point, then the product class provided was not found
    QL_FAIL("Product class string " << pc << " does not correspond to a valid CrifRecord::ProductClass");
}


std::map<QuantLib::Size, std::set<std::string>> CrifRecord::additionalHeaders = {};
    
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

} // namespace analytics
} // namespace ore
