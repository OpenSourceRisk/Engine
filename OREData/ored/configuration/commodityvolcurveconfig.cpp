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

#include <ored/configuration/commodityvolcurveconfig.hpp>

#include <ql/errors.hpp>

namespace ore {
namespace data {

CommodityVolatilityCurveConfig::CommodityVolatilityCurveConfig(const string& curveId, const string& curveDescription, 
    const std::string& currency, const string& quote, const string& dayCounter, const string& calendar)
    : CurveConfig(curveId, curveDescription), currency_(currency), 
      type_(Type::Constant), dayCounter_(dayCounter), calendar_(calendar), extrapolate_(true),
      lowerStrikeConstantExtrapolation_(false), upperStrikeConstantExtrapolation_(false) {

    quotes_ = {quote};
}

CommodityVolatilityCurveConfig::CommodityVolatilityCurveConfig(const string& curveId, const string& curveDescription,
    const std::string& currency, const vector<string>& quotes, 
    const string& dayCounter, const string& calendar, bool extrapolate)
    : CurveConfig(curveId, curveDescription), currency_(currency),
      type_(Type::Curve), dayCounter_(dayCounter), calendar_(calendar), extrapolate_(extrapolate), 
      lowerStrikeConstantExtrapolation_(false), upperStrikeConstantExtrapolation_(false) {

    quotes_ = quotes;
}

CommodityVolatilityCurveConfig::CommodityVolatilityCurveConfig(const string& curveId, const string& curveDescription, 
    const string& currency, const vector<string>& expiries, const vector<string>& strikes, 
    const string & dayCounter, const string& calendar, bool extrapolate, 
    bool lowerStrikeConstantExtrapolation, bool upperStrikeConstantExtrapolation)
    : CurveConfig(curveId, curveDescription), currency_(currency), type_(Type::Surface), 
      expiries_(expiries), strikes_(strikes), dayCounter_(dayCounter), calendar_(calendar), extrapolate_(extrapolate),
      lowerStrikeConstantExtrapolation_(lowerStrikeConstantExtrapolation), 
      upperStrikeConstantExtrapolation_(upperStrikeConstantExtrapolation) {

    // Populate the quotes_ member
    buildQuotes();
}

void CommodityVolatilityCurveConfig::fromXML(XMLNode* node) {
    
    XMLUtils::checkNode(node, "CommodityVolatility");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);

    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    type_ = stringToType(XMLUtils::getChildValue(node, "Type", true));

    if (type_ == Type::Constant) {
        // If type is Constant, we expect a single Quote node
        quotes_ = { XMLUtils::getChildValue(node, "Quote", true) };
    } else if (type_ == Type::Curve) {
        // If type is Curve, we expect a Quotes node with one or more Quote nodes
        quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    } else if (type_ == Type::Surface) {
        // If type is Surface, we expect an Expiries and Strikes node
        XMLNode* surfaceNode = XMLUtils::getChildNode(node, "Surface");
        QL_REQUIRE(surfaceNode, "Expect a 'Surface' node when configuring a commodity volatility surface");
        expiries_ = XMLUtils::getChildrenValuesAsStrings(surfaceNode, "Expiries", true);
        strikes_ = XMLUtils::getChildrenValuesAsStrings(surfaceNode, "Strikes", true);
        buildQuotes();
    }

    // Should I set defaults here?
    // It is cleaner but it means toXML will return additional nodes possibly.
    dayCounter_ = XMLUtils::getChildValue(node, "DayCounter", false);
    if (dayCounter_ == "") dayCounter_ = "A365";
    calendar_ = XMLUtils::getChildValue(node, "Calendar", false);
    if (calendar_ == "") calendar_ = "NullCalendar";
    XMLNode* testNode = XMLUtils::getChildNode(node, "Extrapolation");
    extrapolate_ = testNode ? XMLUtils::getChildValueAsBool(node, "Extrapolation") : true;
    testNode = XMLUtils::getChildNode(node, "LowerStrikeConstantExtrapolation");
    lowerStrikeConstantExtrapolation_ = testNode ? 
        XMLUtils::getChildValueAsBool(node, "LowerStrikeConstantExtrapolation") : false;
    testNode = XMLUtils::getChildNode(node, "UpperStrikeConstantExtrapolation");
    upperStrikeConstantExtrapolation_ = testNode ?
        XMLUtils::getChildValueAsBool(node, "UpperStrikeConstantExtrapolation") : false;
}

XMLNode* CommodityVolatilityCurveConfig::toXML(XMLDocument& doc) {
    
    XMLNode* node = doc.allocNode("CommodityVolatility");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    XMLUtils::addChild(doc, node, "Currency", currency_);
    XMLUtils::addChild(doc, node, "Type", typeToString(type_));

    if (type_ == Type::Constant) {
        XMLUtils::addChild(doc, node, "Quote", quotes_[0]);
    } else if (type_ == Type::Curve) {
        XMLUtils::addChildren(doc, node, "Quotes", "Quote", quotes_);
    } else if (type_ == Type::Surface) {
        XMLNode* surfaceNode = doc.allocNode("Surface");
        XMLUtils::addGenericChildAsList(doc, surfaceNode, "Expiries", expiries_);
        XMLUtils::addGenericChildAsList(doc, surfaceNode, "Strikes", strikes_);
        XMLUtils::appendNode(node, surfaceNode);
    }

    XMLUtils::addChild(doc, node, "DayCounter", dayCounter_);
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolate_);
    XMLUtils::addChild(doc, node, "LowerStrikeConstantExtrapolation", lowerStrikeConstantExtrapolation_);
    XMLUtils::addChild(doc, node, "UpperStrikeConstantExtrapolation", upperStrikeConstantExtrapolation_);

    return node;
}

void CommodityVolatilityCurveConfig::buildQuotes() {
    // Called to initialise quotes_ when Type is Surface
    string base = "COMMODITY_OPTION/RATE_LNVOL/" + curveID_ + "/" + currency_ + "/";
    for (const string& e : expiries_) {
        for (const string& s : strikes_) {
            quotes_.push_back(base + e + "/" + s);
        }
    }
}

string CommodityVolatilityCurveConfig::typeToString(const Type& type) const {
    switch (type) {
    case Type::Constant:
        return "Constant";
    case Type::Curve:
        return "Curve";
    case Type::Surface:
        return "Surface";
    default:
        QL_FAIL("Can't convert commodity volatility type to string");
    }
}

CommodityVolatilityCurveConfig::Type CommodityVolatilityCurveConfig::stringToType(const string& type) const {
    if (type == "Constant") {
        return Type::Constant;
    } else if (type == "Curve") {
        return Type::Curve;
    } else if (type == "Surface") {
        return Type::Surface;
    } else {
        QL_FAIL("Cannot convert string '" << type << "' to commodity volatility type");
    }
}

}
}
