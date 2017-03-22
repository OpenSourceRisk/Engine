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

#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/errors.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FxOption::build(const boost::shared_ptr<EngineFactory>& engineFactory) {

    Currency boughtCcy = parseCurrency(boughtCurrency_);
    Currency soldCcy = parseCurrency(soldCurrency_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxOption");

    // Payoff
    Real strike = soldAmount_ / boughtAmount_;
    Option::Type type = parseOptionType(option_.callPut());
    boost::shared_ptr<StrikedTypePayoff> payoff (new PlainVanillaPayoff(type, strike));

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
    boost::shared_ptr<FxOptionEngineBuilder> fxOptBuilder = boost::dynamic_pointer_cast<FxOptionEngineBuilder>(builder);

    vanilla->setPricingEngine(fxOptBuilder->engine(boughtCcy, soldCcy));

    instrument_ = boost::shared_ptr<InstrumentWrapper>(new VanillaInstrument(vanilla, boughtAmount_));

    npvCurrency_ = soldCurrency_; // sold is the domestic
    notional_ = soldAmount_;
    maturity_ = expiryDate;
}

void FxOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxOptionData");
    QL_REQUIRE(fxNode, "No FxOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    boughtCurrency_ = XMLUtils::getChildValue(fxNode, "BoughtCurrency", true);
    soldCurrency_ = XMLUtils::getChildValue(fxNode, "SoldCurrency", true);
    boughtAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "BoughtAmount", true);
    soldAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "SoldAmount", true);
}

XMLNode* FxOption::toXML(XMLDocument& doc) {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxOptionData");
    XMLUtils::appendNode(node, fxNode);

    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::addChild(doc, fxNode, "BoughtCurrency", boughtCurrency_);
    XMLUtils::addChild(doc, fxNode, "BoughtAmount", boughtAmount_);
    XMLUtils::addChild(doc, fxNode, "SoldCurrency", soldCurrency_);
    XMLUtils::addChild(doc, fxNode, "SoldAmount", soldAmount_);

    return node;
}
}
}
