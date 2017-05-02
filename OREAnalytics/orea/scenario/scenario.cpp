/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <orea/scenario/scenario.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>
#include <vector>
#include <boost/algorithm/string/split.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

std::ostream& operator<<(std::ostream& out, const RiskFactorKey::KeyType& type) {
    switch (type) {
    case RiskFactorKey::KeyType::DiscountCurve:
        return out << "DiscountCurve";
    case RiskFactorKey::KeyType::YieldCurve:
        return out << "YieldCurve";
    case RiskFactorKey::KeyType::IndexCurve:
        return out << "IndexCurve";
    case RiskFactorKey::KeyType::SwaptionVolatility:
        return out << "SwaptionVolatility";
    case RiskFactorKey::KeyType::OptionletVolatility:
        return out << "OptionletVolatility";
    case RiskFactorKey::KeyType::FXSpot:
        return out << "FXSpot";
    case RiskFactorKey::KeyType::FXVolatility:
        return out << "FXVolatility";
    case RiskFactorKey::KeyType::EQSpot:
        return out << "EQSpot";
    case RiskFactorKey::KeyType::EQVolatility:
        return out << "EQVolatility";
    default:
        return out << "?";
    }
}

std::ostream& operator<<(std::ostream& out, const RiskFactorKey& key) {
    return out << key.keytype << "/" << key.name << "/" << key.index;
}

RiskFactorKey::KeyType parseRiskFactorKeyType(const string &string_type) {
    if (string_type == "DiscountCurve")
        return RiskFactorKey::KeyType::DiscountCurve;
    else if (string_type == "YieldCurve")
        return RiskFactorKey::KeyType::YieldCurve;
    else if (string_type == "IndexCurve")
        return RiskFactorKey::KeyType::IndexCurve;
    else if (string_type == "SwaptionVolatility")
        return RiskFactorKey::KeyType::SwaptionVolatility;
    else if (string_type == "FXSpot")
        return RiskFactorKey::KeyType::FXSpot;
    else if (string_type == "FXVolatility")
        return RiskFactorKey::KeyType::FXVolatility;
    QL_FAIL("RiskFactorKey " << string_type << " does not exist.");
}

RiskFactorKey parseRiskFactorKey(const string &string_key) {
    std::vector<string> tokens;
    boost::split(tokens, string_key, boost::is_any_of("/"), boost::token_compress_on);
    QL_REQUIRE(tokens.size() == 3, "Could not parse key " << string_key);
    RiskFactorKey rfk(parseRiskFactorKeyType(tokens[0]), tokens[1], parseInteger(tokens[2]));
    return rfk;
}

}
}
