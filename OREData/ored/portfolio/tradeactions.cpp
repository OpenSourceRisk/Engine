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

#include <ored/portfolio/tradeactions.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

void TradeAction::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "TradeAction");
    type_ = XMLUtils::getChildValue(node, "Type", true);
    owner_ = XMLUtils::getChildValue(node, "Owner", true);
    schedule_.fromXML(XMLUtils::getChildNode(node, "Schedule"));
}

XMLNode* TradeAction::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("TradeAction");
    QL_REQUIRE(node, "Failed to create trade node");
    XMLUtils::addChild(doc, node, "Type", type_);
    XMLUtils::addChild(doc, node, "Owner", owner_);
    XMLUtils::appendNode(node, schedule_.toXML(doc));
    return node;
}

void TradeActions::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "TradeActions");
    for (auto& action : XMLUtils::getChildrenNodes(node, "TradeAction")) {
        actions_.emplace_back();
        actions_.back().fromXML(action);
    }
}

XMLNode* TradeActions::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("TradeActions");
    for (auto& action : actions_)
        XMLUtils::appendNode(node, action.toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
