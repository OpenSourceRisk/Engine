/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/utilities/to_string.hpp>
#include <ored/portfolio/convertiblebondreferencedata.hpp>

namespace ore {
namespace data {

// Convertible Bond
void ConvertibleBondReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* innerNode = XMLUtils::getChildNode(node, "ConvertibleBondReferenceData");
    bondData_.fromXML(XMLUtils::getChildNode(innerNode, "BondData"));
    if (auto n = XMLUtils::getChildNode(innerNode, "CallData")) {
        if (!XMLUtils::getChildrenNodes(n, "").empty()) {
            callData_.fromXML(n);
        }
    }
    if (auto n = XMLUtils::getChildNode(innerNode, "PutData")) {
        if (!XMLUtils::getChildrenNodes(n, "").empty())
            putData_.fromXML(n);
    }
    if (auto n = XMLUtils::getChildNode(innerNode, "ConversionData")) {
        if (!XMLUtils::getChildrenNodes(n, "").empty())
            conversionData_.fromXML(n);
    }
    if (auto n = XMLUtils::getChildNode(innerNode, "DividendProtectionData")) {
        if (!XMLUtils::getChildrenNodes(n, "").empty())
            dividendProtectionData_.fromXML(n);
    }
    detachable_ = XMLUtils::getChildValue(innerNode, "Detachable", false);
}

XMLNode* ConvertibleBondReferenceDatum::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* node0 = ReferenceDatum::toXML(doc);
    XMLNode* node = XMLUtils::addChild(doc, node0, "ConvertibleBondReferenceData");
    XMLUtils::appendNode(node, bondData_.toXML(doc));
    if (callData_.initialised())
        XMLUtils::appendNode(node, callData_.toXML(doc));
    if (putData_.initialised())
        XMLUtils::appendNode(node, putData_.toXML(doc));
    if (conversionData_.initialised())
        XMLUtils::appendNode(node, conversionData_.toXML(doc));
    if (dividendProtectionData_.initialised())
        XMLUtils::appendNode(node, dividendProtectionData_.toXML(doc));
    if (!detachable_.empty())
        XMLUtils::addChild(doc, node, "Detachable");
    return node0;
}

} // namespace data
} // namespace ore
