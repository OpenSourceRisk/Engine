/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/portfolio/tradestrike.hpp>
#include <ored/utilities/parsers.hpp>
#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

void StrikeYield::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "StrikeYield");
    yield_ = XMLUtils::getChildValueAsDouble(node, "Yield", true);
    compounding_ = parseCompounding(XMLUtils::getChildValue(node, "Compounding", false, "SimpleThenCompounded"));
}

XMLNode* StrikeYield::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("StrikeYield");
    XMLUtils::addChild(doc, node, "Yield", yield_);
    XMLUtils::addChild(doc, node, "Compounding", compounding_);
    return node;
}

void StrikePrice::fromXML(XMLNode* node) {
    TradeMonetary::fromXMLNode(node);
}

XMLNode* StrikePrice::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("StrikePrice");
    TradeMonetary::toXMLNode(doc, node);
    return node;
}

void TradeStrike::fromXML(XMLNode* node, const bool isRequired, const bool allowYieldStrike) {
    XMLNode* dataNode = XMLUtils::getChildNode(node, "StrikeData");
    if (dataNode) {
        // first look for StrikeYield node
        if (XMLNode* yieldNode = XMLUtils::getChildNode(dataNode, "StrikeYield")) {
            QL_REQUIRE(allowYieldStrike, "StrikeYield not supported for this trade type.");
            StrikeYield strikeYield;
            strikeYield.fromXML(yieldNode);
            strike_ = boost::make_shared<StrikeYield>(strikeYield);
        } else {
            StrikePrice strikePrice;
            if (XMLNode* priceNode = XMLUtils::getChildNode(dataNode, "StrikePrice"))
                strikePrice.fromXML(priceNode);
            else {
                // in order to remain backward compatible we also allow to be set up
                // without the StrikePrice node                
                strikePrice.fromXML(dataNode);
                noStrikePriceNode_ = true;                
            }
            strike_ = boost::make_shared<StrikePrice>(strikePrice);
        }
    } else {
        // if just a strike is present
        string s = XMLUtils::getChildValue(node, "Strike", isRequired);
        if (!s.empty()) {
            strike_ = boost::make_shared<StrikePrice>(s);
            onlyStrike_ = true;
        }
    }
}

XMLNode* TradeStrike::toXML(XMLDocument& doc) {
    XMLNode* node;
    if (onlyStrike_) {
        auto sp = boost::dynamic_pointer_cast<StrikePrice>(strike_);
        node = doc.allocNode("Strike", boost::lexical_cast<std::string>(sp->valueString()));
    } else if (strike_) {
        node = doc.allocNode("StrikeData");
        if (noStrikePriceNode_) {
            // maintain backward compatibility, must be a StrikePrice to get here
            auto sp = boost::dynamic_pointer_cast<StrikePrice>(strike_);
            sp->toXMLNode(doc, node);
        } else {
            XMLUtils::appendNode(node, strike_->toXML(doc));            
        }
    }
    return node;
}

const bool TradeStrike::isStrikePrice() const {
    QL_REQUIRE(strike_, "no bond price or yield given");
    return (boost::dynamic_pointer_cast<StrikePrice>(strike_) ? true : false);
}

QuantLib::Real TradeStrike::value() const { 
    return strike_->strikeValue(); 
}

} // namespace data
} // namespace ore