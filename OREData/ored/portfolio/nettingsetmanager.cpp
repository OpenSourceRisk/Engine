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
#include <ored/portfolio/structuredconfigurationerror.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>

namespace ore {
namespace data {

using ore::data::NettingSetDetails;

void NettingSetManager::add(const QuantLib::ext::shared_ptr<NettingSetDefinition>& nettingSet) const {
    const NettingSetDetails& k = nettingSet->nettingSetDetails();

    std::pair<NettingSetDetails, QuantLib::ext::shared_ptr<NettingSetDefinition>> newNetSetDef(k, nettingSet);

    bool added = data_.insert(newNetSetDef).second;
    if (added)
        uniqueKeys_.push_back(k);

    QL_REQUIRE(data_.size() == uniqueKeys_.size(), "NettingSetManager: vector/map size mismatch");
}

bool NettingSetManager::has(const NettingSetDetails& nettingSetDetails) const {
    return data_.find(nettingSetDetails) != data_.end() || unparsed_.find(nettingSetDetails) != unparsed_.end();
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
    const auto it = data_.find(nettingSetDetails);
    if (it != data_.end())
        return it->second;

    const auto uit = unparsed_.find(nettingSetDetails);
    if (uit == unparsed_.end())
        QL_FAIL("NettingSetDefinition not found in unparsed netting set manager: " << nettingSetDetails);

    QuantLib::ext::shared_ptr<NettingSetDefinition> nettingSet =
        QuantLib::ext::make_shared<NettingSetDefinition>();
    try {
        nettingSet->fromXMLString(uit->second);
        add(nettingSet);
    } catch (std::exception& ex) {
        string err =
            "NettingSetDefinition for id " + to_string(nettingSetDetails) + " was requested, but could not be parsed.";
        StructuredConfigurationErrorMessage("Netting set manager", nettingSetDetails.nettingSetId(), err, ex.what())
            .log();
        QL_FAIL(err);
    }
    return nettingSet;
}

QuantLib::ext::shared_ptr<NettingSetDefinition> NettingSetManager::get(const string& id) const {
    return get(NettingSetDetails(id));
}

void NettingSetManager::loadAll() {
    for (const auto& [nsd, up] : unparsed_) {
        QuantLib::ext::shared_ptr<NettingSetDefinition> nettingSet = QuantLib::ext::make_shared<NettingSetDefinition>();
        try {
            nettingSet->fromXMLString(up);
            add(nettingSet);
        } catch (std::exception& ex) {
            string err = "NettingSetDefinition for id " + to_string(nsd) +
                         " was requested, but could not be parsed.";
            StructuredConfigurationErrorMessage("Netting set manager", nsd.nettingSetId(), err, ex.what())
                .log();
            QL_FAIL(err);
        }
    }
}

void NettingSetManager::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "NettingSetDefinitions");
    vector<XMLNode*> nettingSetNodes = XMLUtils::getChildrenNodes(node, "NettingSet");
    for (unsigned i = 0; i < nettingSetNodes.size(); i++) {
        XMLNode* child = nettingSetNodes[i];
        try {
            NettingSetDetails nsd = getNettingSetDetails(child);
            unparsed_[nsd] = XMLUtils::toString(child);
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
