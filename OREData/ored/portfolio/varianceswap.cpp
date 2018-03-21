/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2012 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! 
  \file qlw/trades/varswap.cpp  
  \brief Variance Swap data model
  \ingroup Wrap 
*/
#include <ored/portfolio/builders/varianceswap.hpp>
#include <ored/portfolio/varianceswap.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/instruments/varianceswap.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

//--------------------------------------------------------------------------
void VarSwap::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    //--------------------------------------------------------------------------
    Currency ccy = parseCurrency(currency_);
    Position::Type longShort = parsePositionType(longShort_);
    Date startDate = parseDate(startDate_);
    Date endDate = parseDate(endDate_);

    QL_REQUIRE(strike_ > 0, "VarSwap::build() invalid strike " << strike_);
    QL_REQUIRE(notional_ > 0, "VarSwap::build() invalid notional " << notional_);

    // Input strike is annualised Vol
    // QL::varianceStrike needs a annualised variance.
    Real varianceStrike = strike_ * strike_;

    boost::shared_ptr<VarianceSwap> varSwap(new VarianceSwap(longShort, varianceStrike, notional_, startDate, endDate));

    // Pricing Engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<VarSwapEngineBuilder> varSwapBuilder =
        boost::dynamic_pointer_cast<VarSwapEngineBuilder>(builder);

    varSwap->setPricingEngine(varSwapBuilder->engine(eqName_, ccy));

    // set up other Trade details
    instrument_ = boost::shared_ptr<InstrumentWrapper>(new VanillaInstrument(varSwap));
        
    npvCurrency_ = currency_;
    maturity_ = endDate;
}

void VarSwap::fromXML(XMLNode *node) {
    Trade::fromXML(node);
    XMLNode* vNode = XMLUtils::getChildNode(node, "VarSwapData");
    longShort_ = XMLUtils::getChildValue(vNode, "LongShort", true);
    eqName_ = XMLUtils::getChildValue(vNode, "Name", true);
    currency_ = XMLUtils::getChildValue(vNode, "Currency", true);
    notional_ = XMLUtils::getChildValueAsDouble(vNode, "Notional", true);
    strike_ = XMLUtils::getChildValueAsDouble(vNode, "Strike", true);
    startDate_ = XMLUtils::getChildValue(vNode, "StartDate", true);
    endDate_ = XMLUtils::getChildValue(vNode, "EndDate", true);
}

XMLNode* VarSwap::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* vNode = doc.allocNode("VarSwapData");
    XMLUtils::appendNode(node, vNode);
    XMLUtils::addChild(doc, vNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, vNode, "Name", eqName_);
    XMLUtils::addChild(doc, vNode, "Currency", currency_);
    XMLUtils::addChild(doc, vNode, "Strike", strike_);
    XMLUtils::addChild(doc, vNode, "Notional", notional_);
    XMLUtils::addChild(doc, vNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, vNode, "EndDate", endDate_);
    return node;
}

}
}