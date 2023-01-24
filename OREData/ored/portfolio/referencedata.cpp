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

ReferenceDatumRegister<ReferenceDatumBuilder<EquityIndexReferenceDatum>> EquityIndexReferenceDatum::reg_(TYPE);

// Index
void IndexReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* innerNode = XMLUtils::getChildNode(node, type() + "ReferenceData");
    QL_REQUIRE(innerNode, "No " + type() + "ReferenceData node");

    // clear map
    data_.clear();

    // and populate it...
    for (XMLNode* child = XMLUtils::getChildNode(innerNode, "Underlying"); child;
         child = XMLUtils::getNextSibling(child, "Underlying")) {
        string name = XMLUtils::getChildValue(child, "Name", true);
        double weight = XMLUtils::getChildValueAsDouble(child, "Weight", true);
        addUnderlying(name, weight);
    }
}

XMLNode* IndexReferenceDatum::toXML(XMLDocument& doc) {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* rdNode = XMLUtils::addChild(doc, node, type() + "ReferenceData");

    for (auto d : data_) {
        XMLNode* underlyingNode = XMLUtils::addChild(doc, rdNode, "Underlying");
        XMLUtils::addChild(doc, underlyingNode, "Name", d.first);
        XMLUtils::addChild(doc, underlyingNode, "Weight", d.second);
    }

    return node;
}
// Currency Hedged Equity Index

ReferenceDatumRegister<ReferenceDatumBuilder<CurrencyHedgedEquityIndexReferenceDatum>>
    CurrencyHedgedEquityIndexReferenceDatum::reg_(TYPE);

void CurrencyHedgedEquityIndexReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* innerNode = XMLUtils::getChildNode(node, type() + "ReferenceData");
    QL_REQUIRE(innerNode, "No " + type() + "ReferenceData node");

    underlyingIndexName_ = XMLUtils::getChildValue(innerNode, "UnderlyingIndex", true);
    quoteCurrency_ = XMLUtils::getChildValue(innerNode, "HedgeCurrency", true);
    rebalancingFrequency_ = XMLUtils::getChildValueAsPeriod(innerNode, "RebalanceFrequency", true);
    rebalancingBusinessDayOfMonth_ = XMLUtils::getChildValueAsInt(innerNode, "RebalanceBusinessDayOfMonth", true);
    
    std::string hedgeCalendarStr = XMLUtils::getChildValue(innerNode, "HedgingCalendar", true);
    hedgeCalendar_ = parseCalendar(hedgeCalendarStr);

    std::string hedgeBdcStr = XMLUtils::getChildValue(innerNode, "HedgingBusinessDayConvention", true);
    hedgeBdc_ = parseBusinessDayConvention(hedgeBdcStr);

    // Optional Fields
    referenceDateBusinessDaysOffset_ = XMLUtils::getChildValueAsInt(innerNode, "ReferenceBusinessDaysOffset", false, 0);
    hedgeAdjustmentFrequency_ = XMLUtils::getChildValueAsPeriod(innerNode, "HedgeAdjustmentFrequency", false, 0 * Days);
    // clear map
    data_.clear();

    // and populate it...
    XMLNode* currencyWeightNode = XMLUtils::getChildNode(innerNode, "CurrencyWeights");
    if (currencyWeightNode) {
        double totalWeight = 0.0;
        for (XMLNode* child = XMLUtils::getChildNode(currencyWeightNode, "Currency"); child;
             child = XMLUtils::getNextSibling(child, "Currency")) {
            string name = XMLUtils::getChildValue(child, "Name", true);
            double weight = XMLUtils::getChildValueAsDouble(child, "Weight", true);
            QL_REQUIRE(weight > 0 || QuantLib::close_enough(weight, 0.0),
                       "Try to add negtive weight for currency" << name);
            data_.push_back(make_pair(name, weight));
            totalWeight += weight;
        }
        QL_REQUIRE(data_.empty() || QuantLib::close_enough(totalWeight, 1.0),
                   "Sum of currency weights (" << totalWeight << ") is not equal 1.0");
    }
}

