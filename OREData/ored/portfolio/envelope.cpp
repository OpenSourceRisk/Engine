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
    counterparty_ = XMLUtils::getChildValue(node, "CounterParty", false);

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

    std::function<boost::any(XMLNode*)> getValue;
    getValue = [&getValue](XMLNode* node) {
        boost::any value;
        vector<XMLNode*> children = XMLUtils::getChildrenNodes(node, "");
        // If node is a single-value node
        if (children.size() == 1 && XMLUtils::getNodeName(children[0]) == "") {
            value = XMLUtils::getNodeValue(node);
        } else {
            std::multimap<string, boost::any> subAddFields;
            for (XMLNode* child : children) {
                const string& name = XMLUtils::getNodeName(child);
                boost::any childValue = getValue(child);
                subAddFields.insert({name, childValue});
            }
            value = subAddFields;
        }
        return value;
    };

    additionalFields_.clear();
    XMLNode* additionalNode = XMLUtils::getChildNode(node, "AdditionalFields");
    if (additionalNode) {
        for (XMLNode* child = XMLUtils::getChildNode(additionalNode); child; child = XMLUtils::getNextSibling(child)) {
            additionalFields_[XMLUtils::getNodeName(child)] = getValue(child);
        }
    }
    initialized_ = true;
}

XMLNode* Envelope::toXML(XMLDocument& doc) const {
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

    std::function<void(XMLNode*, const string&, const boost::any&)> addChild;
    addChild = [&addChild, &doc](XMLNode* node, const string& name, const boost::any& val) {
        if (val.type() == typeid(string)) {
            XMLUtils::addChild(doc, node, name, boost::any_cast<string>(val));
        } else {
            QL_REQUIRE(val.type() == typeid(std::multimap<string, boost::any>),
                       "Additional field type must be either string or map<string, boost::any>");
            XMLNode* childNode = doc.allocNode(name);
            XMLUtils::appendNode(node, childNode);
            for (const auto& kv : boost::any_cast<std::multimap<string, boost::any>>(val)) {
                addChild(childNode, kv.first, kv.second);
            }
        }
    };

    for (const auto& it : additionalFields_)
        addChild(additionalNode, it.first, it.second);
    return node;
}

const map<string, string> Envelope::additionalFields() const {
    map<string, string> stringAddFields;
    for (const auto& f : additionalFields_)
        if (f.second.type() == typeid(string))
            stringAddFields[f.first] = boost::any_cast<string>(f.second);
    return stringAddFields;
}

string Envelope::additionalField(const std::string& name, const bool mandatory, const std::string& defaultValue) const {
    auto af = additionalFields();
    auto it = af.find(name);
    QL_REQUIRE(it != af.end() || !mandatory,
               "Envelope::additionalField(): Mandatory field '" << name << "' not found.");
    return it == af.end() ? defaultValue : it->second;
}

boost::any Envelope::additionalAnyField(const std::string& name, const bool mandatory, const boost::any& defaultValue) const {
    auto it = additionalFields_.find(name);
    QL_REQUIRE(it != additionalFields_.end() || !mandatory,
               "Envelope::additionalField(): Mandatory field '" << name << "' not found.");
    return it == additionalFields_.end() ? defaultValue : it->second;
}

void Envelope::setAdditionalField(const std::string& key, const boost::any& value) {
    additionalFields_[key] = value;
}


} // namespace data
} // namespace ore
