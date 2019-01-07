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

#include <ored/configuration/equitycurveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

EquityCurveConfig::EquityCurveConfig(const string& curveID, const string& curveDescription,
                                     const string& forecastingCurve, const string& currency,
                                     const EquityCurveConfig::Type& type, const string& equitySpotQuote,
                                     const vector<string>& fwdQuotes, const string& dayCountID,
                                     const string& dividendInterpVariable, const string& dividendInterpMethod,
                                     bool extrapolation)
    : CurveConfig(curveID, curveDescription), fwdQuotes_(fwdQuotes), forecastingCurve_(forecastingCurve),
      currency_(currency), type_(type), equitySpotQuoteID_(equitySpotQuote), dayCountID_(dayCountID),
      divInterpVariable_(dividendInterpVariable), divInterpMethod_(dividendInterpMethod),
      extrapolation_(extrapolation) {
    quotes_ = fwdQuotes;
    quotes_.insert(quotes_.begin(), equitySpotQuote);
}

void EquityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "EquityCurve");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    forecastingCurve_ = XMLUtils::getChildValue(node, "ForecastingCurve", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    type_ = parseEquityCurveConfigType(XMLUtils::getChildValue(node, "Type", true));
    equitySpotQuoteID_ = XMLUtils::getChildValue(node, "SpotQuote", true);
    dayCountID_ = XMLUtils::getChildValue(node, "DayCounter", false);
    fwdQuotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote");
    quotes_ = fwdQuotes_;
    quotes_.insert(quotes_.begin(), equitySpotQuoteID_);

    XMLNode* divInterpNode = XMLUtils::getChildNode(node, "DividendInterpolation");
    if (divInterpNode) {
        divInterpVariable_ = XMLUtils::getChildValue(divInterpNode, "InterpolationVariable", true);
        divInterpMethod_ = XMLUtils::getChildValue(divInterpNode, "InterpolationMethod", true);
    } else {
        divInterpVariable_ = "Zero";
        divInterpMethod_ = divInterpVariable_ == "Zero" ? "Linear" : "LogLinear";
    }

    extrapolation_ = XMLUtils::getChildValueAsBool(node, "Extrapolation"); // defaults to true

    if (type_ == Type::NoDividends) {
        QL_REQUIRE(fwdQuotes_.size() == 0, "Invalid EquityCurveConfig, no Quotes should be present when type=NoDividends");
        QL_REQUIRE(divInterpNode == nullptr, "Invalid EquityCurveConfig, no DividendInterpolation should be present when type=NoDividends");
    } else {
        QL_REQUIRE(fwdQuotes_.size() > 0, "Invalid EquityCurveConfig, Quotes should be present when type!=NoDividends");
    }
}

XMLNode* EquityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("EquityCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "ForecastingCurve", forecastingCurve_);
    XMLUtils::addChild(doc, node, "Currency", currency_);
    XMLUtils::addChild(doc, node, "Type", to_string(type_));
    XMLUtils::addChild(doc, node, "SpotQuote", equitySpotQuoteID_);
    XMLUtils::addChild(doc, node, "DayCounter", dayCountID_);
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", fwdQuotes_);

    XMLNode* divInterpNode = XMLUtils::addChild(doc, node, "DividendInterpolation");
    XMLUtils::addChild(doc, divInterpNode, "InterpolationVariable", divInterpVariable_);
    XMLUtils::addChild(doc, divInterpNode, "InterpolationMethod", divInterpMethod_);

    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);

    return node;
}

std::ostream& operator<<(std::ostream& out, EquityCurveConfig::Type t) {
    switch (t) {
    case EquityCurveConfig::Type::DividendYield:
        return out << "DividendYield";
    case EquityCurveConfig::Type::ForwardPrice:
        return out << "ForwardPrice";
    case EquityCurveConfig::Type::NoDividends:
        return out << "NoDividends";
    default:
        QL_FAIL("unknown EquityCurveConfig::Type(" << int(t) << ")");
    }
}

EquityCurveConfig::Type parseEquityCurveConfigType(const std::string& str) {
    if (str == "DividendYield")
        return EquityCurveConfig::Type::DividendYield;
    else if (str == "ForwardPrice")
        return EquityCurveConfig::Type::ForwardPrice;
    else if (str == "NoDividends")
        return EquityCurveConfig::Type::NoDividends;
    QL_FAIL("Invalid EquityCurveConfig::Type " << str);
}

} // namespace data
} // namespace ore