XMLNode* CurrencyHedgedEquityIndexReferenceDatum::toXML(XMLDocument& doc) {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* rdNode = XMLUtils::addChild(doc, node, type() + "ReferenceData");

    XMLUtils::addChild(doc, rdNode, "UnderlyingIndex", underlyingIndexName_);
    XMLUtils::addChild(doc, rdNode, "HedgeCurrency", quoteCurrency_);
    XMLUtils::addChild(doc, rdNode, "RebalanceFrequency", to_string(rebalancingFrequency_));
    XMLUtils::addChild(doc, rdNode, "rebalancingBusinessDayOfMonth_", rebalancingBusinessDayOfMonth_);
    XMLUtils::addChild(doc, rdNode, "HedgingCalendar", to_string(hedgeCalendar_));
    XMLUtils::addChild(doc, rdNode, "HedgingBusinessDayConvention", to_string(hedgeBdc_));
    if (referenceDateBusinessDaysOffset_ != 0)
        XMLUtils::addChild(doc, rdNode, "ReferenceBusinessDaysOffset", to_string(referenceDateBusinessDaysOffset_));
    if (hedgeAdjustmentFrequency_ != 0 * QuantLib::Days)
        XMLUtils::addChild(doc, rdNode, "HedgeAdjustmentFrequency", to_string(hedgeAdjustmentFrequency_));
    if (!data_.empty()) {
        XMLNode* currencyWeightNode = XMLUtils::addChild(doc, rdNode, "CurrencyWeights");
        for (auto d : data_) {
            XMLNode* underlyingNode = XMLUtils::addChild(doc, currencyWeightNode, "Currency");
            XMLUtils::addChild(doc, underlyingNode, "Name", d.first);
            XMLUtils::addChild(doc, underlyingNode, "Weight", d.second);
        }
    }
    return node;
}

// Credit
void CreditReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* innerNode = XMLUtils::getChildNode(node, "CreditReferenceData");
    QL_REQUIRE(innerNode, "No CreditReferenceData node");

    creditData_.name = XMLUtils::getChildValue(innerNode, "Name", true);
    creditData_.group = XMLUtils::getChildValue(innerNode, "Group", false);
    creditData_.successor = XMLUtils::getChildValue(innerNode, "Successor", false);
    creditData_.predecessor = XMLUtils::getChildValue(innerNode, "Predecessor", false);
    creditData_.successorImplementationDate =
        parseDate(XMLUtils::getChildValue(innerNode, "SuccessorImplementationDate", false));
    creditData_.predecessorImplementationDate =
        parseDate(XMLUtils::getChildValue(innerNode, "PredecessorImplementationDate", false));
}

XMLNode* CreditReferenceDatum::toXML(XMLDocument& doc) {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* creditNode = doc.allocNode("CreditReferenceData");
    XMLUtils::appendNode(node, creditNode);
    XMLUtils::addChild(doc, creditNode, "Name", creditData_.name);
    XMLUtils::addChild(doc, creditNode, "Group", creditData_.group);
    XMLUtils::addChild(doc, creditNode, "Successor", creditData_.successor);
    XMLUtils::addChild(doc, creditNode, "Predecessor", creditData_.predecessor);
    if (creditData_.successorImplementationDate != Date())
        XMLUtils::addChild(doc, creditNode, "SuccessorImplementationDate",
                           to_string(creditData_.successorImplementationDate));
    if (creditData_.predecessorImplementationDate != Date())
        XMLUtils::addChild(doc, creditNode, "PredecessorImplementationDate",
                           to_string(creditData_.predecessorImplementationDate));
    return node;
}

ReferenceDatumRegister<ReferenceDatumBuilder<CreditReferenceDatum>> CreditReferenceDatum::reg_(TYPE);

