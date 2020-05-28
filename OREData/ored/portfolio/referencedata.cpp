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
#include <ored/portfolio/legdata.hpp>
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

ReferenceDatumRegister<ReferenceDatumBuilder<BondReferenceDatum>> BondReferenceDatum::reg_(TYPE);

void BondReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* innerNode = XMLUtils::getChildNode(node, "BondReferenceData");
    QL_REQUIRE(innerNode, "No BondReferenceData node");

    bondData_.issuerId = XMLUtils::getChildValue(innerNode, "IssuerId", true);
    bondData_.settlementDays = XMLUtils::getChildValue(innerNode, "SettlementDays", true);
    bondData_.calendar = XMLUtils::getChildValue(innerNode, "Calendar", true);
    bondData_.issueDate = XMLUtils::getChildValue(innerNode, "IssueDate", true);
    bondData_.creditCurveId = XMLUtils::getChildValue(innerNode, "CreditCurveId", false);
    bondData_.referenceCurveId = XMLUtils::getChildValue(innerNode, "ReferenceCurveId", true);
    bondData_.incomeCurveId = XMLUtils::getChildValue(innerNode, "IncomeCurveId", false);
    bondData_.volatilityCurveId = XMLUtils::getChildValue(innerNode, "VolatilityCurveId", false);

    bondData_.legData.clear();
    XMLNode* legNode = XMLUtils::getChildNode(innerNode, "LegData");
    while (legNode != nullptr) {
        LegData ld;
        ld.fromXML(legNode);
        bondData_.legData.push_back(ld);
        legNode = XMLUtils::getNextSibling(legNode, "LegData");
    }
}

XMLNode* BondReferenceDatum::toXML(XMLDocument& doc) {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* bondNode = doc.allocNode("BondReferenceData");
    XMLUtils::appendNode(node, bondNode);
    XMLUtils::addChild(doc, bondNode, "IssuerId", bondData_.issuerId);
    XMLUtils::addChild(doc, bondNode, "SettlementDays", bondData_.issuerId);
    XMLUtils::addChild(doc, bondNode, "Calendar", bondData_.issuerId);
    XMLUtils::addChild(doc, bondNode, "IssueDate", bondData_.issuerId);
    XMLUtils::addChild(doc, bondNode, "CreditCurveId", bondData_.issuerId);
    XMLUtils::addChild(doc, bondNode, "ReferenceCurveId", bondData_.issuerId);
    XMLUtils::addChild(doc, bondNode, "VolatilityCurveId", bondData_.volatilityCurveId);
    for (auto& bd : bondData_.legData)
        XMLUtils::appendNode(bondNode, bd.toXML(doc));
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

void BasicReferenceDataManager::add(const boost::shared_ptr<ReferenceDatum>& rd) {
    // Add reference datum, it is overwritten if it is already present.
    data_[make_pair(rd->type(), rd->id())] = rd;
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

boost::shared_ptr<ReferenceDatum> BasicReferenceDataManager::getData(const string& type, const string& id) {
    auto it = data_.find(make_pair(type, id));
    QL_REQUIRE(it != data_.end(), "No Reference data for " << type << " " << id);
    return it->second;
}

} // data
} // ore
