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

#include <ored/portfolio/referencedata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using std::function;

namespace ore {
namespace data {

void ReferenceDatum::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ReferenceDatum");
    type_ = XMLUtils::getChildValue(node, "Type", true);
    id_ = XMLUtils::getAttribute(node, "id");
}

XMLNode* ReferenceDatum::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("ReferenceDatum");
    QL_REQUIRE(node, "Failed to create ReferenceDatum node");
    XMLUtils::addAttribute(doc, node, "id", id_);
    return node;
}

ReferenceDatumRegister<ReferenceDatumBuilder<EquityReferenceDatum>> EquityReferenceDatum::reg_(TYPE);

void EquityReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* innerNode = XMLUtils::getChildNode(node, "EquityReferenceData");
    QL_REQUIRE(innerNode, "No EquityReferenceData node");
    equityData_.equityId = XMLUtils::getChildValue(innerNode, "EquityId", true);
    equityData_.equityName = XMLUtils::getChildValue(innerNode, "EquityName", false);
    equityData_.currency = XMLUtils::getChildValue(innerNode, "Currency", true);
    equityData_.scalingFactor = XMLUtils::getChildValueAsInt(innerNode, "ScalingFactor", false);
    equityData_.preferredISIN = XMLUtils::getChildValue(innerNode, "PreferredISIN", false);
    equityData_.exchangeCode = XMLUtils::getChildValue(innerNode, "ExchangeCode", false);
    equityData_.isIndex = XMLUtils::getChildValueAsBool(innerNode, "IsIndex", false);
    string startDateStr = XMLUtils::getChildValue(innerNode, "EquityStartDate", false);
    equityData_.equityStartDate = parseDate(startDateStr);
    equityData_.proxyIdentifier = XMLUtils::getChildValue(innerNode, "ProxyIdentifier", false);
}

XMLNode* EquityReferenceDatum::toXML(XMLDocument& doc) {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* equityNode = doc.allocNode("EquityReferenceData");
    XMLUtils::appendNode(node, equityNode);
    XMLUtils::addChild(doc, equityNode, "EquityId", equityData_.equityId);
    XMLUtils::addChild(doc, equityNode, "EquityName", equityData_.equityName);
    XMLUtils::addChild(doc, equityNode, "Currency", equityData_.currency);
    XMLUtils::addChild(doc, equityNode, "ScalingFactor", static_cast<int>(equityData_.scalingFactor));
    XMLUtils::addChild(doc, equityNode, "PreferredISIN", equityData_.preferredISIN);
    XMLUtils::addChild(doc, equityNode, "ExchangeCode", equityData_.exchangeCode);
    XMLUtils::addChild(doc, equityNode, "IsIndex", equityData_.isIndex);
    XMLUtils::addChild(doc, equityNode, "EquityStartDate", to_string(equityData_.equityStartDate));
    XMLUtils::addChild(doc, equityNode, "ProxyIdentifier", equityData_.proxyIdentifier);
    return node;
}


// BasicReferenceDataManager

void BasicReferenceDataManager::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ReferenceData");

    for (XMLNode* child = XMLUtils::getChildNode(node, "ReferenceDatum"); child;
        child = XMLUtils::getNextSibling(child, "ReferenceDatum")) {
        // get type and id
        string refDataType = XMLUtils::getChildValue(child, "Type", true);
        string id = XMLUtils::getAttribute(child, "id");
        QL_REQUIRE(id != "", "ReferenceDatum has no id");

        QL_REQUIRE(!hasData(refDataType, id), "BasicReferenceDataManager already has " << refDataType << " with id " << id);

        boost::shared_ptr<ReferenceDatum> refData = buildReferenceDatum(refDataType);        
        refData->fromXML(child);
        // set the type and id at top level (is this needed?)
        refData->setType(refDataType);
        refData->setId(id);
        data_[make_pair(refDataType, id)] = refData;
    }
}

boost::shared_ptr<ReferenceDatum> BasicReferenceDataManager::buildReferenceDatum(const string& refDataType) {
    auto refData = ReferenceDatumFactory::instance().build(refDataType);
    QL_REQUIRE(refData, "Reference data type " << refDataType << " has not been registered with the reference data factory.");
    return refData;
}

XMLNode* BasicReferenceDataManager::toXML(XMLDocument& doc) {
    // TODO
    return NULL;
}

bool BasicReferenceDataManager::hasData(const string& type, const string& id) const {
    return data_.find(make_pair(type, id)) != data_.end();
}

boost::shared_ptr<ReferenceDatum> BasicReferenceDataManager::getData(const string& type, const string& id) const {
    auto it = data_.find(make_pair(type, id));
    QL_REQUIRE(it != data_.end(), "No Reference data for " << type << " " << id);
    return it->second;
}

} // data
} // ore
