/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <boost/make_shared.hpp>

#include <ql/errors.hpp>
#include <qle/instruments/commodityforward.hpp>

#include <ored/portfolio/builders/commodityforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/commodityforward.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

CommodityForward::CommodityForward() : Trade("CommodityForward") {}

CommodityForward::CommodityForward(const Envelope& envelope, const string& position, const string& commodityName,
    const string& currency, Real quantity, const string& maturityDate, Real strike) 
    : Trade("CommodityForward", envelope), position_(position), commodityName_(commodityName), currency_(currency), 
      quantity_(quantity), maturityDate_(maturityDate), strike_(strike) {}

void CommodityForward::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    
    // Create the commodity forward instrument
    Currency currency = parseCurrency(currency_);
    Position::Type position = parsePositionType(position_);
    Date maturity = parseDate(maturityDate_);
    boost::shared_ptr<Instrument> commodityForward = boost::make_shared<QuantExt::CommodityForward>(
        commodityName_, currency, position, quantity_, maturity, strike_);

    // Pricing engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<CommodityForwardEngineBuilder> commodityForwardEngineBuilder =
        boost::dynamic_pointer_cast<CommodityForwardEngineBuilder>(builder);
    commodityForward->setPricingEngine(commodityForwardEngineBuilder->engine(commodityName_, currency));

    // set up other Trade details
    instrument_ = boost::make_shared<VanillaInstrument>(commodityForward);
    npvCurrency_ = currency_;
    maturity_ = maturity;
    
    // We really need today's spot to get the correct notional.
    // But rather than having it move around we use strike * quantity
    notional_ = strike_ * quantity_;
}

void CommodityForward::fromXML(XMLNode* node) {
    
    Trade::fromXML(node);
    XMLNode* commodityDataNode = XMLUtils::getChildNode(node, "CommodityForwardData");

    position_ = XMLUtils::getChildValue(commodityDataNode, "Position", true);
    commodityName_ = XMLUtils::getChildValue(commodityDataNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityDataNode, "Currency", true);
    quantity_ = XMLUtils::getChildValueAsDouble(commodityDataNode, "Quantity", true);
    maturityDate_ = XMLUtils::getChildValue(commodityDataNode, "Maturity", true);
    strike_ = XMLUtils::getChildValueAsDouble(commodityDataNode, "Strike", true);
}

XMLNode* CommodityForward::toXML(XMLDocument& doc) {
    
    XMLNode* node = Trade::toXML(doc);
    XMLNode* commodityDataNode = doc.allocNode("CommodityForwardData");
    XMLUtils::appendNode(node, commodityDataNode);

    XMLUtils::addChild(doc, commodityDataNode, "Position", position_);
    XMLUtils::addChild(doc, commodityDataNode, "Name", commodityName_);
    XMLUtils::addChild(doc, commodityDataNode, "Currency", currency_);
    XMLUtils::addChild(doc, commodityDataNode, "Quantity", quantity_);
    XMLUtils::addChild(doc, commodityDataNode, "Maturity", maturityDate_);
    XMLUtils::addChild(doc, commodityDataNode, "Strike", strike_);

    return node;
}

}
}
