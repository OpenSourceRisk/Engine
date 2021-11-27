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

NettingSetDetails::NettingSetDetails(const NettingSetDetails::Key& nettingSetKey) {
    QL_REQUIRE(fieldNames(true).size() == boost::tuples::length<NettingSetDetails::Key>::value,
               "NettingSetDetails: Netting set key provided does not match the number of expected fields.");

    nettingSetId_ = boost::get<0>(nettingSetKey);
    agreementType_ = boost::get<1>(nettingSetKey);
    callType_ = boost::get<2>(nettingSetKey);
    initialMarginType_ = boost::get<3>(nettingSetKey);
    legalEntityId_ = boost::get<4>(nettingSetKey);
}

NettingSetDetails::NettingSetDetails(const map<string, string>& nettingSetMap) {
    const vector<string>& supportedFields = fieldNames();
    
    for (const pair<string, string>& m : nettingSetMap) {
        if (m.first == "NettingSetId") {
            nettingSetId_ = nettingSetMap.at(m.first);
        } else if (m.first == "AgreementType") {
            agreementType_ = nettingSetMap.at(m.first);
        } else if (m.first == "CallType") {
            agreementType_ = nettingSetMap.at(m.first);
        } else if (m.first == "InitialMarginType") {
            initialMarginType_ = nettingSetMap.at(m.first);
        } else if (m.first == "LegalEntityId") {
            legalEntityId_ = nettingSetMap.at(m.first);
        } else {
            WLOG("NettingSetDetails:: Unsupported field \'" << m.second << "\'");
        }
    }
}

void Envelope::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Envelope");
    counterparty_ = XMLUtils::getChildValue(node, "CounterParty", true);

    XMLNode* nettingSetIdNode = XMLUtils::getChildNode(node, "NettingSetId");
    XMLNode* nettingSetDetailsNode = XMLUtils::getChildNode(node, "NettingSetDetails");
    QL_REQUIRE(!(nettingSetIdNode && nettingSetDetailsNode),
               "Only one of NettingSetId and NettingSetDetails should be provided.");
    if (nettingSetDetailsNode) {
        nettingSetDetails_.fromXML(nettingSetDetailsNode);
    } else {
        nettingSetDetails_ = NettingSetDetails();
    }   
    nettingSetId_ = XMLUtils::getChildValue(node, "NettingSetId", false);

    portfolioIds_.clear();
    XMLNode* portfolioNode = XMLUtils::getChildNode(node, "PortfolioIds");
    if (portfolioNode) {
        for (auto const& c : XMLUtils::getChildrenNodes(portfolioNode, "PortfolioId"))
            portfolioIds_.insert(XMLUtils::getNodeValue(c));
    }

    additionalFields_.clear();
    XMLNode* additionalNode = XMLUtils::getChildNode(node, "AdditionalFields");
    if (additionalNode) {
        for (XMLNode* child = XMLUtils::getChildNode(additionalNode); child; child = XMLUtils::getNextSibling(child)) {
            additionalFields_[XMLUtils::getNodeName(child)] = XMLUtils::getNodeValue(child);
        }
    }
}

XMLNode* Envelope::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Envelope");
    XMLUtils::addChild(doc, node, "CounterParty", counterparty_);
    if (nettingSetDetails_.empty()) {
        XMLUtils::addChild(doc, node, "NettingSetId", nettingSetId_);
    } else {
        XMLUtils::appendNode(node, nettingSetDetails_.toXML(doc));
    }
    XMLNode* portfolioNode = doc.allocNode("PortfolioIds");
    XMLUtils::appendNode(node, portfolioNode);
    for (const auto& p : portfolioIds_)
        XMLUtils::addChild(doc, portfolioNode, "PortfolioId", p);
    XMLNode* additionalNode = doc.allocNode("AdditionalFields");
    XMLUtils::appendNode(node, additionalNode);
    for (const auto& it : additionalFields_)
        XMLUtils::addChild(doc, additionalNode, it.first, it.second);
    return node;
}

void NettingSetDetails::fromXML(XMLNode* node) {
    nettingSetId_ = XMLUtils::getChildValue(node, "NettingSetId", true);
    agreementType_ = XMLUtils::getChildValue(node, "AgreementType", false);
    callType_ = XMLUtils::getChildValue(node, "CallType", false);
    initialMarginType_ = XMLUtils::getChildValue(node, "InitialMarginType", false);
    legalEntityId_ = XMLUtils::getChildValue(node, "LegalEntityId", false);
}

XMLNode* NettingSetDetails::toXML(XMLDocument& doc) {
    XMLNode* nettingSetDetailsNode = doc.allocNode("NettingSetDetails");
    XMLUtils::addChild(doc, nettingSetDetailsNode, "NettingSetId", nettingSetId_);
    XMLUtils::addChild(doc, nettingSetDetailsNode, "AgreementType", agreementType_);
    XMLUtils::addChild(doc, nettingSetDetailsNode, "CallType", callType_);
    XMLUtils::addChild(doc, nettingSetDetailsNode, "InitialMarginType", initialMarginType_);
    XMLUtils::addChild(doc, nettingSetDetailsNode, "LegalEntityId", legalEntityId_);

    return nettingSetDetailsNode;
}

const vector<string> NettingSetDetails::fieldNames(bool includeOptionalFields) {
    vector<string> fieldNames;
    if (includeOptionalFields)
        fieldNames = {"NettingSetId", "AgreementType", "CallType", "InitialMarginType", "LegalEntityId"};
    else
        fieldNames = {"NettingSetId"};

    return fieldNames;
}

const vector<string> NettingSetDetails::optionalFieldNames() {
    return vector<string>({"AgreementType", "CallType", "InitialMarginType", "LegalEntityId"});
}

const map<string, string> NettingSetDetails::mapRepresentation() const {
    map<string, string> rep;
    rep.insert({"NettingSetId", nettingSetId()});
    rep.insert({"AgreementType", agreementType()});
    rep.insert({"CallType", callType()});
    rep.insert({"InitialMarginType", initialMarginType()});
    rep.insert({"LegalEntityId", legalEntityId()});

    return rep;
}

std::ostream& operator<<(std::ostream& out, const NettingSetDetails& nettingSetDetails) {
    return out << "NettingSetId=\'" << nettingSetDetails.nettingSetId() << "\', AgreementType=\'"
               << nettingSetDetails.agreementType() << "\', CallType=\'" << nettingSetDetails.callType()
               << "\', InitialMarginType=\'" << nettingSetDetails.initialMarginType() << "\', LegalEntityId=\'"
               << nettingSetDetails.legalEntityId() << "\'";
}

bool operator==(const NettingSetDetails& lhs, const NettingSetDetails& rhs) {
    return (lhs.nettingSetId() == rhs.nettingSetId() && lhs.agreementType() == rhs.agreementType() &&
            lhs.callType() == rhs.callType() && lhs.initialMarginType() == rhs.initialMarginType() &&
            lhs.legalEntityId() == rhs.legalEntityId());
}

} // namespace data
} // namespace ore
