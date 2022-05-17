/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/configuration/smiledynamicsconfig.hpp>

#include <ored/utilities/parsers.hpp>
#include <algorithm>

namespace ore {
namespace data {

using namespace QuantLib;

void SmileDynamicsConfig::validate() {
    std::vector<string> v =  { "StickyStrike", "StickyMoneyness" };
    QL_REQUIRE(std::find(v.begin(), v.end(), swaption_) != v.end(), "swaption smile dynamics " << swaption_ << " invalid");  
    QL_REQUIRE(std::find(v.begin(), v.end(), capFloor_) != v.end(), "capfloor smile dynamics " << capFloor_ << " invalid");  
    QL_REQUIRE(std::find(v.begin(), v.end(), yield_) != v.end(), "yield vol smile dynamics " << yield_ << " invalid");  
    QL_REQUIRE(std::find(v.begin(), v.end(), zeroInflationCapFloor_) != v.end(), "ZCI vol smile dynamics " << zeroInflationCapFloor_ << " invalid");  
    QL_REQUIRE(std::find(v.begin(), v.end(), yoyInflationCapFloor_) != v.end(), "YOY smile dynamics " << yoyInflationCapFloor_ << " invalid");  
    QL_REQUIRE(std::find(v.begin(), v.end(), equity_) != v.end(), "equity vol smile dynamics " << equity_ << " invalid");  
    QL_REQUIRE(std::find(v.begin(), v.end(), commodity_) != v.end(), "commodity vol smile dynamics " << commodity_ << " invalid");  
    QL_REQUIRE(std::find(v.begin(), v.end(), fx_) != v.end(), "FX vol smile dynamics " << fx_ << " invalid");  
    QL_REQUIRE(std::find(v.begin(), v.end(), cds_) != v.end(), "CDS vol smile dynamics " << cds_ << " invalid");  
}
  
void SmileDynamicsConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "SmileDynamics");

    swaption_ = XMLUtils::getChildValue(node, "Swaption", false, "StickyStrike");
    capFloor_ = XMLUtils::getChildValue(node, "CapFloor", false, "StickyStrike");
    yield_ = XMLUtils::getChildValue(node, "Yield", false, "StickyStrike");
    zeroInflationCapFloor_ = XMLUtils::getChildValue(node, "ZeroInflationCapFloor", false, "StickyStrike");
    yoyInflationCapFloor_ = XMLUtils::getChildValue(node, "YoyInflationCapFloor", false, "StickyStrike");
    equity_ = XMLUtils::getChildValue(node, "Equity", false, "StickyStrike");
    commodity_ = XMLUtils::getChildValue(node, "Commodity", false, "StickyStrike");
    fx_ = XMLUtils::getChildValue(node, "FX", false, "StickyStrike");
    cds_ = XMLUtils::getChildValue(node, "CDS", false, "StickyStrike");

    validate();
}

XMLNode* SmileDynamicsConfig::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("SmileDynamics");

    XMLUtils::addChild(doc, node, "Swaption", swaption_);
    XMLUtils::addChild(doc, node, "CapFloor", capFloor_);
    XMLUtils::addChild(doc, node, "Yield", yield_);
    XMLUtils::addChild(doc, node, "ZeroInflationCapFloor", zeroInflationCapFloor_);
    XMLUtils::addChild(doc, node, "YoyInflationCapFloor", yoyInflationCapFloor_);
    XMLUtils::addChild(doc, node, "Equity", equity_);
    XMLUtils::addChild(doc, node, "Commodity", commodity_);
    XMLUtils::addChild(doc, node, "FX", fx_);
    XMLUtils::addChild(doc, node, "CDS", cds_);

    return node;
}

} // namespace data
} // namespace ore
