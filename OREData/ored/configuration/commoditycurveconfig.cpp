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

#include <ored/configuration/commoditycurveconfig.hpp>

using std::string;
using std::vector;

namespace ore {
namespace data {

CommodityCurveConfig::CommodityCurveConfig(const string& curveId, const string& curveDescription, const string& currency,
    const string& commoditySpotQuote, const vector<string>& quotes, const string& dayCountId,
    const string& interpolationMethod, bool extrapolation) 
    : CurveConfig(curveId, curveDescription), currency_(currency), commoditySpotQuoteId_(commoditySpotQuote), 
      dayCountId_(dayCountId), interpolationMethod_(interpolationMethod), extrapolation_(extrapolation) {
    
    quotes_ = quotes;
    quotes_.insert(quotes_.begin(), commoditySpotQuote);
}

void CommodityCurveConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CommodityCurve");

    curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
    curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    
    currency_ = XMLUtils::getChildValue(node, "Currency", true);
    dayCountId_ = XMLUtils::getChildValue(node, "DayCounter", false);
    commoditySpotQuoteId_ = XMLUtils::getChildValue(node, "SpotQuote", true);
    quotes_ = XMLUtils::getChildrenValues(node, "Quotes", "Quote", true);
    quotes_.insert(quotes_.begin(), commoditySpotQuoteId_);
    interpolationMethod_ = XMLUtils::getChildValue(node, "InterpolationMethod", false);
    extrapolation_ = XMLUtils::getChildValueAsBool(node, "Extrapolation");
}

XMLNode* CommodityCurveConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("CommodityCurve");

    XMLUtils::addChild(doc, node, "CurveId", curveID_);
    XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);

    XMLUtils::addChild(doc, node, "Currency", currency_);
    XMLUtils::addChild(doc, node, "DayCounter", dayCountId_);
    XMLUtils::addChild(doc, node, "SpotQuote", commoditySpotQuoteId_);
    vector<string> forwardQuotes(quotes_.begin() + 1, quotes_.end());
    XMLUtils::addChildren(doc, node, "Quotes", "Quote", forwardQuotes);
    XMLUtils::addChild(doc, node, "InterpolationMethod", interpolationMethod_);
    XMLUtils::addChild(doc, node, "Extrapolation", extrapolation_);

    return node;
}
}
}
