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

void EquityForward::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Forward");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");

    additionalData_["strikeCurrency"] = strikeCurrency_;
    additionalData_["quantity"] = quantity_;

    Currency ccy = parseCurrencyWithMinors(currency_);

    // convert strike to the major currency if needed
    Real strike;
    if (!strikeCurrency_.empty()) {
        strike = convertMinorToMajorCurrency(strikeCurrency_, strike_);
        // ensure strike currency matches equity currency
    } else {        
        WLOG("No Strike Currency provide for trade " << id() << ", assuming trade currency " << ccy);
        strike = convertMinorToMajorCurrency(currency_, strike_);
    }
    additionalData_["strike"] = strike;

    QuantLib::Position::Type longShort = parsePositionType(longShort_);
    Date maturity = parseDate(maturityDate_);

    string name = eqName();
    additionalData_["underlyingSecurityId"] = name;

    QuantLib::ext::shared_ptr<Instrument> inst =
        QuantLib::ext::make_shared<QuantExt::EquityForward>(name, ccy, longShort, quantity_, maturity, strike);

    // set up other Trade details
    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(new VanillaInstrument(inst));
    npvCurrency_ = ccy.code();
    maturity_ = maturity;
    // Notional - we really need todays spot to get the correct notional.
    // But rather than having it move around we use strike * quantity
    notional_ = strike * quantity_;
    notionalCurrency_ = ccy.code();

    // We check the ccys here at the end of the build to ensure that the rest of the build above
    // (which does not require the market) runs first

    // get the equity currency from the market
    Currency equityCcy = engineFactory->market()->equityCurve(eqName())->currency();

    // ensure forward currency matches the equity currency
    QL_REQUIRE(!equityCcy.empty(), "No equity currency in equityCurve for equity " << eqName());
    QL_REQUIRE(ccy == equityCcy, "EquityForward currency " << ccy << " does not match equity currency " << equityCcy << " for trade " << id());
    
    if (!strikeCurrency_.empty()) {
        Currency strikeCcy = parseCurrencyWithMinors(strikeCurrency_);
        QL_REQUIRE(strikeCcy == equityCcy, "Strike currency " << ccy << " does not match equity currency " << equityCcy << " for trade " << id());
    }

    // Pricing engine
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<EquityForwardEngineBuilder> eqFwdBuilder =
        QuantLib::ext::dynamic_pointer_cast<EquityForwardEngineBuilder>(builder);
    inst->setPricingEngine(eqFwdBuilder->engine(name, ccy));
    setSensitivityTemplate(*eqFwdBuilder);
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
    strikeCurrency_ = XMLUtils::getChildValue(eNode, "StrikeCurrency", false);
    quantity_ = XMLUtils::getChildValueAsDouble(eNode, "Quantity", true);
}

XMLNode* EquityForward::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eNode = doc.allocNode("EquityForwardData");
    XMLUtils::appendNode(node, eNode);

    XMLUtils::addChild(doc, eNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, eNode, "Maturity", maturityDate_);
    XMLUtils::appendNode(eNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eNode, "Currency", currency_);
    XMLUtils::addChild(doc, eNode, "Strike", strike_);
    if (!strikeCurrency_.empty())
        XMLUtils::addChild(doc, eNode, "StrikeCurrency", strikeCurrency_);
    XMLUtils::addChild(doc, eNode, "Quantity", quantity_);
    return node;
}

std::map<AssetClass, std::set<std::string>>
EquityForward::underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager) const {
    return {{AssetClass::EQ, std::set<std::string>({eqName()})}};
}

} // namespace data
} // namespace ore
