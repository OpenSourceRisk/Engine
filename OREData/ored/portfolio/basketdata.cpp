/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ored/portfolio/basketdata.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <set>
#include <string>
#include <vector>

using namespace QuantLib;
using std::set;
using std::string;
using std::vector;

namespace ore {
namespace data {

BasketConstituent::BasketConstituent()
    : notional_(Null<Real>()), priorNotional_(Null<Real>()), weight_(Null<Real>()), priorWeight_(Null<Real>()),
      recovery_(Null<Real>()), weightInsteadOfNotional_(false) {}

BasketConstituent::BasketConstituent(const string& issuerName, const string& creditCurveId, Real notional,
                                     const string& currency, const string& qualifier, Real priorNotional, Real recovery,
                                     const Date& auctionDate, const Date& auctionSettlementDate,
                                     const Date& defaultDate, const Date& eventDeterminationDate)
    : issuerName_(issuerName), creditCurveId_(creditCurveId), notional_(notional), currency_(currency),
      qualifier_(qualifier), priorNotional_(priorNotional), weight_(Null<Real>()), priorWeight_(Null<Real>()),
      recovery_(recovery), auctionDate_(auctionDate), auctionSettlementDate_(auctionSettlementDate),
      defaultDate_(defaultDate), eventDeterminationDate_(eventDeterminationDate), weightInsteadOfNotional_(false) {}

BasketConstituent::BasketConstituent(const string& issuerName, const string& creditCurveId, Real weight,
                                     const string& qualifier, Real priorWeight, Real recovery, const Date& auctionDate,
                                     const Date& auctionSettlementDate, const Date& defaultDate,
                                     const Date& eventDeterminationDate)
    : issuerName_(issuerName), creditCurveId_(creditCurveId), notional_(Null<Real>()), currency_(""),
      qualifier_(qualifier), priorNotional_(), weight_(weight), priorWeight_(priorWeight), recovery_(recovery),
      auctionDate_(auctionDate), auctionSettlementDate_(auctionSettlementDate), defaultDate_(defaultDate),
      eventDeterminationDate_(eventDeterminationDate), weightInsteadOfNotional_(true) {}

//! Constructor taking CDS reference information, \p cdsReferenceInfo.
BasketConstituent::BasketConstituent(const string& issuerName, const CdsReferenceInformation& cdsReferenceInfo,
                                     Real notional, const string& currency, const string& qualifier, Real priorNotional,
                                     Real recovery, const Date& auctionDate, const Date& auctionSettlementDate,
                                     const Date& defaultDate, const Date& eventDeterminationDate)
    : issuerName_(issuerName), cdsReferenceInfo_(cdsReferenceInfo), creditCurveId_(cdsReferenceInfo_->id()),
      notional_(notional), currency_(currency), qualifier_(qualifier), priorNotional_(priorNotional),
      weight_(Null<Real>()), priorWeight_(Null<Real>()), recovery_(recovery), auctionDate_(auctionDate),
      auctionSettlementDate_(auctionSettlementDate), defaultDate_(defaultDate),
      eventDeterminationDate_(eventDeterminationDate), weightInsteadOfNotional_(false) {}

void BasketConstituent::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "Name");

    issuerName_ = XMLUtils::getChildValue(node, "IssuerId", true);
    qualifier_ = XMLUtils::getChildValue(node, "Qualifier", false);

    // May get an explicit CreditCurveId node. If so, we use it. Otherwise, we must have ReferenceInformation node.
    if (XMLNode* tmp = XMLUtils::getChildNode(node, "CreditCurveId")) {
        creditCurveId_ = XMLUtils::getNodeValue(tmp);
    } else {
        tmp = XMLUtils::getChildNode(node, "ReferenceInformation");
        QL_REQUIRE(tmp, "Need either a CreditCurveId or ReferenceInformation node in each BasketConstituent.");
        cdsReferenceInfo_ = CdsReferenceInformation();
        cdsReferenceInfo_->fromXML(tmp);
        creditCurveId_ = cdsReferenceInfo_->id();
    }

