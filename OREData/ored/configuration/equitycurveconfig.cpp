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
    const string& currency, const EquityCurveConfig::Type& type,
    const string& equitySpotQuote, const string& equityInterestRateCurve,
    const vector<string>& quotes, const string& conventionID, bool extrapolation)
    : curveID_(curveID), curveDescription_(curveDescription), currency_(currency), type_(type),
    equitySpotQuoteID_(equitySpotQuote), equityInterestRateCurveID_(equityInterestRateCurve),
    quotes_(quotes), conventionID_(conventionID), extrapolation_(extrapolation) {}

void EquityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "EquityCurve");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    currency_ = XMLUtils::getChildValue(node, "Currency", true);

    string type = XMLUtils::getChildValue(node, "Type", true);
    if (type == "DividendYield") {
        type_ = Type::DividendYield;
    } else if (type == "ForwardPrice") {
        type_ = Type::ForwardPrice;
    } else {
        QL_FAIL("Type " << type << " not recognized");
    }

    equityInterestRateCurveID_ = XMLUtils::getChildValue(node, "InterestRateCurve", true);
    equitySpotQuoteID_ = XMLUtils::getChildValue(node, "SpotQuote", true);
    conventionID_ = XMLUtils::getChildValue(node, "Conventions", false);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    extrapolation_ = XMLUtils::getChildValueAsBool(node, "Extrapolation"); // defaults to true
}

XMLNode* EquityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("EquityCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
    XMLUtils::addChild(doc, node, "Currency", currency_);

    if (type_ == Type::DividendYield)
        XMLUtils::addChild(doc, node, "Type", "DividendYield");
    else if (type_ == Type::ForwardPrice)
        XMLUtils::addChild(doc, node, "Type", "ForwardPrice");
    else
     QL_FAIL("Unkown type in EquityCurveConfig::toXML()");

    XMLUtils::addChild(doc, node, "SpotQuote", equitySpotQuoteID_);
    XMLUtils::addChild(doc, node, "InterestRateCurve", equityInterestRateCurveID_);
    XMLUtils::addChild(doc, node, "Conventions", conventionID_);
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);

    return node;
}
}
}
