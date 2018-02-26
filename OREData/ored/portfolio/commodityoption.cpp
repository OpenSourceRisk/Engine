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

#include <ored/portfolio/commodityoption.hpp>

#include <boost/make_shared.hpp>

#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

#include <ored/portfolio/builders/commodityoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/utilities/log.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace data {

CommodityOption::CommodityOption(const Envelope& env, const OptionData& optionData, const string& commodityName,
    const string& currency, Real strike, Real quantity)
    : Trade("CommodityOption", env), optionData_(optionData), commodityName_(commodityName), 
      currency_(currency), strike_(strike), quantity_(quantity) {}

void CommodityOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    // Checks
    QL_REQUIRE(tradeActions().empty(), "Trade actions not supported for commodity option");
    QL_REQUIRE(optionData_.style() == "European", "Option style is '" << optionData_.style() << "'. Only European style is supported");
    QL_REQUIRE(optionData_.exerciseDates().size() == 1, "Expect a single exercise date for European option");

    // Build up option trade data
    Currency ccy = parseCurrency(currency_);
    Option::Type type = parseOptionType(optionData_.callPut());
    Date expiryDate = parseDate(optionData_.exerciseDates().front());
    Position::Type positionType = parsePositionType(optionData_.longShort());

    // Create the QuantLib instrument (option for unit of commodity)
    boost::shared_ptr<StrikedTypePayoff> payoff = boost::make_shared<PlainVanillaPayoff>(type, strike_);
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiryDate);
    boost::shared_ptr<Instrument> vanilla = boost::make_shared<VanillaOption>(payoff, exercise);

    // Attach our engine
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<CommodityOptionEngineBuilder> engineBuilder =
        boost::dynamic_pointer_cast<CommodityOptionEngineBuilder>(builder);
    vanilla->setPricingEngine(engineBuilder->engine(commodityName_, ccy));

    // Take care of fees
    vector<boost::shared_ptr<Instrument>> additionalInstruments;
    vector<Real> additionalMultipliers;
    if (optionData_.premiumPayDate() != "" && optionData_.premiumCcy() != "") {
        // Pay premium if long, receive if short
        Real premiumAmount = optionData_.premium();
        if (positionType == Position::Long) premiumAmount *= -1.0;

        Currency premiumCurrency = parseCurrency(optionData_.premiumCcy());
        Date premiumDate = parseDate(optionData_.premiumPayDate());
        
        addPayment(additionalInstruments, additionalMultipliers, premiumDate, premiumAmount, 
            premiumCurrency, ccy, engineFactory, engineBuilder->configuration(MarketContext::pricing));

        DLOG("Option premium added for commodity option " << id());
    }

    // Create the composite instrument wrapper (option trade + fee "trade")
    Real factor = positionType == Position::Long ? quantity_ : -quantity_;
    instrument_ = boost::make_shared<VanillaInstrument>(
        vanilla, factor, additionalInstruments, additionalMultipliers);

    // We just use strike to get notional rather than spot price
    notional_ = strike_ * quantity_;
    
    npvCurrency_ = currency_;
    maturity_ = expiryDate;
}

void CommodityOption::fromXML(XMLNode* node) {
    
    Trade::fromXML(node);

    XMLNode* commodityNode = XMLUtils::getChildNode(node, "CommodityOptionData");
    QL_REQUIRE(commodityNode, "A commodity option needs a 'CommodityOptionData' node");

    optionData_.fromXML(XMLUtils::getChildNode(commodityNode, "OptionData"));
    
    commodityName_ = XMLUtils::getChildValue(commodityNode, "Name", true);
    currency_ = XMLUtils::getChildValue(commodityNode, "Currency", true);
    strike_ = XMLUtils::getChildValueAsDouble(commodityNode, "Strike", true);
    quantity_ = XMLUtils::getChildValueAsDouble(commodityNode, "Quantity", true);
}

XMLNode* CommodityOption::toXML(XMLDocument& doc) {
    
    XMLNode* node = Trade::toXML(doc);
    
    XMLNode* eqNode = doc.allocNode("CommodityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, optionData_.toXML(doc));
    
    XMLUtils::addChild(doc, eqNode, "Name", commodityName_);
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Strike", strike_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}

}
}
