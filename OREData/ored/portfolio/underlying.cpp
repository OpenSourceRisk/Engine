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
        weight_ = 1.0;
}

XMLNode* Underlying::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(nodeName_);
    XMLUtils::addChild(doc, node, "Type", type_);
    XMLUtils::addChild(doc, node, "Name", name_);
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

XMLNode* BasicUnderlying::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode(basicUnderlyingNodeName_, name_);
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
        // if no identifier is provided, we just use name
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

XMLNode* EquityUnderlying::toXML(XMLDocument& doc) const {
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
        futureExpiryDate_ = XMLUtils::getChildValue(node, "FutureExpiryDate", false);
        futureContractMonth_ = XMLUtils::getChildValue(node, "FutureContractMonth", false);
        QL_REQUIRE(futureExpiryDate_.empty() || futureContractMonth_.empty(),
                   "Only futureExpiryDate or futureContractMonth are allowed not both");
    } else {
        QL_FAIL("Need either a Name or Underlying node for CommodityUnderlying.");
    }
    setType("Commodity");
}

XMLNode* CommodityUnderlying::toXML(XMLDocument& doc) const {
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
        if (!futureExpiryDate_.empty()) 
            XMLUtils::addChild(doc, node, "FutureExpiryDate", futureExpiryDate_);
        if (!futureContractMonth_.empty())
            XMLUtils::addChild(doc, node, "FutureContractMonth", futureContractMonth_);
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

XMLNode* FXUnderlying::toXML(XMLDocument& doc) const {
    XMLNode* node;
    if (isBasic_) {
        node = doc.allocNode(basicUnderlyingNodeName_, name_);
    } else {
        node = Underlying::toXML(doc);
    }
    return node;
}

void InterestRateUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        name_ = XMLUtils::getNodeValue(node);
        isBasic_ = true;
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        Underlying::fromXML(node);
        isBasic_ = false;
    } else {
        QL_FAIL("Need either a Name or Underlying node for InterestRateUnderlying.");
    }
    setType("InterestRate");
}

XMLNode* InterestRateUnderlying::toXML(XMLDocument& doc) const {
    XMLNode* node;
    if (isBasic_) {
        node = doc.allocNode(basicUnderlyingNodeName_, name_);
    } else {
        node = Underlying::toXML(doc);
    }
    return node;
}

void CreditUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        name_ = XMLUtils::getNodeValue(node);
        isBasic_ = true;
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        Underlying::fromXML(node);
        isBasic_ = false;
    } else {
        QL_FAIL("Need either a Name or Underlying node for CreditUnderlying.");
    }
    setType("Credit");
}

XMLNode* CreditUnderlying::toXML(XMLDocument& doc) const {
    XMLNode* node;
    if (isBasic_) {
        node = doc.allocNode(basicUnderlyingNodeName_, name_);
    } else {
        node = Underlying::toXML(doc);
    }
    return node;
}

void InflationUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        name_ = XMLUtils::getNodeValue(node);
        isBasic_ = true;
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        Underlying::fromXML(node);
        // optional
        std::string interpolationString = XMLUtils::getChildValue(node, "Interpolation", false);
        if (interpolationString != "")
            interpolation_ = parseObservationInterpolation(interpolationString);
        else
            interpolation_ = QuantLib::CPI::InterpolationType::Flat;
        isBasic_ = false;
    } else {
        QL_FAIL("Need either a Name or Underlying node for InflationUnderlying.");
    }
    setType("Inflation");
}

XMLNode* InflationUnderlying::toXML(XMLDocument& doc) const {
    XMLNode* node;
    if (isBasic_) {
        node = doc.allocNode(basicUnderlyingNodeName_, name_);
    } else {
        node = Underlying::toXML(doc);
        XMLUtils::addChild(doc, node, "Interpolation", std::to_string(interpolation_));
    }
    return node;
}

void BondUnderlying::setBondName() {
    if (bondName_.empty()) {
        if (!identifierType_.empty())
            bondName_ = identifierType_ + ":" + name_;
        else
            bondName_ = name_;
    }
}

void BondUnderlying::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        name_ = XMLUtils::getNodeValue(node);
        isBasic_ = true;
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        Underlying::fromXML(node);
        QL_REQUIRE(type_ == "Bond", "Underlying must be of type 'Bond'.");
        identifierType_ = XMLUtils::getChildValue(node, "IdentifierType", false);
        setBondName();
        isBasic_ = false;
    } else {
        QL_FAIL("Need either a " << basicUnderlyingNodeName_ << " or " << nodeName_ << " for BondUnderlying.");
    }
    bidAskAdjustment_ = XMLUtils::getChildValueAsDouble(node, "BidAskAdjustment", false, 0.0);
    setType("Bond");
}

XMLNode* BondUnderlying::toXML(XMLDocument& doc) const {
    XMLNode* node;
    if (isBasic_) {
        node = doc.allocNode(basicUnderlyingNodeName_, name_);
    } else {
        node = Underlying::toXML(doc);
        if (!identifierType_.empty())
            XMLUtils::addChild(doc, node, "IdentifierType", identifierType_);
    }
    return node;
}

void UnderlyingBuilder::fromXML(XMLNode* node) {
    if (XMLUtils::getNodeName(node) == basicUnderlyingNodeName_) {
        underlying_ = QuantLib::ext::make_shared<BasicUnderlying>();
    } else if (XMLUtils::getNodeName(node) == nodeName_) {
        string type = XMLUtils::getChildValue(node, "Type", true);
        if (type == "Equity")
            underlying_ = QuantLib::ext::make_shared<EquityUnderlying>();
        else if (type == "Commodity")
            underlying_ = QuantLib::ext::make_shared<CommodityUnderlying>();
        else if (type == "FX")
            underlying_ = QuantLib::ext::make_shared<FXUnderlying>();
        else if (type == "InterestRate")
            underlying_ = QuantLib::ext::make_shared<InterestRateUnderlying>();
        else if (type == "Inflation")
            underlying_ = QuantLib::ext::make_shared<InflationUnderlying>();
        else if (type == "Credit")
            underlying_ = QuantLib::ext::make_shared<CreditUnderlying>();
        else if (type == "Bond")
            underlying_ = QuantLib::ext::make_shared<BondUnderlying>();
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

XMLNode* UnderlyingBuilder::toXML(XMLDocument& doc) const { return NULL; }

} // namespace data
} // namespace ore
