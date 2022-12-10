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
    creditCurveId = XMLUtils::getChildValue(node, "CreditCurveId", false);
    creditGroup = XMLUtils::getChildValue(node, "CreditGroup", false);
    referenceCurveId = XMLUtils::getChildValue(node, "ReferenceCurveId", true);
    incomeCurveId = XMLUtils::getChildValue(node, "IncomeCurveId", false);
    volatilityCurveId = XMLUtils::getChildValue(node, "VolatilityCurveId", false);
    settlementDays = XMLUtils::getChildValue(node, "SettlementDays", true);
    calendar = XMLUtils::getChildValue(node, "Calendar", true);
    issueDate = XMLUtils::getChildValue(node, "IssueDate", true);
    priceQuoteMethod = XMLUtils::getChildValue(node, "PriceQuoteMethod", false);
    priceQuoteBaseValue = XMLUtils::getChildValue(node, "PriceQuoteBaseValue", false);

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
    XMLUtils::addChild(doc, node, "CreditCurveId", creditCurveId);
    XMLUtils::addChild(doc, node, "CreditGroup", creditGroup);
    XMLUtils::addChild(doc, node, "ReferenceCurveId", issuerId);
    XMLUtils::addChild(doc, node, "IncomeCurveId", incomeCurveId);
    XMLUtils::addChild(doc, node, "VolatilityCurveId", volatilityCurveId);
    XMLUtils::addChild(doc, node, "SettlementDays", settlementDays);
    XMLUtils::addChild(doc, node, "Calendar", calendar);
    XMLUtils::addChild(doc, node, "IssueDate", issueDate);
    XMLUtils::addChild(doc, node, "PriceQuoteMethod", priceQuoteMethod);
    XMLUtils::addChild(doc, node, "PriceQuoteBaseValue", priceQuoteBaseValue);
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

CreditIndexConstituent::CreditIndexConstituent()
    : weight_(Null<Real>()), priorWeight_(Null<Real>()), recovery_(Null<Real>()) {}

CreditIndexConstituent::CreditIndexConstituent(const string& name, Real weight, Real priorWeight, Real recovery,
    const Date& auctionDate, const Date& auctionSettlementDate, const Date& defaultDate,
    const Date& eventDeterminationDate)
    : name_(name), weight_(weight), priorWeight_(priorWeight), recovery_(recovery), auctionDate_(auctionDate),
      auctionSettlementDate_(auctionSettlementDate), defaultDate_(defaultDate),
      eventDeterminationDate_(eventDeterminationDate) {}

void CreditIndexConstituent::fromXML(XMLNode* node) {

    name_ = XMLUtils::getChildValue(node, "Name", true);
    weight_ = XMLUtils::getChildValueAsDouble(node, "Weight", true);

    if (close(weight_, 0.0)) {
        priorWeight_ = Null<Real>();
        if (auto n = XMLUtils::getChildNode(node, "PriorWeight"))
            priorWeight_ = parseReal(XMLUtils::getNodeValue(n));

        recovery_ = Null<Real>();
        if (auto n = XMLUtils::getChildNode(node, "RecoveryRate"))
            recovery_ = parseReal(XMLUtils::getNodeValue(n));

        auctionDate_ = Date();
        if (auto n = XMLUtils::getChildNode(node, "AuctionDate"))
            auctionDate_ = parseDate(XMLUtils::getNodeValue(n));

        auctionSettlementDate_ = Date();
        if (auto n = XMLUtils::getChildNode(node, "AuctionSettlementDate"))
            auctionSettlementDate_ = parseDate(XMLUtils::getNodeValue(n));

        defaultDate_ = Date();
        if (auto n = XMLUtils::getChildNode(node, "DefaultDate"))
            defaultDate_ = parseDate(XMLUtils::getNodeValue(n));

        eventDeterminationDate_ = Date();
        if (auto n = XMLUtils::getChildNode(node, "EventDeterminationDate"))
            eventDeterminationDate_ = parseDate(XMLUtils::getNodeValue(n));
    }
}

XMLNode* CreditIndexConstituent::toXML(ore::data::XMLDocument& doc) {

    XMLNode* node = doc.allocNode("Underlying");

    XMLUtils::addChild(doc, node, "Name", name_);
    XMLUtils::addChild(doc, node, "Weight", weight_);

    if (close(weight_, 0.0)) {
        if (priorWeight_ != Null<Real>())
            XMLUtils::addChild(doc, node, "PriorWeight", priorWeight_);

        if (recovery_ != Null<Real>())
            XMLUtils::addChild(doc, node, "RecoveryRate", recovery_);

        if (auctionDate_ != Date())
            XMLUtils::addChild(doc, node, "AuctionDate", to_string(auctionDate_));

        if (auctionSettlementDate_ != Date())
            XMLUtils::addChild(doc, node, "AuctionSettlementDate", to_string(auctionSettlementDate_));

        if (defaultDate_ != Date())
            XMLUtils::addChild(doc, node, "DefaultDate", to_string(defaultDate_));

        if (eventDeterminationDate_ != Date())
            XMLUtils::addChild(doc, node, "EventDeterminationDate", to_string(eventDeterminationDate_));
    }

    return node;
}

const string& CreditIndexConstituent::name() const { return name_; }

Real CreditIndexConstituent::weight() const { return weight_; }

Real CreditIndexConstituent::priorWeight() const { return priorWeight_; }

