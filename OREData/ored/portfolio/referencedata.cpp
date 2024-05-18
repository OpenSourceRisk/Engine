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
#include <ored/portfolio/tradefactory.hpp>
#include <ored/portfolio/structuredtradeerror.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using std::function;

namespace ore {
namespace data {

void ReferenceDatum::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ReferenceDatum");
    type_ = XMLUtils::getChildValue(node, "Type", true);
    id_ = XMLUtils::getAttribute(node, "id");
    std::string dateStr = XMLUtils::getAttribute(node, "validFrom");
    if (!dateStr.empty()) {
        validFrom_ = parseDate(dateStr);
    } else {
        validFrom_ = QuantLib::Date::minDate();    
    }
}

XMLNode* ReferenceDatum::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ReferenceDatum");
    QL_REQUIRE(node, "Failed to create ReferenceDatum node");
    XMLUtils::addAttribute(doc, node, "id", id_);
    XMLUtils::addChild(doc, node, "Type", type_);
    if (validFrom_ > QuantLib::Date::minDate()) {
        XMLUtils::addAttribute(doc, node, "validFrom", to_string(validFrom_));    
    }
    return node;
}

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
    subType = XMLUtils::getChildValue(node, "SubType", false);

    legData.clear();
    XMLNode* legNode = XMLUtils::getChildNode(node, "LegData");
    while (legNode != nullptr) {
        LegData ld;
        ld.fromXML(legNode);
        legData.push_back(ld);
        legNode = XMLUtils::getNextSibling(legNode, "LegData");
    }
}

XMLNode* BondReferenceDatum::BondData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("BondData");
    XMLUtils::addChild(doc, node, "IssuerId", issuerId);
    XMLUtils::addChild(doc, node, "CreditCurveId", creditCurveId);
    XMLUtils::addChild(doc, node, "CreditGroup", creditGroup);
    XMLUtils::addChild(doc, node, "ReferenceCurveId", referenceCurveId);
    XMLUtils::addChild(doc, node, "IncomeCurveId", incomeCurveId);
    XMLUtils::addChild(doc, node, "VolatilityCurveId", volatilityCurveId);
    XMLUtils::addChild(doc, node, "SettlementDays", settlementDays);
    XMLUtils::addChild(doc, node, "Calendar", calendar);
    XMLUtils::addChild(doc, node, "IssueDate", issueDate);
    XMLUtils::addChild(doc, node, "PriceQuoteMethod", priceQuoteMethod);
    XMLUtils::addChild(doc, node, "PriceQuoteBaseValue", priceQuoteBaseValue);
    XMLUtils::addChild(doc, node, "SubType", subType);
    for (auto& bd : legData)
        XMLUtils::appendNode(node, bd.toXML(doc));
    return node;
}

void BondReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    bondData_.fromXML(XMLUtils::getChildNode(node, "BondReferenceData"));
}

