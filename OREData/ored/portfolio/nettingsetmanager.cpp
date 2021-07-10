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

#include <ored/portfolio/nettingsetmanager.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

void NettingSetManager::add(const boost::shared_ptr<NettingSetDefinition>& nettingSet) {

    bool added =
        data_.insert(std::pair<string, boost::shared_ptr<NettingSetDefinition>>(nettingSet->nettingSetId(), nettingSet))
            .second;

    if (added)
        uniqueKeys_.push_back(nettingSet->nettingSetId());

    QL_REQUIRE(data_.size() == uniqueKeys_.size(), "NettingSetManager: vector/map size mismatch");
}

bool NettingSetManager::has(string id) const { return data_.find(id) != data_.end(); }

void NettingSetManager::reset() {
    data_.clear();
    uniqueKeys_.clear();
}

boost::shared_ptr<NettingSetDefinition> NettingSetManager::get(string id) const {
    if (has(id))
        return data_.find(id)->second;
    else
        QL_FAIL("NettingSetDefinition not found in manager: " << id);
}

void NettingSetManager::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "NettingSetDefinitions");
    vector<XMLNode*> nettingSetNodes = XMLUtils::getChildrenNodes(node, "NettingSet");
    for (unsigned i = 0; i < nettingSetNodes.size(); i++) {
        XMLNode* child = nettingSetNodes[i];
        try {
            boost::shared_ptr<NettingSetDefinition> nettingSet(new NettingSetDefinition(child));
            add(nettingSet);
        } catch (std::exception& ex) {
            ALOG("Exception parsing netting set definition: " << ex.what());
        }
    }
}

XMLNode* NettingSetManager::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("NettingSetDefinitions");
    map<string, const boost::shared_ptr<NettingSetDefinition>>::iterator it;
    for (it = data_.begin(); it != data_.end(); ++it)
        XMLUtils::appendNode(node, it->second->toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
