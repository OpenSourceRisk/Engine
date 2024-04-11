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

#include <ored/marketdata/curvespecparser.hpp>

#include <ql/errors.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <boost/tokenizer.hpp>

#include <map>
#include <vector>

using namespace std;

namespace ore {
namespace data {

static CurveSpec::CurveType parseCurveSpecType(const string& s) {
    static map<string, CurveSpec::CurveType> b = {
        {"Yield", CurveSpec::CurveType::Yield},
        {"CapFloorVolatility", CurveSpec::CurveType::CapFloorVolatility},
        {"SwaptionVolatility", CurveSpec::CurveType::SwaptionVolatility},
        {"YieldVolatility", CurveSpec::CurveType::YieldVolatility},
        {"FX", CurveSpec::CurveType::FX},
        {"FXVolatility", CurveSpec::CurveType::FXVolatility},
        {"Default", CurveSpec::CurveType::Default},
        {"CDSVolatility", CurveSpec::CurveType::CDSVolatility},
        {"BaseCorrelation", CurveSpec::CurveType::BaseCorrelation},
        {"Inflation", CurveSpec::CurveType::Inflation},
        {"InflationCapFloorVolatility", CurveSpec::CurveType::InflationCapFloorVolatility},
        {"Equity", CurveSpec::CurveType::Equity},
        {"EquityVolatility", CurveSpec::CurveType::EquityVolatility},
        {"Security", CurveSpec::CurveType::Security},
        {"Commodity", CurveSpec::CurveType::Commodity},
        {"Correlation", CurveSpec::CurveType::Correlation},
        {"CommodityVolatility", CurveSpec::CurveType::CommodityVolatility}};

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert \"" << s << "\" to CurveSpecType");
    }
}

