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
    case RiskFactorKey::KeyType::EquitySpot:
        return out << "EquitySpot";
    case RiskFactorKey::KeyType::EquityVolatility:
        return out << "EquityVolatility";
    case RiskFactorKey::KeyType::EquityForecastCurve:
        return out << "EquityForecastCurve";
    case RiskFactorKey::KeyType::DividendYield:
        return out << "DividendYield";
    case RiskFactorKey::KeyType::SurvivalProbability:
        return out << "SurvivalProbability";
    case RiskFactorKey::KeyType::RecoveryRate:
        return out << "RecoveryRate";
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
    case RiskFactorKey::KeyType::CommoditySpot:
        return out << "CommoditySpot";
    case RiskFactorKey::KeyType::CommodityCurve:
        return out << "CommodityCurve";
    case RiskFactorKey::KeyType::CommodityVolatility:
        return out << "CommodityVolatility";
    case RiskFactorKey::KeyType::SecuritySpread:
        return out << "SecuritySpread";
    default:
        return out << "?";
    }
}

std::ostream& operator<<(std::ostream& out, const RiskFactorKey& key) {
    // If empty key just return empty string (not "?//0")
    if (key == RiskFactorKey()) {
        return out << "";
    }

    // If not empty key
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
    else if (str == "EquityForecastCurve")
        return RiskFactorKey::KeyType::EquityForecastCurve;
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
    else if (str == "CommoditySpot")
        return RiskFactorKey::KeyType::CommoditySpot;
    else if (str == "CommodityCurve")
        return RiskFactorKey::KeyType::CommodityCurve;
    else if (str == "CommodityVolatility")
        return RiskFactorKey::KeyType::CommodityVolatility;
    else if (str == "SecuritySpread")
        return RiskFactorKey::KeyType::SecuritySpread;

    QL_FAIL("RiskFactorKey " << str << " does not exist.");
}

RiskFactorKey parseRiskFactorKey(const string& str) {
    std::vector<string> tokens;
    boost::split(tokens, str, boost::is_any_of("/"), boost::token_compress_on);
    QL_REQUIRE(tokens.size() == 3, "Could not parse key " << str);
    RiskFactorKey rfk(parseRiskFactorKeyType(tokens[0]), tokens[1], parseInteger(tokens[2]));
    return rfk;
}
} // namespace analytics
} // namespace ore
