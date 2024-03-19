/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <ored/portfolio/nettingsetdetails.hpp>
#include <ored/utilities/log.hpp>

using ore::data::XMLUtils;

namespace ore {
namespace data {

NettingSetDetails::NettingSetDetails(const map<string, string>& nettingSetMap) {
    for (const auto& m : nettingSetMap) {
        if (m.first == "NettingSetId") {
            nettingSetId_ = nettingSetMap.at(m.first);
        } else if (m.first == "AgreementType") {
            agreementType_ = nettingSetMap.at(m.first);
        } else if (m.first == "CallType") {
            callType_ = nettingSetMap.at(m.first);
        } else if (m.first == "InitialMarginType") {
            initialMarginType_ = nettingSetMap.at(m.first);
        } else if (m.first == "LegalEntityId") {
            legalEntityId_ = nettingSetMap.at(m.first);
        } else {
            WLOG("NettingSetDetails:: Unsupported field \'" << m.second << "\'");
        }
    }
}

void NettingSetDetails::fromXML(XMLNode* node) {
    nettingSetId_ = XMLUtils::getChildValue(node, "NettingSetId", true);
    agreementType_ = XMLUtils::getChildValue(node, "AgreementType", false);
    callType_ = XMLUtils::getChildValue(node, "CallType", false);
    initialMarginType_ = XMLUtils::getChildValue(node, "InitialMarginType", false);
    legalEntityId_ = XMLUtils::getChildValue(node, "LegalEntityId", false);
}

XMLNode* NettingSetDetails::toXML(XMLDocument& doc) const {
    XMLNode* nettingSetDetailsNode = doc.allocNode("NettingSetDetails");
    XMLUtils::addChild(doc, nettingSetDetailsNode, "NettingSetId", nettingSetId_);
    if (!agreementType_.empty())
        XMLUtils::addChild(doc, nettingSetDetailsNode, "AgreementType", agreementType_);
    if (!callType_.empty())
        XMLUtils::addChild(doc, nettingSetDetailsNode, "CallType", callType_);
    if (!initialMarginType_.empty())
        XMLUtils::addChild(doc, nettingSetDetailsNode, "InitialMarginType", initialMarginType_);
    if (!legalEntityId_.empty())
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

bool operator<(const NettingSetDetails& lhs, const NettingSetDetails& rhs) {
    return std::tie(lhs.nettingSetId(), lhs.agreementType(), lhs.callType(), lhs.initialMarginType(),
                    lhs.legalEntityId()) < std::tie(rhs.nettingSetId(), rhs.agreementType(), rhs.callType(),
                                                    rhs.initialMarginType(), rhs.legalEntityId());
}

bool operator==(const NettingSetDetails& lhs, const NettingSetDetails& rhs) {
    return (lhs.nettingSetId() == rhs.nettingSetId() && lhs.agreementType() == rhs.agreementType() &&
            lhs.callType() == rhs.callType() && lhs.initialMarginType() == rhs.initialMarginType() &&
            lhs.legalEntityId() == rhs.legalEntityId());
}

bool operator!=(const NettingSetDetails& lhs, const NettingSetDetails& rhs) { return !(lhs == rhs); }

std::ostream& operator<<(std::ostream& out, const NettingSetDetails& nettingSetDetails) {
    std::ostream& tmp = out << "NettingSetId=\'" << nettingSetDetails.nettingSetId() << "\'";

    if (!nettingSetDetails.emptyOptionalFields()) {
        return tmp << ", AgreementType=\'" << nettingSetDetails.agreementType() << "\', CallType=\'"
                   << nettingSetDetails.callType() << "\', InitialMarginType=\'"
                   << nettingSetDetails.initialMarginType() << "\', LegalEntityId=\'"
                   << nettingSetDetails.legalEntityId() << "\'";
    }

    return tmp;
}

} // namespace data
} // namespace ore
