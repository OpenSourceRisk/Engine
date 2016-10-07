/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include <ored/portfolio/trade.hpp>

using openriskengine::data::XMLUtils;

namespace openriskengine {
namespace data {

void Trade::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Trade");
    tradeType_ = XMLUtils::getChildValue(node, "TradeType", true);
    envelope_.fromXML(XMLUtils::getChildNode(node, "Envelope"));
    tradeActions_.clear();
    XMLNode* taNode = XMLUtils::getChildNode(node, "TradeActions");
    if (taNode)
        tradeActions_.fromXML(taNode);
}

XMLNode* Trade::toXML(XMLDocument& doc) {
    // Crete Trade Node with Id attribute.
    XMLNode* node = doc.allocNode("Trade");
    QL_REQUIRE(node, "Failed to create trade node");
    XMLUtils::addAttribute(doc, node, "id", id_);
    XMLUtils::addChild(doc, node, "TradeType", tradeType_);
    XMLUtils::appendNode(node, envelope_.toXML(doc));
    if (!tradeActions_.empty())
        XMLUtils::appendNode(node, tradeActions_.toXML(doc));
    return node;
}
}
}
