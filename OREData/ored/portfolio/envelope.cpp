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

#include <ored/portfolio/envelope.hpp>
#include <ored/utilities/log.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

void Envelope::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Envelope");
    counterparty_ = XMLUtils::getChildValue(node, "CounterParty", true);

    XMLNode* nettingSetDetailsNode = XMLUtils::getChildNode(node, "NettingSetDetails");
    if (nettingSetDetailsNode) {
        nettingSetDetails_.fromXML(nettingSetDetailsNode);
    } else {
        string nettingSetId = XMLUtils::getChildValue(node, "NettingSetId", false);
        nettingSetDetails_ = NettingSetDetails(nettingSetId);
    }   

    portfolioIds_.clear();
    XMLNode* portfolioNode = XMLUtils::getChildNode(node, "PortfolioIds");
    if (portfolioNode) {
        for (auto const& c : XMLUtils::getChildrenNodes(portfolioNode, "PortfolioId"))
            portfolioIds_.insert(XMLUtils::getNodeValue(c));
    }

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
    if (nettingSetDetails_.emptyOptionalFields()) {
        XMLUtils::addChild(doc, node, "NettingSetId", nettingSetDetails_.nettingSetId());
    } else {
        XMLUtils::appendNode(node, nettingSetDetails_.toXML(doc));
    }
    XMLNode* portfolioNode = doc.allocNode("PortfolioIds");
    XMLUtils::appendNode(node, portfolioNode);
    for (const auto& p : portfolioIds_)
        XMLUtils::addChild(doc, portfolioNode, "PortfolioId", p);
    XMLNode* additionalNode = doc.allocNode("AdditionalFields");
    XMLUtils::appendNode(node, additionalNode);
    for (const auto& it : additionalFields_)
        XMLUtils::addChild(doc, additionalNode, it.first, it.second);
    return node;
}

} // namespace data
} // namespace ore
