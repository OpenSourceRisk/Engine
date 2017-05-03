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

RiskFactorKey::KeyType parseRiskFactorKeyType(const string& str) {
    if (str == "DiscountCurve")
        return RiskFactorKey::KeyType::DiscountCurve;
    else if (str == "YieldCurve")
        return RiskFactorKey::KeyType::YieldCurve;
    else if (str == "IndexCurve")
        return RiskFactorKey::KeyType::IndexCurve;
    else if (str == "SwaptionVolatility")
        return RiskFactorKey::KeyType::SwaptionVolatility;
    else if (str == "FXSpot")
        return RiskFactorKey::KeyType::FXSpot;
    else if (str == "FXVolatility")
        return RiskFactorKey::KeyType::FXVolatility;
    else if (str == "EQSpot")
        return RiskFactorKey::KeyType::EQSpot;
    else if (str == "EQVolatility")
        return RiskFactorKey::KeyType::EQVolatility;
    QL_FAIL("RiskFactorKey " << str << " does not exist.");
}

RiskFactorKey parseRiskFactorKey(const string& str) {
    std::vector<string> tokens;
    boost::split(tokens, str, boost::is_any_of("/"), boost::token_compress_on);
    QL_REQUIRE(tokens.size() == 3, "Could not parse key " << str);
    RiskFactorKey rfk(parseRiskFactorKeyType(tokens[0]), tokens[1], parseInteger(tokens[2]));
    return rfk;
}
}
}
