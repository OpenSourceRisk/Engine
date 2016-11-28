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

#include <ored/portfolio/equityoption.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/errors.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    Currency ccy = parseCurrency(currency_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for EquityOption");

    // Payoff
    boost::shared_ptr<StrikedTypePayoff> payoff;
    if (option_.callPut() == "Call") {
        payoff.reset(new PlainVanillaPayoff(Option::Call, strike_));
    } else if (option_.callPut() == "Put") {
        payoff.reset(new PlainVanillaPayoff(Option::Put, strike_));
    } else {
        QL_FAIL("Option Type unknown: " << option_.callPut());
    }

    // Only European Vanilla supported for now
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of excercise dates");
    Date expiryDate = parseDate(option_.exerciseDates().front());

    // Exercise
    boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(expiryDate);

    // Vanilla European/American.
    // If price adjustment is necessary we build a simple EU Option
    boost::shared_ptr<QuantLib::Instrument> instrument;

    // QL does not have an FXOption, so we add a vanilla one here and wrap
    // it in a composite to get the notional in.
    boost::shared_ptr<Instrument> vanilla = boost::make_shared<VanillaOption>(payoff, exercise);

    // we buy foriegn with domestic(=sold ccy).
    boost::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    boost::shared_ptr<EquityOptionEngineBuilder> eqOptBuilder = boost::dynamic_pointer_cast<EquityOptionEngineBuilder>(builder);

    vanilla->setPricingEngine(eqOptBuilder->engine(eqName_, ccy));

    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = quantity_* bsInd;

    instrument_ = boost::shared_ptr<InstrumentWrapper>(new VanillaInstrument(vanilla, mult));

    npvCurrency_ = currency_;
    maturity_ = expiryDate;
}

void EquityOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "EquityOptionData");
    QL_REQUIRE(fxNode, "No EquityOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    eqName_ = XMLUtils::getChildValue(fxNode, "Name", true);
    currency_ = XMLUtils::getChildValue(fxNode, "Currency", true);
    strike_ = XMLUtils::getChildValueAsDouble(fxNode, "Strike", true);
    quantity_ = XMLUtils::getChildValueAsDouble(fxNode, "Quantity", true);
}

XMLNode* EquityOption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Name", eqName_);
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::addChild(doc, eqNode, "Strike", strike_);
    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}
}
}
