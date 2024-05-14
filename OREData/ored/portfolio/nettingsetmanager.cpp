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
#include <ored/portfolio/structuredconfigurationwarning.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

using ore::data::NettingSetDetails;

void NettingSetManager::add(const QuantLib::ext::shared_ptr<NettingSetDefinition>& nettingSet) {
    const NettingSetDetails& k = nettingSet->nettingSetDetails();

    std::pair<NettingSetDetails, QuantLib::ext::shared_ptr<NettingSetDefinition>> newNetSetDef(k, nettingSet);

    bool added = data_.insert(newNetSetDef).second;
    if (added)
        uniqueKeys_.push_back(k);

    QL_REQUIRE(data_.size() == uniqueKeys_.size(), "NettingSetManager: vector/map size mismatch");
}

bool NettingSetManager::has(const NettingSetDetails& nettingSetDetails) const {
    return data_.find(nettingSetDetails) != data_.end();
}

bool NettingSetManager::has(const string& id) const {
    return has(NettingSetDetails(id));
}

void NettingSetManager::reset() {
    data_.clear();
    uniqueKeys_.clear();
}

const bool NettingSetManager::empty() const {
    return data_.empty();
}

const bool NettingSetManager::calculateIMAmount() const { 
    for (const auto& nsd : data_) {
        if (nsd.second->activeCsaFlag() && nsd.second->csaDetails()->calculateIMAmount())
            return true;
    }
    return false;
}

const set<NettingSetDetails> NettingSetManager::calculateIMNettingSets() const {
    set<NettingSetDetails> calculateIMNettingSets = set<NettingSetDetails>();
    for (const auto& nsd : data_) {
        if (nsd.second->activeCsaFlag() && nsd.second->csaDetails()->calculateIMAmount()) {
            calculateIMNettingSets.insert(nsd.first);
        }
    }
    return calculateIMNettingSets;
}

QuantLib::ext::shared_ptr<NettingSetDefinition> NettingSetManager::get(const NettingSetDetails& nettingSetDetails) const {
    if (has(nettingSetDetails))
        return data_.find(nettingSetDetails)->second;
    else
        QL_FAIL("NettingSetDefinition not found in manager: " << nettingSetDetails);
}

QuantLib::ext::shared_ptr<NettingSetDefinition> NettingSetManager::get(const string& id) const {
    auto found = std::find_if(data_.begin(), data_.end(),
                              [&id](const auto& details) { return details.first.nettingSetId() == id; });
    if (found != data_.end())
        return found->second;
    else
        QL_FAIL("NettingSetDefinition not found in manager: " + id);
}

void NettingSetManager::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "NettingSetDefinitions");
    vector<XMLNode*> nettingSetNodes = XMLUtils::getChildrenNodes(node, "NettingSet");
    for (unsigned i = 0; i < nettingSetNodes.size(); i++) {
        XMLNode* child = nettingSetNodes[i];
        try {
            QuantLib::ext::shared_ptr<NettingSetDefinition> nettingSet(new NettingSetDefinition(child));
            add(nettingSet);
        } catch (std::exception& ex) {
            StructuredConfigurationWarningMessage("Netting set manager", "", "Failed to parse netting set definition",
                                                  ex.what())
                .log();
        }
    }
}

XMLNode* NettingSetManager::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("NettingSetDefinitions");
    // map<NettingSetDetails, const QuantLib::ext::shared_ptr<NettingSetDefinition>>::iterator it;
    for (auto it = data_.begin(); it != data_.end(); ++it)
        XMLUtils::appendNode(node, it->second->toXML(doc));
    return node;
}
} // namespace data
} // namespace ore
