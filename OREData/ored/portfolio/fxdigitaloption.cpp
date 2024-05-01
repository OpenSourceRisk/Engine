/*
  Copyright (C) 2017 Quaternion Risk Management Ltd
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
#include <ored/portfolio/builders/fxoption.hpp>
#include <ored/portfolio/builders/fxdigitaloption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fxdigitaloption.hpp>
#include <ored/utilities/log.hpp>
#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void FxDigitalOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Foreign Exchange");
    additionalData_["isdaBaseProduct"] = string("Simple Exotic");
    additionalData_["isdaSubProduct"] = string("Digital");  
    additionalData_["isdaTransaction"] = string("");  

    additionalData_["payoffAmount"] = payoffAmount_;
    additionalData_["payoffCurrency"] = payoffCurrency_;

    // Only European Vanilla supported for now
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(option_.payoffAtExpiry() == true, "PayoffAtExpiry must be True for FxDigitalOption");
    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxDigitalOption");
    QL_REQUIRE(strike_ > 0.0 && strike_ != Null<Real>(), "Invalid strike " << strike_);

    Currency domCcy = parseCurrency(domesticCurrency_);
    Currency forCcy = parseCurrency(foreignCurrency_);

    // Payoff Type
    Option::Type type = parseOptionType(option_.callPut());

    // Handle PayoffCurrency, we might have to flip the trade here
    Real strike = strike_;
    bool flipResults = false;
    if (payoffCurrency_ == "") {
        DLOG("PayoffCurrency defaulting to " << domesticCurrency_ << " for FxDigitalOption " << id());
    } else if (payoffCurrency_ == foreignCurrency_) {
        // Invert the trade, switch dom and for and flip Put/Call
        strike = 1.0 / strike;
        std::swap(domCcy, forCcy);
        type = type == Option::Call ? Option::Put : Option::Call;
        flipResults = true;
    } else if (payoffCurrency_ != domesticCurrency_) {
        QL_FAIL("Invalid Payoff currency (" << payoffCurrency_ << ") for FxDigitalOption " << forCcy << domCcy);
    }
    DLOG("Setting up FxDigitalOption with strike " << strike << " foreign " << forCcy << " domestic " << domCcy);

    // Set up the CashOrNothing
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff(new CashOrNothingPayoff(type, strike, payoffAmount_));

    npvCurrency_ = domCcy.code(); // don't use domesticCurrency_ as it might be flipped
    notional_ = payoffAmount_;
    notionalCurrency_ = payoffCurrency_ != "" ? payoffCurrency_ : domesticCurrency_; // see logic above

    // Exercise
    Date expiryDate = parseDate(option_.exerciseDates().front());
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);
    maturity_ = std::max(option_.premiumData().latestPremiumDate(), expiryDate);

    // QL does not have an FXDigitalOption, so we add a vanilla one here and wrap
    // it in a composite.
    QuantLib::ext::shared_ptr<Instrument> vanilla = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);

    // set pricing engines
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<FxDigitalOptionEngineBuilder> fxOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<FxDigitalOptionEngineBuilder>(builder);
    vanilla->setPricingEngine(fxOptBuilder->engine(forCcy, domCcy, flipResults));
    setSensitivityTemplate(*fxOptBuilder);

    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);
    Real mult = bsInd;

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    addPremiums(additionalInstruments, additionalMultipliers, mult, option_.premiumData(), -bsInd, domCcy,
                engineFactory, fxOptBuilder->configuration(MarketContext::pricing));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(vanilla, mult, additionalInstruments, additionalMultipliers));
}

void FxDigitalOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* fxNode = XMLUtils::getChildNode(node, "FxDigitalOptionData");
    QL_REQUIRE(fxNode, "No FxDigitalOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(fxNode, "OptionData"));
    strike_ = XMLUtils::getChildValueAsDouble(fxNode, "Strike", true);
    payoffCurrency_ = XMLUtils::getChildValue(fxNode, "PayoffCurrency", false);
    payoffAmount_ = XMLUtils::getChildValueAsDouble(fxNode, "PayoffAmount", true);
    foreignCurrency_ = XMLUtils::getChildValue(fxNode, "ForeignCurrency", true);
    domesticCurrency_ = XMLUtils::getChildValue(fxNode, "DomesticCurrency", true);
}

XMLNode* FxDigitalOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* fxNode = doc.allocNode("FxDigitalOptionData");
    XMLUtils::appendNode(node, fxNode);

    XMLUtils::appendNode(fxNode, option_.toXML(doc));
    XMLUtils::addChild(doc, fxNode, "Strike", strike_);
    XMLUtils::addChild(doc, fxNode, "PayoffCurrency", payoffCurrency_);
    XMLUtils::addChild(doc, fxNode, "PayoffAmount", payoffAmount_);
    XMLUtils::addChild(doc, fxNode, "ForeignCurrency", foreignCurrency_);
    XMLUtils::addChild(doc, fxNode, "DomesticCurrency", domesticCurrency_);

    return node;
}
} // namespace data
} // namespace oreplus
