/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/scenario/cvascenario.hpp>
#include <ored/utilities/parsers.hpp>
#include <vector>
#include <boost/algorithm/string/split.hpp>
#include <boost/assign.hpp>
#include <boost/bimap.hpp>

using namespace std;
using namespace ore::data;
using namespace QuantLib;
using boost::assign::list_of;

namespace ore {
namespace analytics {

struct string_cmp {
    bool operator()(const string& lhs, const string& rhs) const {
        return ((boost::to_lower_copy(lhs)) < (boost::to_lower_copy(rhs)));
    }
};

template <typename T> using bm = boost::bimap<T, boost::bimaps::set_of<std::string, string_cmp>>;

const bm<CvaRiskFactorKey::MarginType> marginTypeMap =
    list_of<bm<CvaRiskFactorKey::MarginType>::value_type>
        (CvaRiskFactorKey::MarginType::Delta, "Delta")
        (CvaRiskFactorKey::MarginType::Vega, "Vega");

const bm<CvaRiskFactorKey::KeyType> keyTypeMap =
    list_of<bm<CvaRiskFactorKey::KeyType>::value_type>
        (CvaRiskFactorKey::KeyType::InterestRate, "InterestRate")
        (CvaRiskFactorKey::KeyType::ForeignExchange, "ForeignExchange")
        (CvaRiskFactorKey::KeyType::CreditCounterparty, "CreditCounterparty")
        (CvaRiskFactorKey::KeyType::CreditReference, "CreditReference")
        (CvaRiskFactorKey::KeyType::Equity, "Equity")
        (CvaRiskFactorKey::KeyType::Commodity, "Commodity");

std::ostream& operator<<(std::ostream& out, const CvaRiskFactorKey::KeyType& type) {
    QL_REQUIRE(keyTypeMap.left.count(type) > 0, "Key type: " << type << " is not a valid CvaRiskFactorKey::KeyType");
    return out << keyTypeMap.left.at(type);
}

std::ostream& operator<<(std::ostream& out, const CvaRiskFactorKey::MarginType& type) {
    QL_REQUIRE(marginTypeMap.left.count(type) > 0, "Margin type: " << type << " not a valid CvaRiskFactorKey::MarginType");
    return out << marginTypeMap.left.at(type);
}

std::ostream& operator<<(std::ostream& out, const CvaRiskFactorKey& key) {
    // If empty key just return empty string (not "?//0")
    if (key == CvaRiskFactorKey()) {
        return out << "";
    }

    // If not empty key
    return out << key.keytype << "/" << key.margintype << "/" << key.name << "/" << key.period;
}

CvaRiskFactorKey::MarginType parseCvaRiskFactorMarginType(const string& str) {
    QL_REQUIRE(marginTypeMap.right.count(str) > 0,
        "Margin type string " << str << " does not correspond to a valid CvaRiskFactorKey::MarginType");
    return marginTypeMap.right.at(str);
}

CvaRiskFactorKey::KeyType parseCvaRiskFactorKeyType(const string& str) {
    QL_REQUIRE(keyTypeMap.right.count(str) > 0,
        "CVA Risk type string " << str << " does not correspond to a valid CvaRiskFactorKey::KeyType");
    return keyTypeMap.right.at(str);
}

CvaRiskFactorKey parseCvaRiskFactorKey(const string& str) {
    std::vector<string> tokens;
    boost::split(tokens, str, boost::is_any_of("/"), boost::token_compress_on);
    QL_REQUIRE(tokens.size() == 4, "Could not parse key " << str);
    CvaRiskFactorKey rfk(parseCvaRiskFactorKeyType(tokens[0]), parseCvaRiskFactorMarginType(tokens[1]), tokens[2], parsePeriod(tokens[3]));
    return rfk;
}

Real CvaShiftedScenario::get(std::string id) const {
    if (has(id))
        return CvaScenario::get(id);
    return baseScenario_->get(id);
}

}
}
