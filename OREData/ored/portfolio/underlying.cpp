/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
 */

#include <ored/portfolio/underlying.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

Underlying::Underlying(const std::string& type, const std::string& name, const QuantLib::Real weight) : Underlying() {
    type_ = type;
    name_ = name;
    weight_ = weight;
};

void Underlying::fromXML(XMLNode* node) {
    type_ = XMLUtils::getChildValue(node, "Type", true);
    name_ = XMLUtils::getChildValue(node, "Name", true);
    if (auto n = XMLUtils::getChildNode(node, "Weight"))
        weight_ = parseReal(XMLUtils::getNodeValue(n));
    else
        weight_ = Null<Real>();
}

XMLNode* Underlying::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(nodeName_);
    XMLUtils::addChild(doc, node, "Type", type_);
    XMLUtils::addChild(doc, node, "Name", name_);
    if (weight_ != Null<Real>())
        XMLUtils::addChild(doc, node, "Weight", weight_);
    return node;
}

void BasicUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        name_ = XMLUtils::getNodeValue(node);
        isBasic_ = true;
    } else {
        QL_FAIL("Need a " << basicUnderlyingNodeName_ << " node for BasicUnderlying.");
    }
    setType("Basic");
}

XMLNode* BasicUnderlying::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode(nodeName_, name_);
    return node;
}

void EquityUnderlying::setEquityName() {
    if (equityName_.empty()) {
        string name = name_;
        if (!identifierType_.empty())
            name = identifierType_ + ":" + name_;
        if (!currency_.empty())
            name = name + ":" + currency_;
        if (!exchange_.empty()) {
            if (currency_.empty())
                name = name + ":";
            name = name + ":" + exchange_;
        }
        equityName_ = name;
    }
}

void EquityUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        name_ = XMLUtils::getNodeValue(node);
        isBasic_ = true;
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        Underlying::fromXML(node);
        QL_REQUIRE(type_ == "Equity", "Underlying must be of type 'Equity'.");
        identifierType_ = XMLUtils::getChildValue(node, "IdentifierType", false);
        // if no identfier is provided, we just use name
        if (!identifierType_.empty()) {
            currency_ = XMLUtils::getChildValue(node, "Currency", false);
            exchange_ = XMLUtils::getChildValue(node, "Exchange", false);
        }
        setEquityName();
        isBasic_ = false;
    } else {
        QL_FAIL("Need either a " << basicUnderlyingNodeName_ << " or " << nodeName_ << " for EquityUnderlying.");
    }
    setType("Equity");
}

XMLNode* EquityUnderlying::toXML(XMLDocument& doc) {
    XMLNode* node;
    if (isBasic_) {
        node = doc.allocNode(basicUnderlyingNodeName_, name_);
    } else {
        node = Underlying::toXML(doc);
        if (!identifierType_.empty())
            XMLUtils::addChild(doc, node, "IdentifierType", identifierType_);
        if (!currency_.empty())
            XMLUtils::addChild(doc, node, "Currency", currency_);
        if (!exchange_.empty())
            XMLUtils::addChild(doc, node, "Exchange", exchange_);
    }
    return node;
}

void CommodityUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        name_ = XMLUtils::getNodeValue(node);
        isBasic_ = true;
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        Underlying::fromXML(node);
        QL_REQUIRE(type_ == "Commodity", "Underlying must be of type 'Commodity'.");
        priceType_ = XMLUtils::getChildValue(node, "PriceType", false);
        if (auto n = XMLUtils::getChildNode(node, "FutureMonthOffset"))
            futureMonthOffset_ = parseInteger(XMLUtils::getNodeValue(n));
        else
            futureMonthOffset_ = Null<Size>();
        if (auto n = XMLUtils::getChildNode(node, "DeliveryRollDays"))
            deliveryRollDays_ = parseInteger(XMLUtils::getNodeValue(n));
        else
            deliveryRollDays_ = Null<Size>();
        deliveryRollCalendar_ = XMLUtils::getChildValue(node, "DeliveryRollCalendar", false);
        isBasic_ = false;
    } else {
        QL_FAIL("Need either a Name or Underlying node for CommodityUnderlying.");
    }
    setType("Commodity");
}

XMLNode* CommodityUnderlying::toXML(XMLDocument& doc) {
    XMLNode* node;
    if (isBasic_) {
        node = doc.allocNode(basicUnderlyingNodeName_, name_);
    } else {
        node = Underlying::toXML(doc);
        if (!priceType_.empty())
            XMLUtils::addChild(doc, node, "PriceType", priceType_);
        if (futureMonthOffset_ != Null<Size>())
            XMLUtils::addChild(doc, node, "FutureMonthOffset", (int)futureMonthOffset_);
        if (deliveryRollDays_ != Null<Size>())
            XMLUtils::addChild(doc, node, "DeliveryRollDays", (int)deliveryRollDays_);
        if (!deliveryRollCalendar_.empty())
            XMLUtils::addChild(doc, node, "DeliveryRollCalendar", deliveryRollCalendar_);
    }
    return node;
}

void FXUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        name_ = XMLUtils::getNodeValue(node);
        isBasic_ = true;
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        Underlying::fromXML(node);
        isBasic_ = false;
    } else {
        QL_FAIL("Need either a Name or Underlying node for FXUnderlying.");
    }
    setType("FX");
}

XMLNode* FXUnderlying::toXML(XMLDocument& doc) {
    XMLNode* node;
    if (isBasic_) {
        node = doc.allocNode(basicUnderlyingNodeName_, name_);
    } else {
        node = Underlying::toXML(doc);
    }
    return node;
}

void UnderlyingBuilder::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        underlying_ = boost::make_shared<BasicUnderlying>();
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        string type = XMLUtils::getChildValue(node, "Type", true);
        if (type == "Equity")
            underlying_ = boost::make_shared<EquityUnderlying>();
        else if (type == "Commodity")
            underlying_ = boost::make_shared<CommodityUnderlying>();
        else if (type == "FX")
            underlying_ = boost::make_shared<FXUnderlying>();
        else {
            QL_FAIL("Unknown Underlying type " << type);
        }
    } else {
        QL_FAIL("Need either a " << basicUnderlyingNodeName_ << " or " << nodeName_ << " node for Underlying.");
    }
    QL_REQUIRE(underlying_ != nullptr, "UnderlyingBuilder: underlying_ is null, this is unexpected");
    underlying_->setNodeName(nodeName_);
    underlying_->setBasicUnderlyingNodeName(basicUnderlyingNodeName_);
    underlying_->fromXML(node);
}

XMLNode* UnderlyingBuilder::toXML(XMLDocument& doc) { return NULL; }

} // namespace data
} // namespace ore
