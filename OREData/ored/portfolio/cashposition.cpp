/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <ored/portfolio/cashposition.hpp>
#include <qle/instruments/cashposition.hpp>
#include <qle/pricingengines/cashpositionengine.hpp>

namespace ore {
namespace data {

void CashPosition::build(const QuantLib::ext::shared_ptr<EngineFactory>& engine) {
    // ISDA taxonomy 
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Spot");
    additionalData_["isdaSubProduct"] = string("");
    additionalData_["isdaTransaction"] = string("");
    
    Currency ccy = parseCurrencyWithMinors(currency_);
    Real amountMajor = convertMinorToMajorCurrency(currency_, amount_);

    QuantLib::ext::shared_ptr<Instrument> inst =
        QuantLib::ext::make_shared<QuantExt::CashPosition>(amountMajor);

    // set up other Trade details
    instrument_ = QuantLib::ext::make_shared<InstrumentWrapper>(new VanillaInstrument(inst));
    npvCurrency_ = ccy.code();

    maturity_ = Date::maxDate();
    notional_ = amountMajor;
    notionalCurrency_ = ccy.code();

    // Pricing engine - set pricing engine directly, no need for a builder as no logic in pricing engine
    QuantLib::ext::shared_ptr<QuantExt::CashPositionEngine> pricingEngine =
        QuantLib::ext::make_shared<QuantExt::CashPositionEngine>();
    inst->setPricingEngine(pricingEngine);

    setSensitivityTemplate("");
}

void CashPosition::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* cpNode = XMLUtils::getChildNode(node, "CashPositionData");
    QL_REQUIRE(cpNode, "No CashPositionData Node");
    currency_ = XMLUtils::getChildValue(cpNode, "Currency", true);
    amount_ = XMLUtils::getChildValueAsDouble(cpNode, "Amount", true);
}

XMLNode* CashPosition::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* cpNode = doc.allocNode("CashPositionData");
    XMLUtils::appendNode(node, cpNode);
    XMLUtils::addChild(doc, cpNode, "Currency", currency_);
    XMLUtils::addChild(doc, cpNode, "Amount", amount_);
    return node;
}

} // namespace data
} // namespace ore