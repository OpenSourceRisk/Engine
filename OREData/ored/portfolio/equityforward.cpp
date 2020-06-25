/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*!
  \file qlw/trades/equityforward.cpp
  \brief Equity Forward data model
  \ingroup portfolio
*/

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/equityforward.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/equityforward.hpp>
#include <ored/portfolio/referencedata.hpp>
#include <ql/errors.hpp>
#include <qle/instruments/equityforward.hpp>

using namespace QuantLib;
using namespace std;

namespace ore {
namespace data {

void EquityForward::build(const boost::shared_ptr<EngineFactory>& engineFactory) {
    Currency ccy = parseCurrency(currency_);
    QuantLib::Position::Type longShort = parsePositionType(longShort_);
    Date maturity = parseDate(maturityDate_);

    string name = eqName();

    boost::shared_ptr<Instrument> inst =
        boost::make_shared<QuantExt::EquityForward>(name, ccy, longShort, quantity_, maturity, strike_);

    // Pricing engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<EquityForwardEngineBuilder> eqFwdBuilder =
        boost::dynamic_pointer_cast<EquityForwardEngineBuilder>(builder);
    inst->setPricingEngine(eqFwdBuilder->engine(name, ccy));

    // set up other Trade details
    instrument_ = boost::shared_ptr<InstrumentWrapper>(new VanillaInstrument(inst));
    npvCurrency_ = currency_;
    maturity_ = maturity;
    // Notional - we really need todays spot to get the correct notional.
    // But rather than having it move around we use strike * quantity
    notional_ = strike_ * quantity_;
    notionalCurrency_ = currency_;
}

void EquityForward::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* eNode = XMLUtils::getChildNode(node, "EquityForwardData");

    longShort_ = XMLUtils::getChildValue(eNode, "LongShort", true);
    maturityDate_ = XMLUtils::getChildValue(eNode, "Maturity", true);
    XMLNode* tmp = XMLUtils::getChildNode(eNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eNode, "Name");
    equityUnderlying_.fromXML(tmp);
    currency_ = XMLUtils::getChildValue(eNode, "Currency", true);
    strike_ = XMLUtils::getChildValueAsDouble(eNode, "Strike", true);
    quantity_ = XMLUtils::getChildValueAsDouble(eNode, "Quantity", true);
}

XMLNode* EquityForward::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eNode = doc.allocNode("EquityForwardData");
    XMLUtils::appendNode(node, eNode);

    XMLUtils::addChild(doc, eNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, eNode, "Maturity", maturityDate_);
    XMLUtils::appendNode(eNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eNode, "Currency", currency_);
    XMLUtils::addChild(doc, eNode, "Strike", strike_);
    XMLUtils::addChild(doc, eNode, "Quantity", quantity_);
    return node;
}

std::map<AssetClass, std::set<std::string>> EquityForward::underlyingIndices() const {
    return {{AssetClass::EQ, std::set<std::string>({eqName()})}};
}

} // namespace data
} // namespace ore
