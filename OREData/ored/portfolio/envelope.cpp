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

#include <ored/portfolio/envelope.hpp>

using openriskengine::data::XMLUtils;

namespace openriskengine {
namespace data {

void Envelope::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Envelope");
    counterparty_ = XMLUtils::getChildValue(node, "CounterParty", true);
    nettingSetId_ = XMLUtils::getChildValue(node, "NettingSetId", false);
    additionalFields_.clear();

    XMLNode* additionalNode = XMLUtils::getChildNode(node, "AdditionalFields");
    if (additionalNode) {
        for (XMLNode* child = XMLUtils::getChildNode(additionalNode); child; child = XMLUtils::getNextSibling(child)) {
            additionalFields_[XMLUtils::getNodeName(child)] = XMLUtils::getNodeValue(child);
        }
    }
}

XMLNode* Envelope::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Envelope");
    XMLUtils::addChild(doc, node, "CounterParty", counterparty_);
    XMLUtils::addChild(doc, node, "NettingSetId", nettingSetId_);
    XMLNode* additionalNode = doc.allocNode("AdditionalFields");
    XMLUtils::appendNode(node, additionalNode);
    for (const auto& it : additionalFields_)
        XMLUtils::addChild(doc, additionalNode, it.first, it.second);
    return node;
}
}
}
