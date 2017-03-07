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
#include <boost/algorithm/string/split.hpp>

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
    default:
        return out << "?";
    }
}

std::ostream& operator<<(std::ostream& out, const RiskFactorKey& key) {
    return out << key.keytype << "/" << key.name << "/" << key.index;
}

std::string toString(const RiskFactorKey::KeyType& keytype) {
    std::ostringstream o;
    o << keytype;
    return o.str();
}

std::string toString(const RiskFactorKey& key) {
    std::ostringstream o;
    o << key;
    return o.str();
}

  RiskFactorKey parseRiskFactorKey(const std::string& s) {
    std::vector<std::string> tokens;
    boost::split(tokens, s, boost::is_any_of("/:;,|"));
    QL_REQUIRE(tokens.size() >= 3, "at least three tokens expected in risk factor key string " << s);
    static std::map<std::string, RiskFactorKey::KeyType> keys = {
        { "None", RiskFactorKey::KeyType::None },
        { "DiscountCurve", RiskFactorKey::KeyType::DiscountCurve },
        { "YieldCurve", RiskFactorKey::KeyType::YieldCurve },
        { "IndexCurve", RiskFactorKey::KeyType::IndexCurve },
        { "SwaptionVolatility", RiskFactorKey::KeyType::SwaptionVolatility },
        { "OptionletVolatility", RiskFactorKey::KeyType::OptionletVolatility },
        { "FXSpot", RiskFactorKey::KeyType::FXSpot },
        { "FXVolatility", RiskFactorKey::KeyType::FXVolatility }
    };
    RiskFactorKey::KeyType type;
    auto it = keys.find(tokens[0]);
    if (it != keys.end()) {
        type = it->second;
    } else {
        QL_FAIL("Cannot convert " << s << " to RiskFactorKey::KeyType");
    }
    string name = tokens[1];
    Size index = ore::data::parseInteger(tokens[2]);
    return RiskFactorKey(type, name, index);
}
}
}
