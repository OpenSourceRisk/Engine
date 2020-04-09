/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
 */

#include <ored/portfolio/underlying.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void Underlying::fromXML(XMLNode* node) {
    type_ = XMLUtils::getChildValue(node, "Type", true);
    name_ = XMLUtils::getChildValue(node, "Name", true);
    if (auto n = XMLUtils::getChildNode(node, "Weight"))
        weight_ = parseReal(XMLUtils::getNodeValue(n));
    else
        weight_ = Null<Real>();
}

XMLNode* Underlying::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Underlying");
    XMLUtils::addChild(doc, node, "Type", type_);
    XMLUtils::addChild(doc, node, "Name", name_);
    if (weight_ != Null<Real>())
        XMLUtils::addChild(doc, node, "Weight", weight_);
    return node;
}

void BasicUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == "Name") {
        name_ = XMLUtils::getNodeValue(node);
    } else {
        QL_FAIL("Need a Name node for BasicUnderlying.");
    }
    setType("Basic");
}

XMLNode* BasicUnderlying::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Name", name_);
    return node;
}

void EquityUnderlying::setEquityName() {
    if (equityName_.empty()) {
        string name = identifierType_ + ":" + name_;
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
    if (XMLUtils::getNodeName(node) == "Name"){
        name_ = XMLUtils::getNodeValue(node);
    } else if (XMLUtils::getNodeName(node) == "Underlying") {
        Underlying::fromXML(node);
        QL_REQUIRE(type_ == "Equity", "Underlying must be of type 'Equity'.");
        identifierType_ = XMLUtils::getChildValue(node, "IdentifierType", false);
        // if no identfier is provided, we just use name
        if (!identifierType_.empty()) {
            currency_ = XMLUtils::getChildValue(node, "Currency", false);
            exchange_ = XMLUtils::getChildValue(node, "Exchange", false);
            setEquityName();
        }
    } else {
        QL_FAIL("Need either a Name or Underlying node for EquityUnderlying.");
    }
    setType("Equity");
}

XMLNode* EquityUnderlying::toXML(XMLDocument& doc) {
    XMLNode* node;
    if (equityName_.empty()) {
        node = doc.allocNode("Name", name_);
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
    if (XMLUtils::getNodeName(node) == "Name") {
        name_ = XMLUtils::getNodeValue(node);
    } else if (XMLUtils::getNodeName(node) == "Underlying") {
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
    } else {
        QL_FAIL("Need either a Name or Underlying node for CommodityUnderlying.");
    }
    setType("Commodity");
}

XMLNode* CommodityUnderlying::toXML(XMLDocument& doc) {
    XMLNode* node;
    node = Underlying::toXML(doc);
    if (!priceType_.empty())
        XMLUtils::addChild(doc, node, "PriceType", priceType_);
    if (weight_ != Null<Real>())
        XMLUtils::addChild(doc, node, "Weight", weight_);
    if (futureMonthOffset_ != Null<Size>())
        XMLUtils::addChild(doc, node, "FutureMonthOffset", (int)futureMonthOffset_);
    if (deliveryRollDays_ != Null<Size>())
        XMLUtils::addChild(doc, node, "DeliveryRollDays", (int)deliveryRollDays_);
    if (!deliveryRollCalendar_.empty())
        XMLUtils::addChild(doc, node, "DeliveryRollCalendar", deliveryRollCalendar_);
    return node;
}

void FXUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == "Name") {
        name_ = XMLUtils::getNodeValue(node);
    } else if (XMLUtils::getNodeName(node) == "Underlying") {
        Underlying::fromXML(node);
    } else {
        QL_FAIL("Need either a Name or Underlying node for FXUnderlying.");
    }
    setType("FX");
}

XMLNode* FXUnderlying::toXML(XMLDocument& doc) {
    XMLNode* node;
    node = Underlying::toXML(doc);
    return node;
}

void UnderlyingBuilder::fromXML(XMLNode* node) {    
    if (XMLUtils::getNodeName(node) == "Name") {
        underlying_ = boost::make_shared<BasicUnderlying>();
    } else if (XMLUtils::getNodeName(node) == "Underlying") {
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
        underlying_->fromXML(node);
    } else {
        QL_FAIL("Need either a Name or Underlying node for Underlying.");
    }
}

XMLNode* UnderlyingBuilder::toXML(XMLDocument& doc) {
    return NULL;
}

} // namespace data
} // namespace oreplus
