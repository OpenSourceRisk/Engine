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

#include <boost/make_shared.hpp>
#include <ored/portfolio/builders/equitydigitaloption.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/equitydigitaloption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityDigitalOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Option");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");

    // Only European Vanilla supported for now
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(option_.payoffAtExpiry() == true, "PayoffAtExpiry must be True for EquityDigitalOption");
    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for EquityDigitalOption");
    QL_REQUIRE(strike_ > 0.0 && strike_ != Null<Real>(), "Invalid strike " << strike_);
    QL_REQUIRE(payoffAmount_ > 0.0 && payoffAmount_ != Null<Real>(), "Invalid payoff amount " << payoffAmount_);
    QL_REQUIRE(payoffCurrency_ != "", "PayoffCurrency is missing");

    // Currency 
    Currency ccy = parseCurrency(payoffCurrency_);

    // Asset Name
    string assetName = equityName();

    // Payoff Type
    Option::Type type = parseOptionType(option_.callPut());

    // Set up the CashOrNothing
    Real strike = strike_;
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(type, strike, payoffAmount_));

    // Exercise
    Date expiryDate = parseDate(option_.exerciseDates().front());
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

    // QL does not have an EquityDigitalOption, so we add a vanilla one here and wrap
    // it in a composite.
    QuantLib::ext::shared_ptr<Instrument> vanilla = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);

    // set pricing engines
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<EquityDigitalOptionEngineBuilder> eqOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<EquityDigitalOptionEngineBuilder>(builder);
    vanilla->setPricingEngine(eqOptBuilder->engine(assetName, ccy));
    setSensitivityTemplate(*eqOptBuilder);

    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = bsInd;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Date lastPremiumDate =
        addPremiums(additionalInstruments, additionalMultipliers, mult * quantity_, option_.premiumData(), -bsInd, ccy,
                    engineFactory, eqOptBuilder->configuration(MarketContext::pricing));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(vanilla, mult*quantity_, additionalInstruments, additionalMultipliers));

    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_; 
    npvCurrency_ = payoffCurrency_;
    maturity_ = std::max(lastPremiumDate, expiryDate);

    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;
}

void EquityDigitalOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityDigitalOptionData");
    QL_REQUIRE(eqNode, "No EquityDigitalOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    strike_ = XMLUtils::getChildValueAsDouble(eqNode, "Strike", true);
    payoffCurrency_ = XMLUtils::getChildValue(eqNode, "PayoffCurrency", true);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(eqNode, "PayoffAmount", true);
    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);
}

XMLNode* EquityDigitalOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityDigitalOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Strike", strike_);
    XMLUtils::addChild(doc, eqNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, eqNode, "PayoffAmount", payoffAmount_);
    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}
} // namespace data
} // namespace oreplus
