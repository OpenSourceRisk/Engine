/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! 
  \file qlw/trades/equityforward.cpp  
  \brief Equity Forward data model
  \ingroup Wrap 
*/

#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <qle/instruments/equityforward.hpp>
#include <ql/errors.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

void EquityForward::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    Currency ccy = parseCurrency(currency_);
    QuantLib::Position::Type longShort = parsePositionType(longShort_);
    Date maturity = parseDate(maturityDate_);

    boost::shared_ptr<Instrument> inst = 
        boost::make_shared<QuantExt::EquityForward>(
            eqName_,ccy,longShort,quantity_,maturity,strike_);

    // Pricing engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<EquityForwardEngineBuilder> eqFwdBuilder = boost::dynamic_pointer_cast<EquityForwardEngineBuilder>(builder);
    inst->setPricingEngine(eqFwdBuilder->engine(eqName_, ccy));

    // set up other Trade details
    instrument_ = boost::shared_ptr<InstrumentWrapper>(new VanillaInstrument(inst));
    npvCurrency_ = currency_;
    maturity_ = maturity;
}

void EquityForward::fromXML(XMLNode *node) {
    Trade::fromXML(node);
    XMLNode* eNode = XMLUtils::getChildNode(node, "EquityForwardData");

    longShort_ = XMLUtils::getChildValue(eNode, "LongShort", true);
    eqName_ = XMLUtils::getChildValue(eNode, "Name", true);
    currency_ = XMLUtils::getChildValue(eNode, "Currency", true);
    quantity_ = XMLUtils::getChildValueAsDouble(eNode, "Quantity", true);
    maturityDate_ = XMLUtils::getChildValue(eNode, "Maturity", true);
    strike_ = XMLUtils::getChildValueAsDouble(eNode, "Strike", true);
}

XMLNode* EquityForward::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eNode = doc.allocNode("EquityForwardData");
    XMLUtils::appendNode(node, eNode);

    XMLUtils::addChild(doc, eNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, eNode, "Name", eqName_);
    XMLUtils::addChild(doc, eNode, "Currency", currency_);
    XMLUtils::addChild(doc, eNode, "Quantity", quantity_);
    XMLUtils::addChild(doc, eNode, "Maturity", maturityDate_);
    XMLUtils::addChild(doc, eNode, "Strike", strike_);
    return node;
}
}
}