//! function to convert a string into a curve spec
QuantLib::ext::shared_ptr<CurveSpec> parseCurveSpec(const string& s) {


    boost::escaped_list_separator<char> sep('\\', '/', '\"');
    boost::tokenizer<boost::escaped_list_separator<char> > tokenSplit(s, sep);

    vector<string> tokens(tokenSplit.begin(), tokenSplit.end());

    QL_REQUIRE(tokens.size() > 1, "number of tokens too small in curve spec " << s);

    CurveSpec::CurveType curveType = parseCurveSpecType(tokens[0]);

    switch (curveType) {

    case CurveSpec::CurveType::Yield: {
        // Expected format: Yield/CCY/CurveConfigID
        // Example: Yield/EUR/eur-6M-curve
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in yield curve spec "
                                           << s);
        const string& ccy = tokens[1];
        const string& curveConfigID = tokens[2];
        return QuantLib::ext::make_shared<YieldCurveSpec>(ccy, curveConfigID);
    }

    case CurveSpec::CurveType::Default: {
        // Default/USD/CurveConfigID
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in default curve spec "
                                           << s);
        const string& ccy = tokens[1];
        const string& curveConfigID = tokens[2];
        return QuantLib::ext::make_shared<DefaultCurveSpec>(ccy, curveConfigID);
    }

    case CurveSpec::CurveType::CDSVolatility: {
        // CDSVolatility/CurveConfigID
        QL_REQUIRE(tokens.size() == 2, "Unexpected number"
                                       " of tokens in cds vol spec "
                                           << s);
        const string& curveConfigID = tokens[1];
        return QuantLib::ext::make_shared<CDSVolatilityCurveSpec>(curveConfigID);
    }

    case CurveSpec::CurveType::BaseCorrelation: {
        // BaseCorrelation/CurveConfigID
        QL_REQUIRE(tokens.size() == 2, "Unexpected number"
                                       " of tokens in cds vol spec "
                                           << s);
        const string& curveConfigID = tokens[1];
        return QuantLib::ext::make_shared<BaseCorrelationCurveSpec>(curveConfigID);
    }

    case CurveSpec::CurveType::FX: {
        // FX/USD/CHF
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in FX curve spec "
                                           << s);
        const string& unitCcy = tokens[1];
        const string& ccy = tokens[2];
        return QuantLib::ext::make_shared<FXSpotSpec>(unitCcy, ccy);
    }

    case CurveSpec::CurveType::FXVolatility: {
        // FX/USD/CHF/CurveConfigID
        QL_REQUIRE(tokens.size() == 4, "Unexpected number"
                                       " of tokens in fx vol curve spec "
                                           << s);
        const string& unitCcy = tokens[1];
        const string& ccy = tokens[2];
        const string& curveConfigID = tokens[3];
        return QuantLib::ext::make_shared<FXVolatilityCurveSpec>(unitCcy, ccy, curveConfigID);
    }

    case CurveSpec::CurveType::SwaptionVolatility: {
        // SwaptionVolatility/EUR/CurveConfigID
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in swaption vol curve spec "
                                           << s);
        const string& key = tokens[1];
        const string& curveConfigID = tokens[2];
        return QuantLib::ext::make_shared<SwaptionVolatilityCurveSpec>(key, curveConfigID);
    }

    case CurveSpec::CurveType::YieldVolatility: {
        // YieldVolatility/CurveConfigID
        QL_REQUIRE(tokens.size() == 2, "Unexpected number"
                                       " of tokens in yield vol curve spec "
                                           << s);
        const string& curveConfigID = tokens[1];
        return QuantLib::ext::make_shared<YieldVolatilityCurveSpec>(curveConfigID);
    }

    case CurveSpec::CurveType::CapFloorVolatility: {
        // e.g. CapFloorVolatility/EUR-EURIBOR-3M/CurveConfigID
        //      CapFloorVolatility/EUR-ESTER/CurveConfigID
        //      CapFloorVolatility/EUR/CurveConfigID
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in CapFloor volatility curve spec "
                                           << s);
        const string& key = tokens[1];
        const string& curveConfigID = tokens[2];
        return QuantLib::ext::make_shared<CapFloorVolatilityCurveSpec>(key, curveConfigID);
    }

    case CurveSpec::CurveType::Inflation: {
        // Inflation/EUHICPXT/CurveConfigID
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in inflation curve spec "
                                           << s);
        const string& index = tokens[1];
        const string& curveConfigID = tokens[2];
        return QuantLib::ext::make_shared<InflationCurveSpec>(index, curveConfigID);
    }

    case CurveSpec::CurveType::InflationCapFloorVolatility: {
        // e.g. InflationCapFloorVolatility/EUHICPXT/CurveConfigID
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in InflationCapFloor volatility curve spec "
                                           << s);
        const string& index = tokens[1];
        const string& curveConfigID = tokens[2];
        return QuantLib::ext::make_shared<InflationCapFloorVolatilityCurveSpec>(index, curveConfigID);
    }

    case CurveSpec::CurveType::Equity: {
        // Equity/USD/CurveConfigID
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in default curve spec "
                                           << s);
        const string& ccy = tokens[1];
        const string& curveConfigID = tokens[2];
        return QuantLib::ext::make_shared<EquityCurveSpec>(ccy, curveConfigID);
    }

    case CurveSpec::CurveType::EquityVolatility: {
        // EquityVolatility/USD/CurveConfigID
        QL_REQUIRE(tokens.size() == 3, "Unexpected number"
                                       " of tokens in default curve spec "
                                           << s);
        const string& ccy = tokens[1];
        const string& curveConfigID = tokens[2];
        return QuantLib::ext::make_shared<EquityVolatilityCurveSpec>(ccy, curveConfigID);
    }

    case CurveSpec::CurveType::Security: {
        // Security/ISIN
        QL_REQUIRE(tokens.size() == 2, "Unexpected number"
                                       " of tokens in Security Spread spec "
                                           << s);
        const string& securityID = tokens[1];
        return QuantLib::ext::make_shared<SecuritySpec>(securityID);
    }

    case CurveSpec::CurveType::Commodity: {
        // Commodity/CCY/CommodityCurveConfigId
        QL_REQUIRE(tokens.size() == 3, "Unexpected number of tokens in commodity curve spec " << s);
        return QuantLib::ext::make_shared<CommodityCurveSpec>(tokens[1], tokens[2]);
    }

    case CurveSpec::CurveType::CommodityVolatility: {
        // CommodityVolatility/CCY/CommodityVolatilityConfigId
        QL_REQUIRE(tokens.size() == 3, "Unexpected number of tokens in commodity volatility spec " << s);
        return QuantLib::ext::make_shared<CommodityVolatilityCurveSpec>(tokens[1], tokens[2]);
    }

    case CurveSpec::CurveType::Correlation: {
        // Correlation/CorrelationCurveConfigId
        QL_REQUIRE(tokens.size() == 2, "Unexpected number of tokens in correlation spec " << s);
        string id = tokens[1];
        return QuantLib::ext::make_shared<CorrelationCurveSpec>(id);
    }

        // TODO: the rest...
    }

    QL_FAIL("Unable to convert \"" << s << "\" into CurveSpec");
}


CurveSpec::CurveType parseCurveConfigurationType(const std::string& s) {
    static const map<string, CurveSpec::CurveType> b = {
        {"YieldCurves", CurveSpec::CurveType::Yield},
        {"CapFloorVolatilities", CurveSpec::CurveType::CapFloorVolatility},
        {"SwaptionVolatilities", CurveSpec::CurveType::SwaptionVolatility},
        {"YieldVolatilities", CurveSpec::CurveType::YieldVolatility},
        {"FXSpots", CurveSpec::CurveType::FX},
        {"FXVolatilities", CurveSpec::CurveType::FXVolatility},
        {"DefaultCurves", CurveSpec::CurveType::Default},
        {"CDSVolatilities", CurveSpec::CurveType::CDSVolatility},
        {"BaseCorrelations", CurveSpec::CurveType::BaseCorrelation},
        {"InflationCurves", CurveSpec::CurveType::Inflation},
        {"InflationCapFloorVolatilities", CurveSpec::CurveType::InflationCapFloorVolatility},
        {"EquityCurves", CurveSpec::CurveType::Equity},
        {"EquityVolatilities", CurveSpec::CurveType::EquityVolatility},
        {"Securities", CurveSpec::CurveType::Security},
        {"CommodityCurves", CurveSpec::CurveType::Commodity},
        {"Correlations", CurveSpec::CurveType::Correlation},
        {"CommodityVolatilities", CurveSpec::CurveType::CommodityVolatility}};

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert \"" << s << "\" to CurveSpecType");
    }
}

} // namespace data
} // namespace ore
