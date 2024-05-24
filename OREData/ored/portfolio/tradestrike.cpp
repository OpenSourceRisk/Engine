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

struct StrikeValue : public boost::static_visitor<QuantLib::Real> {
public:
    QuantLib::Real operator()(const TradeMonetary& s) const { return s.value(); }
    QuantLib::Real operator()(const TradeStrike::StrikeYield& s) const { return s.yield; }
};

} // namespace

TradeStrike::TradeStrike(Type type, const QuantLib::Real& value) : type_(type) { 
    if (type_ == Type::Price)
        strike_ = TradeMonetary(value);
    else
        strike_ = StrikeYield(value);
}

TradeStrike::TradeStrike(const QuantLib::Real& value, const std::string& currency) : type_(Type::Price) {
    strike_ = TradeMonetary(value, currency);
}

TradeStrike::TradeStrike(const QuantLib::Real& value, QuantLib::Compounding compounding) : type_(Type::Yield) {
    strike_ = StrikeYield(value, compounding);
}

void TradeStrike::fromXML(XMLNode* node, const bool isRequired, const bool allowYieldStrike) {
    XMLNode* dataNode = XMLUtils::getChildNode(node, "StrikeData");
    if (dataNode) {
        // first look for StrikeYield node
        if (XMLNode* yieldNode = XMLUtils::getChildNode(dataNode, "StrikeYield")) {
            QL_REQUIRE(allowYieldStrike, "StrikeYield not supported for this trade type.");
            StrikeYield strikeYield;
            strikeYield.yield = XMLUtils::getChildValueAsDouble(yieldNode, "Yield", true);
            strikeYield.compounding =
                parseCompounding(XMLUtils::getChildValue(yieldNode, "Compounding", false, "SimpleThenCompounded"));
            strike_ = strikeYield;
            type_ = Type::Yield;
        } else {
            TradeMonetary strikePrice;
            if (XMLNode* priceNode = XMLUtils::getChildNode(dataNode, "StrikePrice"))
                strikePrice.fromXMLNode(priceNode);
            else {
                // in order to remain backward compatible we also allow to be set up
                // without the StrikePrice node                
                strikePrice.fromXMLNode(dataNode);
                noStrikePriceNode_ = true;                
            }
            strike_ = strikePrice;
            type_ = Type::Price;
        }
    } else {
        // if just a strike is present
        string s = XMLUtils::getChildValue(node, "Strike", isRequired);
        if (!s.empty()) {
            strike_ = TradeMonetary(s);
            type_ = Type::Price;
            onlyStrike_ = true;
        }
    }
}

XMLNode* TradeStrike::toXML(XMLDocument& doc) const {
    XMLNode* node;
    if (onlyStrike_) {
        // can only happen for a StrikePrice
        auto sp = QuantLib::ext::get<StrikePrice>(strike_);
        node = doc.allocNode("Strike", boost::lexical_cast<std::string>(sp.valueString()));
    } else {
        node = doc.allocNode("StrikeData");
        if (noStrikePriceNode_) {
            // maintain backward compatibility, must be a Strike Type Price to get here
            strikePrice().toXMLNode(doc, node);
        } else {
            XMLNode* subNode;
            if (type_ == Type::Yield) {
                auto yld = strikeYield();
                subNode = doc.allocNode("StrikeYield");
                XMLUtils::addChild(doc, subNode, "Yield", yld.yield);
                XMLUtils::addChild(doc, subNode, "Compounding", to_string(yld.compounding));
            } else {
                auto sp = strikePrice();
                subNode = doc.allocNode("StrikePrice");
                sp.toXMLNode(doc, subNode);
            }
            XMLUtils::appendNode(node, subNode);     
        }
    }
    return node;
}

QuantLib::Real TradeStrike::value() const { 
    return boost::apply_visitor(StrikeValue(), strike_); 
}

std::string TradeStrike::currency() {
    QL_REQUIRE(type_ == Type::Price, "TradeStrike currency only valid when Strike type is Price");
    return strikePrice().currency();
}

const QuantLib::Compounding& TradeStrike::compounding() {
    QL_REQUIRE(type_ == Type::Yield, "TradeStrike currency only valid when Strike type is Yield");
    return strikeYield().compounding;
}

void TradeStrike::setValue(const QuantLib::Real& value) {
    if (type_ == Type::Price) {
        strikePrice().setValue(value);
    } else {
        strikeYield().yield = value;    
    }
}

void TradeStrike::setCurrency(const std::string& currency) {
    QL_REQUIRE(type_ == Type::Price, "TradeStrike currency only valid when Strike type is Price");
    strikePrice().setCurrency(currency);
}

const bool TradeStrike::empty() const {
    try {      
        return value() == Null<Real>();
    } catch (...) {
        return true;
    }
}

} // namespace data
} // namespace ore