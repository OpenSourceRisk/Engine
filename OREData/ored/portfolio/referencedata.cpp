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

        // TODO: this should be an extensible factory, for now we just hardcode the types...
        boost::shared_ptr<ReferenceDatum> refData;
            refData = boost::make_shared<BondReferenceDatum>(id);
        else if (refDataType == CreditIndexReferenceDatum::TYPE)
            refData = boost::make_shared<CreditIndexReferenceDatum>(id);
        else if (refDataType == CreditReferenceDatum::TYPE)
            refData = boost::make_shared<CreditReferenceDatum>(id);
        else if (refDataType == EquityIndexReferenceDatum::TYPE)
            refData = boost::make_shared<EquityIndexReferenceDatum>(id);
        else {
            QL_FAIL("Unknown type " << refDataType);
        }

        refData->fromXML(child);
        data_[make_pair(refDataType, id)] = refData;
    }
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