    bool zeroWeightOrZeroNotional = false;
    if (auto notionalNode = XMLUtils::getChildNode(node, "Notional")) {
        weightInsteadOfNotional_ = false;
        notional_ = parseReal(XMLUtils::getNodeValue(notionalNode));
        currency_ = XMLUtils::getChildValue(node, "Currency", true);
        if (close(notional_, 0.0))
            zeroWeightOrZeroNotional = true;
    } else {
        auto weightNode = XMLUtils::getChildNode(node, "Weight");
        QL_REQUIRE(weightNode, "a 'Notional' or 'Weight' node is mandatory.");
        weightInsteadOfNotional_ = true;
        weight_ = parseReal(XMLUtils::getNodeValue(weightNode));
        if (close(weight_, 0.0))
            zeroWeightOrZeroNotional = true;
        currency_ = "";
    }
    if (zeroWeightOrZeroNotional) {
        priorNotional_ = Null<Real>();
        priorWeight_ = Null<Real>();
        if (!weightInsteadOfNotional_) {
            if (auto n = XMLUtils::getChildNode(node, "PriorNotional"))
                priorNotional_ = parseReal(XMLUtils::getNodeValue(n));
        } else {
            if (auto n = XMLUtils::getChildNode(node, "PriorWeight"))
                priorWeight_ = parseReal(XMLUtils::getNodeValue(n));
        }

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

XMLNode* BasketConstituent::toXML(ore::data::XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("Name");

    XMLUtils::addChild(doc, node, "IssuerId", issuerName_);
    if (!qualifier_.empty())
        XMLUtils::addChild(doc, node, "Qualifier", qualifier_);

    // We either have reference information or an explicit credit curve ID
    if (cdsReferenceInfo_) {
        XMLUtils::appendNode(node, cdsReferenceInfo_->toXML(doc));
    } else {
        XMLUtils::addChild(doc, node, "CreditCurveId", creditCurveId_);
    }

    if (!weightInsteadOfNotional_) {
        XMLUtils::addChild(doc, node, "Notional", notional_);
        XMLUtils::addChild(doc, node, "Currency", currency_);
    } else {
        XMLUtils::addChild(doc, node, "Weight", weight_);
    }

    if (close(notional_, 0.0) && !weightInsteadOfNotional_) {

        if (priorNotional_ != Null<Real>())
            XMLUtils::addChild(doc, node, "PriorNotional", priorNotional_);

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
    } else if (close(weight_, 0.0) && weightInsteadOfNotional_) {

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

const string& BasketConstituent::issuerName() const { return issuerName_; }

const string& BasketConstituent::creditCurveId() const { return creditCurveId_; }

const boost::optional<CdsReferenceInformation>& BasketConstituent::cdsReferenceInfo() const {
    return cdsReferenceInfo_;
}

QuantLib::Real BasketConstituent::notional() const {
    QL_REQUIRE(!weightInsteadOfNotional_, "Try to access notional from basket constituent "
                                                        << issuerName_ << ", but weight (w=" << weight_
                                                        << ") was given.");
    return notional_;
}
const string& BasketConstituent::currency() const { 
    QL_REQUIRE(!weightInsteadOfNotional_, "Try to access currceny from basket constituent "
                                              << issuerName_ << ", but weight instead of notional given");
    return currency_;
}

QuantLib::Real BasketConstituent::priorNotional() const {
    QL_REQUIRE(!weightInsteadOfNotional_, "Try to access priorNotional from basket constituent "
                                                        << issuerName_ << ", but priorWeight (w=" << priorWeight_
                                                        << ") was given.");
    return priorNotional_;
}

Real BasketConstituent::recovery() const { return recovery_; }

QuantLib::Real BasketConstituent::weight() const {
    QL_REQUIRE(weightInsteadOfNotional_, "Try to access weight from basket constituent "
                                                      << issuerName_ << ", but notional (N=" << notional_ << " "
                                                      << currency_ << ") was given.");
    return weight_;
}

QuantLib::Real BasketConstituent::priorWeight() const {
    QL_REQUIRE(weightInsteadOfNotional_, "Try to access priorWeight from basket constituent "
                                                      << issuerName_ << ", but priorNotional (N=" << priorNotional_ << " "
                                                      << currency_ << ") was given.");
    return priorWeight_;
}

const Date& BasketConstituent::auctionDate() const { return auctionDate_; }

const Date& BasketConstituent::auctionSettlementDate() const { return auctionSettlementDate_; }

const Date& BasketConstituent::defaultDate() const { return defaultDate_; }

const Date& BasketConstituent::eventDeterminationDate() const { return eventDeterminationDate_; }

bool BasketConstituent::weightInsteadOfNotional() const { return weightInsteadOfNotional_; }

bool operator<(const BasketConstituent& lhs, const BasketConstituent& rhs) {
    return lhs.creditCurveId() < rhs.creditCurveId();
}

BasketData::BasketData() {}

BasketData::BasketData(const vector<BasketConstituent>& constituents) : constituents_(constituents) {}

const vector<BasketConstituent>& BasketData::constituents() const { return constituents_; }

void BasketData::fromXML(XMLNode* node) {

    XMLUtils::checkNode(node, "BasketData");

    constituents_.clear();
    for (XMLNode* child = XMLUtils::getChildNode(node, "Name"); child; child = XMLUtils::getNextSibling(child)) {
        BasketConstituent constituent;
        constituent.fromXML(child);
        constituents_.push_back(constituent);
    }
}

XMLNode* BasketData::toXML(ore::data::XMLDocument& doc) const {

    XMLNode* node = doc.allocNode("BasketData");

    for (auto c : constituents_) {
        auto cNode = c.toXML(doc);
        XMLUtils::appendNode(node, cNode);
    }

    return node;
}
} // namespace data
} // namespace ore
