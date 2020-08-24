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

#include <ored/portfolio/legdata.hpp>
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
    XMLUtils::addChild(doc, node, "Type", type_);
    return node;
}

ReferenceDatumRegister<ReferenceDatumBuilder<BondReferenceDatum>> BondReferenceDatum::reg_(TYPE);

void BondReferenceDatum::BondData::fromXML(XMLNode* node) {
    QL_REQUIRE(node, "BondReferenceDatum::BondData::fromXML(): no node given");
    issuerId = XMLUtils::getChildValue(node, "IssuerId", true);
    settlementDays = XMLUtils::getChildValue(node, "SettlementDays", true);
    calendar = XMLUtils::getChildValue(node, "Calendar", true);
    issueDate = XMLUtils::getChildValue(node, "IssueDate", true);
    creditCurveId = XMLUtils::getChildValue(node, "CreditCurveId", false);
    referenceCurveId = XMLUtils::getChildValue(node, "ReferenceCurveId", true);
    proxySecurityId = XMLUtils::getChildValue(node, "ProxySecurityId", false);
    incomeCurveId = XMLUtils::getChildValue(node, "IncomeCurveId", false);
    volatilityCurveId = XMLUtils::getChildValue(node, "VolatilityCurveId", false);

    legData.clear();
    XMLNode* legNode = XMLUtils::getChildNode(node, "LegData");
    while (legNode != nullptr) {
        LegData ld;
        ld.fromXML(legNode);
        legData.push_back(ld);
        legNode = XMLUtils::getNextSibling(legNode, "LegData");
    }
}

XMLNode* BondReferenceDatum::BondData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("BondData");
    XMLUtils::addChild(doc, node, "IssuerId", issuerId);
    XMLUtils::addChild(doc, node, "SettlementDays", issuerId);
    XMLUtils::addChild(doc, node, "Calendar", issuerId);
    XMLUtils::addChild(doc, node, "IssueDate", issuerId);
    XMLUtils::addChild(doc, node, "CreditCurveId", issuerId);
    XMLUtils::addChild(doc, node, "ReferenceCurveId", issuerId);
    XMLUtils::addChild(doc, node, "ProxySecurityId", proxySecurityId);
    XMLUtils::addChild(doc, node, "VolatilityCurveId", volatilityCurveId);
    for (auto& bd : legData)
        XMLUtils::appendNode(node, bd.toXML(doc));
    return node;
}

void BondReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    bondData_.fromXML(XMLUtils::getChildNode(node, "BondReferenceData"));
}

XMLNode* BondReferenceDatum::toXML(XMLDocument& doc) {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* dataNode = bondData_.toXML(doc);
    XMLUtils::setNodeName(doc, dataNode, "BondReferenceData");
    XMLUtils::appendNode(node, dataNode);
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

        QL_REQUIRE(!hasData(refDataType, id),
                   "BasicReferenceDataManager already has " << refDataType << " with id " << id);

        boost::shared_ptr<ReferenceDatum> refData = buildReferenceDatum(refDataType);
        refData->fromXML(child);
        // set the type and id at top level (is this needed?)
        refData->setType(refDataType);
        refData->setId(id);
        data_[make_pair(refDataType, id)] = refData;
    }
}

void BasicReferenceDataManager::add(const boost::shared_ptr<ReferenceDatum>& rd) {
    // Add reference datum, it is overwritten if it is already present.
    data_[make_pair(rd->type(), rd->id())] = rd;
}

boost::shared_ptr<ReferenceDatum> BasicReferenceDataManager::buildReferenceDatum(const string& refDataType) {
    auto refData = ReferenceDatumFactory::instance().build(refDataType);
    QL_REQUIRE(refData,
               "Reference data type " << refDataType << " has not been registered with the reference data factory.");
    return refData;
}

XMLNode* BasicReferenceDataManager::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("ReferenceData");
    for (const auto& kv : data_) {
        XMLUtils::appendNode(node, kv.second->toXML(doc));
    }
    return node;
}

bool BasicReferenceDataManager::hasData(const string& type, const string& id) const {
    return data_.find(make_pair(type, id)) != data_.end();
}

boost::shared_ptr<ReferenceDatum> BasicReferenceDataManager::getData(const string& type, const string& id) {
    auto it = data_.find(make_pair(type, id));
    QL_REQUIRE(it != data_.end(), "No Reference data for " << type << " " << id);
    return it->second;
}

} // namespace data
} // namespace ore