Real CreditIndexConstituent::recovery() const { return recovery_; }

const Date& CreditIndexConstituent::auctionDate() const { return auctionDate_; }

const Date& CreditIndexConstituent::auctionSettlementDate() const { return auctionSettlementDate_; }

const Date& CreditIndexConstituent::defaultDate() const { return defaultDate_; }

const Date& CreditIndexConstituent::eventDeterminationDate() const { return eventDeterminationDate_; }

bool operator<(const CreditIndexConstituent& lhs, const CreditIndexConstituent& rhs) { return lhs.name() < rhs.name(); }

ReferenceDatumRegister<ReferenceDatumBuilder<CreditIndexReferenceDatum>> CreditIndexReferenceDatum::reg_(TYPE);

CreditIndexReferenceDatum::CreditIndexReferenceDatum() {}

CreditIndexReferenceDatum::CreditIndexReferenceDatum(const string& name) : ReferenceDatum(TYPE, name) {}

void CreditIndexReferenceDatum::fromXML(XMLNode* node) {

    ReferenceDatum::fromXML(node);

    XMLNode* cird = XMLUtils::getChildNode(node, "CreditIndexReferenceData");
    QL_REQUIRE(cird, "Expected a CreditIndexReferenceData node.");

    constituents_.clear();

    for (XMLNode* child = XMLUtils::getChildNode(cird, "Underlying"); child;
         child = XMLUtils::getNextSibling(child, "Underlying")) {
        CreditIndexConstituent c;
        c.fromXML(child);
        add(c);
    }
}

XMLNode* CreditIndexReferenceDatum::toXML(ore::data::XMLDocument& doc) {

    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* cird = XMLUtils::addChild(doc, node, "CreditIndexReferenceData");

    for (auto c : constituents_) {
        auto cNode = c.toXML(doc);
        XMLUtils::appendNode(cird, cNode);
    }

    return node;
}

void CreditIndexReferenceDatum::add(const CreditIndexConstituent& c) {
    auto it = constituents_.find(c);
    if (it != constituents_.end()) {
        DLOG("Constituent " << c.name() << " not added to credit index " << id() << " because already present.");
    } else {
        constituents_.insert(c);
        DLOG("Constituent " << c.name() << " added to credit index " << id() << ".");
    }
}

const set<CreditIndexConstituent>& CreditIndexReferenceDatum::constituents() const { return constituents_; }

// BasicReferenceDataManager

void BasicReferenceDataManager::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ReferenceData");
    for (XMLNode* child = XMLUtils::getChildNode(node, "ReferenceDatum"); child;
         child = XMLUtils::getNextSibling(child, "ReferenceDatum")) {
	    addFromXMLNode(child);
    }
}

void BasicReferenceDataManager::add(const boost::shared_ptr<ReferenceDatum>& rd) {
    // Add reference datum, it is overwritten if it is already present.
    data_[make_pair(rd->type(), rd->id())] = rd;
}

boost::shared_ptr<ReferenceDatum> BasicReferenceDataManager::addFromXMLNode(XMLNode* node, const std::string& inputId) {
    string refDataType = XMLUtils::getChildValue(node, "Type", false);
    boost::shared_ptr<ReferenceDatum> refData;

    if (refDataType.empty()) {
        ALOG("Found referenceDatum without Type - skipping");
        return refData;
    }

    string id = inputId.empty() ? XMLUtils::getAttribute(node, "id") : inputId;

    if (id.empty()) {
        ALOG("Found referenceDatum without id - skipping");
        return refData;
    }

    if (data_.find(make_pair(refDataType, id)) != data_.end()) {
        duplicates_.insert(make_pair(refDataType, id));
        ALOG("Found duplicate referenceDatum for type='" << refDataType << "', id='" << id << "'");
        return refData;
    }

    try {
        refData = buildReferenceDatum(refDataType);
        refData->fromXML(node);
        // set the type and id at top level (is this needed?)
        refData->setType(refDataType);
        refData->setId(id);
        data_[make_pair(refDataType, id)] = refData;
        TLOG("added referenceDatum for type='" << refDataType << "', id='" << id << "'");
    } catch (const std::exception& e) {
        buildErrors_[make_pair(refDataType, id)] = e.what();
        ALOG("Error building referenceDatum for type='" << refDataType << "', id='" << id << "': " << e.what());
    }

    return refData;
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

void BasicReferenceDataManager::check(const string& type, const string& id) const {
    auto key = make_pair(type, id);
    if (duplicates_.find(key) != duplicates_.end())
        ALOG("BasicReferenceDataManager: duplicate entries for type='" << type << "', id='" << id << "'");
    auto err = buildErrors_.find(key);
    if (err != buildErrors_.end())
        ALOG("BasicReferenceDataManager: Build error for type='" << type << "', id='" << id << "': " << err->second);
}

bool BasicReferenceDataManager::hasData(const string& type, const string& id) const {
    check(type, id);
    return data_.find(make_pair(type, id)) != data_.end();
}

boost::shared_ptr<ReferenceDatum> BasicReferenceDataManager::getData(const string& type, const string& id) {
    check(type, id);
    auto it = data_.find(make_pair(type, id));
    QL_REQUIRE(it != data_.end(),
               "BasicReferenceDataManager::getData(): No Reference data for type='" << type << "', id='" << id << "'");
    return it->second;
}

} // namespace data
} // namespace ore