// Equity
void EquityReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* innerNode = XMLUtils::getChildNode(node, "EquityReferenceData");
    QL_REQUIRE(innerNode, "No EquityReferenceData node");

    equityData_.equityId = XMLUtils::getChildValue(innerNode, "EquityId", true);
    equityData_.equityName = XMLUtils::getChildValue(innerNode, "EquityName", true);
    equityData_.currency = XMLUtils::getChildValue(innerNode, "Currency", true);
    equityData_.scalingFactor = XMLUtils::getChildValueAsInt(innerNode, "ScalingFactor", true);
    equityData_.exchangeCode = XMLUtils::getChildValue(innerNode, "ExchangeCode", true);
    equityData_.isIndex = XMLUtils::getChildValueAsBool(innerNode, "IsIndex", true);
    equityData_.equityStartDate = parseDate(XMLUtils::getChildValue(innerNode, "EquityStartDate", true));
    equityData_.proxyIdentifier = XMLUtils::getChildValue(innerNode, "ProxyIdentifier", true);
    equityData_.simmBucket = XMLUtils::getChildValue(innerNode, "SimmBucket", true);
    equityData_.crifQualifier = XMLUtils::getChildValue(innerNode, "CrifQualifier", true);
    equityData_.proxyVolatilityId = XMLUtils::getChildValue(innerNode, "ProxyVolatilityId", true);
}

XMLNode* EquityReferenceDatum::toXML(ore::data::XMLDocument& doc) {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* equityNode = doc.allocNode("EquityReferenceData");
    XMLUtils::appendNode(node, equityNode);
    XMLUtils::addChild(doc, equityNode, "EquityId", equityData_.equityId);
    XMLUtils::addChild(doc, equityNode, "EquityName", equityData_.equityName);
    XMLUtils::addChild(doc, equityNode, "Currency", equityData_.currency);
    XMLUtils::addChild(doc, equityNode, "ScalingFactor", int(equityData_.scalingFactor));
    XMLUtils::addChild(doc, equityNode, "ExchangeCode", equityData_.exchangeCode);
    XMLUtils::addChild(doc, equityNode, "IsIndex", equityData_.isIndex);
    XMLUtils::addChild(doc, equityNode, "EquityStartDate", to_string(equityData_.equityStartDate));
    XMLUtils::addChild(doc, equityNode, "ProxyIdentifier", equityData_.proxyIdentifier);
    XMLUtils::addChild(doc, equityNode, "SimmBucket", equityData_.simmBucket);
    XMLUtils::addChild(doc, equityNode, "CrifQualifier", equityData_.crifQualifier);
    XMLUtils::addChild(doc, equityNode, "ProxyVolatilityId", equityData_.proxyVolatilityId);
    return node;
}

ReferenceDatumRegister<ReferenceDatumBuilder<EquityReferenceDatum>> EquityReferenceDatum::reg_(TYPE);

// Bond Basket
void BondBasketReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* b = XMLUtils::getChildNode(node, "BondBasketData");
    QL_REQUIRE(b, "No BondBasketData node");
    underlyingData_.clear();
    auto c = XMLUtils::getChildrenNodes(b, "Underlying");
    for (auto const n : c) {
        underlyingData_.push_back(BondUnderlying());
        underlyingData_.back().fromXML(n);
    }
}

XMLNode* BondBasketReferenceDatum::toXML(ore::data::XMLDocument& doc) {
    XMLNode* res = ReferenceDatum::toXML(doc);
    XMLNode* node = doc.allocNode("BondBasketData");
    XMLUtils::appendNode(res, node);
    for (auto& u : underlyingData_) {
        XMLUtils::appendNode(node, u.toXML(doc));
    }
    return res;
}

ReferenceDatumRegister<ReferenceDatumBuilder<BondBasketReferenceDatum>> BondBasketReferenceDatum::reg_(TYPE);
    
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
