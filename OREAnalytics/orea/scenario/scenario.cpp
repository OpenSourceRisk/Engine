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

#include <boost/algorithm/string/split.hpp>
#include <orea/scenario/scenario.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>
#include <vector>

using namespace ore::data;

namespace ore {
namespace analytics {

std::size_t hash_value(const RiskFactorKey& k) {
    std::size_t seed = 0;
    boost::hash_combine(seed, k.keytype);
    boost::hash_combine(seed, k.name);
    boost::hash_combine(seed, k.index);
    return seed;
}

bool Scenario::isCloseEnough(const QuantLib::ext::shared_ptr<Scenario>& s) const {
    return asof() == s->asof() && label() == s->label() && QuantLib::close_enough(getNumeraire(), s->getNumeraire()) &&
           keys() == s->keys() && std::all_of(keys().begin(), keys().end(), [this, s](const RiskFactorKey& k) {
               return QuantLib::close_enough(this->get(k), s->get(k));
           });
}

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
    case RiskFactorKey::KeyType::YieldVolatility:
        return out << "YieldVolatility";
    case RiskFactorKey::KeyType::OptionletVolatility:
        return out << "OptionletVolatility";
    case RiskFactorKey::KeyType::FXSpot:
        return out << "FXSpot";
    case RiskFactorKey::KeyType::FXVolatility:
        return out << "FXVolatility";
    case RiskFactorKey::KeyType::EquitySpot:
        return out << "EquitySpot";
    case RiskFactorKey::KeyType::EquityVolatility:
        return out << "EquityVolatility";
    case RiskFactorKey::KeyType::DividendYield:
        return out << "DividendYield";
    case RiskFactorKey::KeyType::SurvivalProbability:
        return out << "SurvivalProbability";
    case RiskFactorKey::KeyType::SurvivalWeight:
        return out << "SurvivalWeight";
    case RiskFactorKey::KeyType::RecoveryRate:
        return out << "RecoveryRate";
    case RiskFactorKey::KeyType::CreditState:
        return out << "CrState";
    case RiskFactorKey::KeyType::CDSVolatility:
        return out << "CDSVolatility";
    case RiskFactorKey::KeyType::BaseCorrelation:
        return out << "BaseCorrelation";
    case RiskFactorKey::KeyType::CPIIndex:
        return out << "CPIIndex";
    case RiskFactorKey::KeyType::ZeroInflationCurve:
        return out << "ZeroInflationCurve";
    case RiskFactorKey::KeyType::YoYInflationCurve:
        return out << "YoYInflationCurve";
    case RiskFactorKey::KeyType::YoYInflationCapFloorVolatility:
        return out << "YoYInflationCapFloorVolatility";
    case RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility:
        return out << "ZeroInflationCapFloorVolatility";
    case RiskFactorKey::KeyType::CommodityCurve:
        return out << "CommodityCurve";
    case RiskFactorKey::KeyType::CommodityVolatility:
        return out << "CommodityVolatility";
    case RiskFactorKey::KeyType::SecuritySpread:
        return out << "SecuritySpread";
    case RiskFactorKey::KeyType::Correlation:
        return out << "Correlation";
    case RiskFactorKey::KeyType::CPR:
        return out << "CPR";
    default:
        return out << "?";
    }
}

