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

namespace {

struct StrikeToXML : public boost::static_visitor<XMLNode*> {
public:
    explicit StrikeToXML(XMLDocument& doc) : doc_(doc) {}
    XMLNode* operator()(StrikePrice& s) { return s.toXML(doc_); }
    XMLNode* operator()(StrikeYield& s) { return s.toXML(doc_); }
    XMLDocument& doc_;
};

struct StrikeValue : public boost::static_visitor<QuantLib::Real> {
public:
    QuantLib::Real operator()(const StrikePrice& s) const { return s.value(); }
    QuantLib::Real operator()(const StrikeYield& s) const { return s.yield(); }
};

} // namespace

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

void TradeStrike::fromXML(XMLNode* node, const bool allowYieldStrike) {
    XMLNode* dataNode = XMLUtils::getChildNode(node, "StrikeData");
    if (dataNode) {
        // first look for StrikeYield node
        if (XMLNode* yieldNode = XMLUtils::getChildNode(dataNode, "StrikeYield")) {
            QL_REQUIRE(allowYieldStrike, "StrikeYield not supported for this trade type.");
            StrikeYield strikeYield;
            strikeYield.fromXML(yieldNode);
            strike_ = strikeYield;
        } else {
            StrikePrice strikePrice;
            if (XMLNode* priceNode = XMLUtils::getChildNode(dataNode, "StrikePrice"))
                strikePrice.fromXML(priceNode);
            else {
                // in order to remain backward compatible we also allow to be set up
                // without the StrikePrice node
                
                strikePrice.fromXML(dataNode);
            }
            strike_ = strikePrice;
        }
    } else {
        // if just a strike is present
        strike_ = StrikePrice(parseReal(XMLUtils::getChildValue(node, "Strike", true)));
        onlyStrike_ = true;
    }
}

XMLNode* TradeStrike::toXML(XMLDocument& doc) {
    XMLNode* node;
    if (onlyStrike_)
        node = doc.allocNode("Strike", boost::lexical_cast<std::string>(value()));
    else {
        node = doc.allocNode("StrikeData");
        if (noStrikePriceNode_) {
            // maintain backward compatibility, must be a StrikePrice to get here
            StrikePrice sp = boost::get<StrikePrice>(*strike_);
            sp.toXMLNode(doc, node);
        } else {
            StrikeToXML toXml(doc);
            XMLUtils::appendNode(node, boost::apply_visitor(toXml, *strike_));
        }
    }
    return node;
}

const bool TradeStrike::isStrikePrice() const {
    QL_REQUIRE(strike_, "no bond price or yield given");
    return (*strike_).which() == 0;
}

QuantLib::Real TradeStrike::value() { 
    return boost::apply_visitor(StrikeValue(), *strike_);
}

} // namespace data
} // namespace ore