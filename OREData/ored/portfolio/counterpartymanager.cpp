/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/portfolio/counterpartymanager.hpp>
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/utilities/log.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void CounterpartyManager::add(const QuantLib::ext::shared_ptr<CounterpartyInformation>& cp) {
    bool added =
        data_.insert(std::pair<string, QuantLib::ext::shared_ptr<CounterpartyInformation>>(cp->counterpartyId(), cp))
        .second;

    if (added)
        uniqueKeys_.push_back(cp->counterpartyId());

    QL_REQUIRE(data_.size() == uniqueKeys_.size(), "CounterpartyManager: vector/map size mismatch");
}

bool CounterpartyManager::has(string id) const { return data_.find(id) != data_.end(); }

//! adds a new CounterpartyInformation object to manager
void CounterpartyManager::addCorrelation(const std::string& cpty1, const std::string& cpty2,
                                          Real correlation) {
    if (!correlations_)
        correlations_ = QuantLib::ext::make_shared<CounterpartyCorrelationMatrix>();
    correlations_->addCorrelation(cpty1, cpty2, correlation);                                         
};


void CounterpartyManager::reset() {
    data_.clear();
    uniqueKeys_.clear();
}

const bool CounterpartyManager::empty() {
    return data_.empty();
}

QuantLib::ext::shared_ptr<CounterpartyInformation> CounterpartyManager::get(string id) const {
    if (has(id))
        return data_.find(id)->second;
    else
        QL_FAIL("CounterpartyInformation not found in manager: " << id);
}

void CounterpartyManager::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CounterpartyInformation");
    
    XMLNode* cptyNode = XMLUtils::getChildNode(node, "Counterparties");
    vector<XMLNode*> cpNodes = XMLUtils::getChildrenNodes(cptyNode, "Counterparty");
    for (unsigned i = 0; i < cpNodes.size(); i++) {
        XMLNode* child = cpNodes[i];
        try {
            QuantLib::ext::shared_ptr<CounterpartyInformation> cp(new CounterpartyInformation(child));
            add(cp);
        }
        catch (std::exception& ex) {
            StructuredConfigurationWarningMessage("Counterparty manager", "",
                                                  "Failed to parse counterparty information", ex.what())
                .log();
        }
    }
   
    if (XMLUtils::getChildNode(node, "Correlations")) {
        XMLNode* correlationNode = XMLUtils::getChildNode(node, "Correlations");
        correlations_ = QuantLib::ext::make_shared<CounterpartyCorrelationMatrix>(correlationNode);
    } else {
        correlations_ = QuantLib::ext::make_shared<CounterpartyCorrelationMatrix>();
    }
}

XMLNode* CounterpartyManager::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("CounterpartyInformation");

    XMLNode* cptyNode = XMLUtils::addChild(doc, node, "Counterparties");
    XMLUtils::appendNode(node, cptyNode);
    // map<string, const QuantLib::ext::shared_ptr<CounterpartyInformation>>::iterator it;
    for (auto it = data_.begin(); it != data_.end(); ++it)
        XMLUtils::appendNode(cptyNode, it->second->toXML(doc));
    
    XMLNode* corrNode = XMLUtils::addChild(doc, node, "Correlations");
    XMLUtils::appendNode(node, corrNode);
    XMLUtils::appendNode(corrNode, correlations_->toXML(doc));
    
    
    return node;
}

} // data
} // ore