std::ostream& operator<<(std::ostream& out, const RiskFactorKey& key) {
    // If empty key just return empty string (not "?//0")
    if (key == RiskFactorKey()) {
        return out << "";
    }

    string keyStr = key.name;
    size_t index = 0;
    while (true) {
        /* Locate the substring to replace. */
        index = keyStr.find("/", index);
        if (index == std::string::npos)
            break;

        keyStr.replace(index, 1, "\\/");
        index += 2;
    }

    // If not empty key
    return out << key.keytype << "/" << keyStr << "/" << key.index;
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
    else if (str == "YieldVolatility")
        return RiskFactorKey::KeyType::YieldVolatility;
    else if (str == "OptionletVolatility")
        return RiskFactorKey::KeyType::OptionletVolatility;
    else if (str == "FXSpot")
        return RiskFactorKey::KeyType::FXSpot;
    else if (str == "FXVolatility")
        return RiskFactorKey::KeyType::FXVolatility;
    else if (str == "EquitySpot")
        return RiskFactorKey::KeyType::EquitySpot;
    else if (str == "EquityVolatility")
        return RiskFactorKey::KeyType::EquityVolatility;
    else if (str == "DividendYield")
        return RiskFactorKey::KeyType::DividendYield;
    else if (str == "SurvivalProbability")
        return RiskFactorKey::KeyType::SurvivalProbability;
    else if (str == "RecoveryRate")
        return RiskFactorKey::KeyType::RecoveryRate;
    else if (str == "CDSVolatility")
        return RiskFactorKey::KeyType::CDSVolatility;
    else if (str == "BaseCorrelation")
        return RiskFactorKey::KeyType::BaseCorrelation;
    else if (str == "CPIIndex")
        return RiskFactorKey::KeyType::CPIIndex;
    else if (str == "ZeroInflationCurve")
        return RiskFactorKey::KeyType::ZeroInflationCurve;
    else if (str == "YoYInflationCurve")
        return RiskFactorKey::KeyType::YoYInflationCurve;
    else if (str == "YoYInflationCapFloorVolatility")
        return RiskFactorKey::KeyType::YoYInflationCapFloorVolatility;
    else if (str == "ZeroInflationCapFloorVolatility")
        return RiskFactorKey::KeyType::ZeroInflationCapFloorVolatility;
    else if (str == "CommodityCurve")
        return RiskFactorKey::KeyType::CommodityCurve;
    else if (str == "CommodityVolatility")
        return RiskFactorKey::KeyType::CommodityVolatility;
    else if (str == "SecuritySpread")
        return RiskFactorKey::KeyType::SecuritySpread;
    else if (str == "Correlation")
        return RiskFactorKey::KeyType::Correlation;
    else if (str == "CPR")
        return RiskFactorKey::KeyType::CPR;

    QL_FAIL("RiskFactorKey " << str << " does not exist.");
}

RiskFactorKey parseRiskFactorKey(const string& str) {
    boost::escaped_list_separator<char> sep('\\', '/', '\"');
    boost::tokenizer<boost::escaped_list_separator<char> > tokenSplit(str, sep);
    vector<string> tokens(tokenSplit.begin(), tokenSplit.end());

    QL_REQUIRE(tokens.size() == 3, "Could not parse key " << str);
    RiskFactorKey rfk(parseRiskFactorKeyType(tokens[0]), tokens[1], parseInteger(tokens[2]));
    return rfk;
}

ShiftScheme parseShiftScheme(const std::string& s) {
    static map<string, ShiftScheme> m = {{"Forward", ShiftScheme::Forward},
                                         {"Backward", ShiftScheme::Backward},
                                         {"Central", ShiftScheme::Central}};
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert shift scheme \"" << s << "\" to ShiftScheme");
    }
}

std::ostream& operator<<(std::ostream& out, const ShiftScheme& shiftScheme) {
    if (shiftScheme == ShiftScheme::Forward)
        return out << "Forward";
    else if (shiftScheme == ShiftScheme::Backward)
        return out << "Backward";
    else if (shiftScheme == ShiftScheme::Central)
        return out << "Central";
    else
        QL_FAIL("Invalid ShiftScheme " << static_cast<int>(shiftScheme));
}

ShiftType parseShiftType(const std::string& s) {
    static map<string, ShiftType> m = {
        {"Absolute", ShiftType::Absolute},
        {"Relative", ShiftType::Relative}};
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert shift type \"" << s << "\" to ShiftType");
    }
}

std::ostream& operator<<(std::ostream& out, const ShiftType& shiftType) {
    if (shiftType == ShiftType::Absolute)
        return out << "Absolute";
    else if (shiftType == ShiftType::Relative)
        return out << "Relative";
    else
        QL_FAIL("Invalid ShiftType " << shiftType);
}

} // namespace analytics
} // namespace ore
