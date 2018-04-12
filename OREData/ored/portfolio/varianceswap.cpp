/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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

/*! \file ored/portfolio/varianceswap.hpp
\brief variance swap representation
\ingroup tradedata
*/

#pragma once
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
    Calendar calendar = parseCalendar(calendar_);

    if (calendar.empty()) {
        calendar = parseCalendar(ccy.code());
    }
    
    QL_REQUIRE(strike_ > 0, "VarSwap::build() invalid strike " << strike_);
    QL_REQUIRE(notional_ > 0, "VarSwap::build() invalid notional " << notional_);

    // Input strike is annualised Vol
    // The quantlib strike and notional are in terms of variance, not volatility, so we convert here.
    Real varianceStrike = strike_ * strike_;
    Real varianceNotional = notional_ / (2 * 100 * notional_);

    boost::shared_ptr<VarianceSwap> varSwap(new VarianceSwap(longShort, varianceStrike, varianceNotional, startDate, endDate));

    // Pricing Engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<VarSwapEngineBuilder> varSwapBuilder =
        boost::dynamic_pointer_cast<VarSwapEngineBuilder>(builder);

    varSwap->setPricingEngine(varSwapBuilder->engine(eqName_, ccy, calendar));

    // set up other Trade details
    instrument_ = boost::shared_ptr<InstrumentWrapper>(new VanillaInstrument(varSwap));
        
    npvCurrency_ = currency_;
    maturity_ = endDate;
}

void VarSwap::fromXML(XMLNode *node) {
    Trade::fromXML(node);
    XMLNode* vNode = XMLUtils::getChildNode(node, "VarianceSwapData");
    longShort_ = XMLUtils::getChildValue(vNode, "LongShort", true);
    eqName_ = XMLUtils::getChildValue(vNode, "Name", true);
    currency_ = XMLUtils::getChildValue(vNode, "Currency", true);
    notional_ = XMLUtils::getChildValueAsDouble(vNode, "Notional", true);
    strike_ = XMLUtils::getChildValueAsDouble(vNode, "Strike", true);
    startDate_ = XMLUtils::getChildValue(vNode, "StartDate", true);
    endDate_ = XMLUtils::getChildValue(vNode, "EndDate", true);
    calendar_ = XMLUtils::getChildValue(vNode, "Calendar", true);
}

XMLNode* VarSwap::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* vNode = doc.allocNode("VarianceSwapData");
    XMLUtils::appendNode(node, vNode);
    XMLUtils::addChild(doc, vNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, vNode, "Name", eqName_);
    XMLUtils::addChild(doc, vNode, "Currency", currency_);
    XMLUtils::addChild(doc, vNode, "Strike", strike_);
    XMLUtils::addChild(doc, vNode, "Notional", notional_);
    XMLUtils::addChild(doc, vNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, vNode, "EndDate", endDate_);
    XMLUtils::addChild(doc, vNode, "Calendar", calendar_);
    return node;
}

}
}