XMLNode* BondReferenceDatum::toXML(XMLDocument& doc) const {
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

XMLNode* CreditIndexConstituent::toXML(ore::data::XMLDocument& doc) const {

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

CreditIndexReferenceDatum::CreditIndexReferenceDatum() {}

CreditIndexReferenceDatum::CreditIndexReferenceDatum(const string& name) : ReferenceDatum(TYPE, name) {}

CreditIndexReferenceDatum::CreditIndexReferenceDatum(const string& name, const QuantLib::Date& validFrom)
    : ReferenceDatum(TYPE, name, validFrom) {}

void CreditIndexReferenceDatum::fromXML(XMLNode* node) {

    ReferenceDatum::fromXML(node);

    XMLNode* cird = XMLUtils::getChildNode(node, "CreditIndexReferenceData");
    QL_REQUIRE(cird, "Expected a CreditIndexReferenceData node.");

    indexFamily_ = XMLUtils::getChildValue(cird, "IndexFamily", false);

    constituents_.clear();

    for (XMLNode* child = XMLUtils::getChildNode(cird, "Underlying"); child;
         child = XMLUtils::getNextSibling(child, "Underlying")) {
        CreditIndexConstituent c;
        c.fromXML(child);
        add(c);
    }
}

XMLNode* CreditIndexReferenceDatum::toXML(ore::data::XMLDocument& doc) const {

    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* cird = XMLUtils::addChild(doc, node, "CreditIndexReferenceData");

    XMLUtils::addChild(doc, cird, "IndexFamily", indexFamily_);

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

XMLNode* IndexReferenceDatum::toXML(XMLDocument& doc) const {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* rdNode = XMLUtils::addChild(doc, node, type() + "ReferenceData");

    for (auto d : data_) {
        XMLNode* underlyingNode = XMLUtils::addChild(doc, rdNode, "Underlying");
        XMLUtils::addChild(doc, underlyingNode, "Name", d.first);
        XMLUtils::addChild(doc, underlyingNode, "Weight", d.second);
    }

    return node;
}
// Currency Hedged Equity Indexes
/*
<ReferenceDatum id="RIC:.SPXEURHedgedMonthly">
  <Type>CurrencyHedgedEquityIndex</Type>
  <CurrencyHedgedEquityIndexReferenceDatum>
      <UnderlyingIndex>RIC:.SPX</UnderlyingIndex>
      <UnderlyingIndexCurrency>USD</UnderlyingIndexCurrency>
      <HedgeCurrency>EUR</HedgeCurrency>
      <RebalancingStrategy>EndOfMonth</RebalancingStrategy>
      <ReferenceDateOffset>1</ReferenceDateOffset>
      <HedgeAdjustment>None|Daily</HedgeAdjustment>
      <HedgeCalendar>EUR,USD</HedgeCalendar>
      <FxIndex>ECB-EUR-USD</FxIndex>
      <IndexWeightsAtLastRebalancingDate>
        <Underlying>
            <Name>Apple</Name>
            <Weight>0.1</Weight>
        </Underlying>
        ...
      </IndexWeightsAtLastRebalancingDate>
  </CurrencyHedgedEquityIndexReferenceDatum>
</ReferenceDatum>
*/

void CurrencyHedgedEquityIndexReferenceDatum::fromXML(XMLNode* node) {
    ReferenceDatum::fromXML(node);
    XMLNode* innerNode = XMLUtils::getChildNode(node, type() + "ReferenceData");
    QL_REQUIRE(innerNode, "No " + type() + "ReferenceData node");

    underlyingIndexName_ = XMLUtils::getChildValue(innerNode, "UnderlyingIndex", true);
    
    auto rebalancingStr = XMLUtils::getChildValue(innerNode, "RebalancingStrategy", false, "EndOfMonth");
    if (rebalancingStr == "EndOfMonth") {
        rebalancingStrategy_ = CurrencyHedgedEquityIndexReferenceDatum::RebalancingDate::EndOfMonth;
    } else {
        QL_FAIL("unexpected rebalancing strategy " << rebalancingStr);
    }

    std::string hedgeCalendarStr = XMLUtils::getChildValue(innerNode, "HedgeCalendar", true);
    hedgeCalendar_ = parseCalendar(hedgeCalendarStr);
 
    XMLNode* fxIndexesNode = XMLUtils::getChildNode(innerNode, "FxIndexes");
    if (fxIndexesNode) {
        for (XMLNode* child = XMLUtils::getChildNode(fxIndexesNode, "FxIndex"); child;
             child = XMLUtils::getNextSibling(child, "FxIndex")) {
            string currency = XMLUtils::getChildValue(child, "Currency", true);
            string indexFamily = XMLUtils::getChildValue(child, "IndexName", true);
            fxIndexes_[currency] = indexFamily;
        }
    }
    // Optional Fields
    referenceDateOffset_ = XMLUtils::getChildValueAsInt(innerNode, "ReferenceDateOffset", false, 0);
    auto hedgeAdjStr = XMLUtils::getChildValue(innerNode, "HedgeAdjustment", false, "None");
    
    if (hedgeAdjStr == "None") {
        hedgeAdjustmentRule_ = HedgeAdjustment::None;
    } else if (hedgeAdjStr == "Daily") {
        hedgeAdjustmentRule_ = HedgeAdjustment::Daily;
    } else {
        QL_FAIL("unexpected rebalancing strategy " << hedgeAdjStr);
    }
    // clear map
    data_.clear();

    // and populate it...
    XMLNode* currencyWeightNode = XMLUtils::getChildNode(innerNode, "IndexWeightsAtLastRebalancingDate");
    if (currencyWeightNode) {
        double totalWeight = 0.0;
        for (XMLNode* child = XMLUtils::getChildNode(currencyWeightNode, "Underlying"); child;
             child = XMLUtils::getNextSibling(child, "Underlying")) {
            string name = XMLUtils::getChildValue(child, "Name", true);
            double weight = XMLUtils::getChildValueAsDouble(child, "Weight", true);
            QL_REQUIRE(weight > 0 || QuantLib::close_enough(weight, 0.0),
                       "Try to add negtive weight for Underlying" << name);
            data_[name] += weight;
            totalWeight += weight;
        }
        QL_REQUIRE(data_.empty() || QuantLib::close_enough(totalWeight, 1.0),
                   "Sum of underlying weights at last rebalancing date (" << totalWeight << ") is not 1.0");
    }
}

XMLNode* CurrencyHedgedEquityIndexReferenceDatum::toXML(XMLDocument& doc) const {
    XMLNode* node = ReferenceDatum::toXML(doc);
    XMLNode* rdNode = XMLUtils::addChild(doc, node, type() + "ReferenceData");

    XMLUtils::addChild(doc, rdNode, "UnderlyingIndex", underlyingIndexName_);
    XMLUtils::addChild(doc, rdNode, "RebalancingStrategy", "EndOfMonth");
    XMLUtils::addChild(doc, rdNode, "HedgeCalendar", to_string(hedgeCalendar_));
    if (referenceDateOffset_ != 0)
        XMLUtils::addChild(doc, rdNode, "ReferenceDateOffset", to_string(referenceDateOffset_));
    if (hedgeAdjustmentRule_ == HedgeAdjustment::Daily) {
        XMLUtils::addChild(doc, rdNode, "HedgeAdjustment", "Daily");
    } 

    if (!fxIndexes_.empty()) {
        XMLNode* currencyWeightNode = XMLUtils::addChild(doc, rdNode, "FxIndexes");
        for (auto d : fxIndexes_) {
            XMLNode* underlyingNode = XMLUtils::addChild(doc, currencyWeightNode, "FxIndex");
            XMLUtils::addChild(doc, underlyingNode, "Currency", d.first);
            XMLUtils::addChild(doc, underlyingNode, "IndexName", d.second);
        }
    }

    if (!data_.empty()) {
        XMLNode* currencyWeightNode = XMLUtils::addChild(doc, rdNode, "IndexWeightsAtLastRebalancingDate");
        for (auto d : data_) {
            XMLNode* underlyingNode = XMLUtils::addChild(doc, currencyWeightNode, "Underlying");
            XMLUtils::addChild(doc, underlyingNode, "Name", d.first);
            XMLUtils::addChild(doc, underlyingNode, "Weight", d.second);
        }
    }
    return node;
}

// Portfolio Basket
/*
<ReferenceDatum id="MSFDSJP">
 <PortfolioBasketReferenceData>
  <Components>
   <Trade>
    <TradeType>EquityPosition</TradeType>
     <Envelope>
      <CounterParty>{{netting_set_id}}</CounterParty>
       <NettingSetId>{{netting_set_id}}</NettingSetId>
       <AdditionalFields>
        <valuation_date>2023-11-07</valuation_date>
        <im_model>SIMM</im_model>
        <post_regulations>SEC</post_regulations>
        <collect_regulations>SEC</collect_regulations>
       </AdditionalFields>
      </Envelope>
      <EquityPositionData>
       <Quantity>7006.0</Quantity>
        <Underlying>
         <Type>Equity</Type>
         <Name>CR.N</Name>
         <IdentifierType>RIC</IdentifierType>
        </Underlying>
       </EquityPositionData>
      </Trade>
      <Trade id="CashSWAP_USD.CASH">
       <TradeType>Swap</TradeType>
        <Envelope>
          <CounterParty>{{netting_set_id}}</CounterParty>
           <NettingSetId>{{netting_set_id}}</NettingSetId>
           <AdditionalFields>
            <valuation_date>2023-11-07</valuation_date>
            <im_model>SIMM</im_model>
            <post_regulations>SEC</post_regulations>
            <collect_regulations>SEC</collect_regulations>
           </AdditionalFields>
          </Envelope>
          <SwapData>
           <LegData>
           <Payer>true</Payer>
           <LegType>Cashflow</LegType>
           <Currency>USD</Currency>
           <CashflowData>
            <Cashflow>
             <Amount date="2023-11-08">28641475.824680243</Amount>
            </Cashflow>
           </CashflowData>
       </LegData>
      </SwapData>
     </Trade>
   </Components>
  </PortfolioBasketReferenceData>
</ReferenceDatum>
*/

void PortfolioBasketReferenceDatum::fromXML(XMLNode* node) {
        ReferenceDatum::fromXML(node);
        XMLNode* innerNode = XMLUtils::getChildNode(node, type() + "ReferenceData");
        QL_REQUIRE(innerNode, "No " + type() + "ReferenceData node");  

        // Get the "Components" node
        XMLNode* componentsNode = XMLUtils::getChildNode(innerNode, "Components");
        QL_REQUIRE(componentsNode, "No Components node");

        tradecomponents_.clear();
        auto c = XMLUtils::getChildrenNodes(componentsNode, "Trade");
        int k = 0;
        for (auto const n : c) {

            string tradeType = XMLUtils::getChildValue(n, "TradeType", true);
            string id = XMLUtils::getAttribute(n, "id");
            if (id == "") {
                id = std::to_string(k);
            }
            
            DLOG("Parsing composite trade " << this->id() << " node " << k << " with id: " << id);
            
            QuantLib::ext::shared_ptr<Trade> trade;
            try {
                trade = TradeFactory::instance().build(tradeType);
                trade->id() = id;
                Envelope componentEnvelope;
                if (XMLNode* envNode = XMLUtils::getChildNode(n, "Envelope")) {
                   componentEnvelope.fromXML(envNode);
                }
                Envelope env;
                // the component trade's envelope is the main trade's envelope with possibly overwritten add fields
                for (auto const& [k, v] : componentEnvelope.fullAdditionalFields()) {
                   env.setAdditionalField(k, v);
                }
                    
                trade->setEnvelope(env);
                trade->fromXML(n);
                tradecomponents_.push_back(trade);
                DLOG("Added Trade " << id << " (" << trade->id() << ")"
                                        << " type:" << tradeType << " to composite trade " << this->id() << ".");
                k += 1;
            } catch (const std::exception& e) {
                StructuredTradeErrorMessage(
                    id, tradeType,
                    "Failed to build subtrade with id '" + id + "' inside composite trade: ", e.what())
                    .log();
            }
            
         }
}

XMLNode* PortfolioBasketReferenceDatum::toXML(XMLDocument& doc) const {
        XMLNode* node = ReferenceDatum::toXML(doc);
        XMLNode* rdNode = XMLUtils::addChild(doc, node, type() + "ReferenceData");
        XMLUtils::appendNode(node, rdNode);
        XMLNode* cNode = XMLUtils::addChild(doc, rdNode, "Components");
        for (auto& u : tradecomponents_) {
            auto test = u->toXML(doc);
            XMLUtils::appendNode(cNode, test);
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
    creditData_.entityType = (XMLUtils::getChildValue(innerNode, "EntityType", false) == "Corp."|| 
                              XMLUtils::getChildValue(innerNode, "EntityType", false) == "Corp")
                                 ? "Corporate"
                                 : XMLUtils::getChildValue(innerNode, "EntityType", false);
}

XMLNode* CreditReferenceDatum::toXML(XMLDocument& doc) const {
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
    XMLUtils::addChild(doc, creditNode, "EntityType", creditData_.entityType);
    return node;
}

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

XMLNode* EquityReferenceDatum::toXML(ore::data::XMLDocument& doc) const {
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

XMLNode* BondBasketReferenceDatum::toXML(ore::data::XMLDocument& doc) const {
    XMLNode* res = ReferenceDatum::toXML(doc);
    XMLNode* node = doc.allocNode("BondBasketData");
    XMLUtils::appendNode(res, node);
    for (auto& u : underlyingData_) {
        XMLUtils::appendNode(node, u.toXML(doc));
    }
    return res;
}

// BasicReferenceDataManager

void BasicReferenceDataManager::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "ReferenceData");
    for (XMLNode* child = XMLUtils::getChildNode(node, "ReferenceDatum"); child;
         child = XMLUtils::getNextSibling(child, "ReferenceDatum")) {
        addFromXMLNode(child);
    }
}

void BasicReferenceDataManager::add(const QuantLib::ext::shared_ptr<ReferenceDatum>& rd) {
    // Add reference datum, it is overwritten if it is already present.
    data_[make_pair(rd->type(), rd->id())][rd->validFrom()] = rd;
}

QuantLib::ext::shared_ptr<ReferenceDatum> BasicReferenceDataManager::addFromXMLNode(XMLNode* node, const std::string& inputId,
                                                                            const QuantLib::Date& inputValidFrom) {
    string refDataType = XMLUtils::getChildValue(node, "Type", false);
    QuantLib::ext::shared_ptr<ReferenceDatum> refData;

    if (refDataType.empty()) {
        ALOG("Found referenceDatum without Type - skipping");
        return refData;
    }

    string id = inputId.empty() ? XMLUtils::getAttribute(node, "id") : inputId;
    
    string validFromStr = XMLUtils::getAttribute(node, "validFrom");
    QuantLib::Date validFrom;
    if (validFromStr.empty()) {
        validFrom = QuantLib::Date::minDate();
    } else {
        validFrom = ore::data::parseDate(validFromStr);
    }

    validFrom = inputValidFrom == QuantLib::Null<QuantLib::Date>() ? validFrom : inputValidFrom;

    if (id.empty()) {
        ALOG("Found referenceDatum without id - skipping");
        return refData;
    }

    if (auto it = data_.find(make_pair(refDataType, id)); it != data_.end()) {
        if (it->second.count(validFrom) > 0) {
            duplicates_.insert(make_tuple(refDataType, id, validFrom));
            ALOG("Found duplicate referenceDatum for type='" << refDataType << "', id='" << id << "', validFrom='"
                                                             << validFrom << "'");
            return refData;
        }
    }

    try {
        refData = buildReferenceDatum(refDataType);
        refData->fromXML(node);
        // set the type and id at top level (is this needed?)
        refData->setType(refDataType);
        refData->setId(id);
        refData->setValidFrom(validFrom);
        data_[make_pair(refDataType, id)][validFrom] = refData;
        TLOG("added referenceDatum for type='" << refDataType << "', id='" << id << "', validFrom='" << validFrom
                                               << "'");
    } catch (const std::exception& e) {
        buildErrors_[make_pair(refDataType, id)][validFrom] = e.what();
        ALOG("Error building referenceDatum for type='" << refDataType << "', id='" << id << "', validFrom='" << validFrom
                                               << "': " << e.what());
    }

    return refData;
}

QuantLib::ext::shared_ptr<ReferenceDatum> BasicReferenceDataManager::buildReferenceDatum(const string& refDataType) {
    auto refData = ReferenceDatumFactory::instance().build(refDataType);
    QL_REQUIRE(refData,
               "Reference data type " << refDataType << " has not been registered with the reference data factory.");
    return refData;
}

XMLNode* BasicReferenceDataManager::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ReferenceData");
    for (const auto& kv : data_) {
        for (const auto& [_, refData] : kv.second) {
            XMLUtils::appendNode(node, refData->toXML(doc));
        }
    }
    return node;
}

std::tuple<QuantLib::Date, QuantLib::ext::shared_ptr<ReferenceDatum>> BasicReferenceDataManager::latestValidFrom(const string& type, const string& id,
                                                          const QuantLib::Date& asof) const {   
    auto it = data_.find(make_pair(type, id));
    if (it != data_.end() && !it->second.empty()){
        auto uB = it->second.upper_bound(asof);
        if (uB != it->second.begin()) {
            return *(--uB);
        } 
    }
    return {QuantLib::Date(), nullptr};
}

void BasicReferenceDataManager::check(const string& type, const string& id, const QuantLib::Date& validFrom) const {
    auto key = make_tuple(type, id, validFrom);
    if (duplicates_.find(key) != duplicates_.end())
        ALOG("BasicReferenceDataManager: duplicate entries for type='" << type << "', id='" << id << "', validFrom='"
                                                                       << validFrom << "'");
    auto err = buildErrors_.find(make_pair(type, id));
    if (err != buildErrors_.end()) {
        for (const auto& [validFrom, error] : err->second) {
            ALOG("BasicReferenceDataManager: Build error for type='" << type << "', id='" << id << "', validFrom='"
                                                                     << validFrom << "': " << error);
        }
    }
        
}

bool BasicReferenceDataManager::hasData(const string& type, const string& id, const QuantLib::Date& asof) {
    Date asofDate = asof;
    if (asofDate == QuantLib::Null<QuantLib::Date>()) {
        asofDate = Settings::instance().evaluationDate();
    }
    auto [validFrom, refData] = latestValidFrom(type, id, asofDate);
    check(type, id, validFrom);
    return refData != nullptr;
}

QuantLib::ext::shared_ptr<ReferenceDatum> BasicReferenceDataManager::getData(const string& type, const string& id,
                                                                     const QuantLib::Date& asof) {
    Date asofDate = asof;
    if (asofDate == QuantLib::Null<QuantLib::Date>()) {
        asofDate = Settings::instance().evaluationDate();
    }
    auto [validFrom, refData] = latestValidFrom(type, id, asofDate);
    check(type, id, validFrom);
    QL_REQUIRE(refData != nullptr, "BasicReferenceDataManager::getData(): No Reference data for type='"
                                       << type << "', id='" << id << "', asof='" << asof << "'");
    return refData;
}

} // namespace data
} // namespace ore
