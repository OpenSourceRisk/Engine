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

#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/builders/equityoption.hpp>
#include <ored/portfolio/builders/equitydigitaloption.hpp>
#include <ored/portfolio/equityeuropeanbarrieroption.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/equityoption.hpp>
#include <ored/utilities/log.hpp>

#include <ql/errors.hpp>
#include <ql/exercise.hpp>
#include <ql/instruments/barriertype.hpp>
#include <ql/instruments/compositeinstrument.hpp>
#include <ql/instruments/vanillaoption.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void EquityEuropeanBarrierOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    additionalData_["isdaAssetClass"] = string("Equity");
    additionalData_["isdaBaseProduct"] = string("Other");
    additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    // skip the transaction level mapping for now
    additionalData_["isdaTransaction"] = string("");

    const QuantLib::ext::shared_ptr<Market> market = engineFactory->market();

    // Only European Single Barrier supported for now
    QL_REQUIRE(option_.style() == "European", "Option Style unknown: " << option_.style());
    QL_REQUIRE(option_.exerciseDates().size() == 1, "Invalid number of exercise dates");
    QL_REQUIRE(barrier_.levels().size() == 1, "Invalid number of barrier levels");
    QL_REQUIRE(barrier_.style().empty() || barrier_.style() == "European", "Only european barrier style suppported");
    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for FxEuropeanBarrierOption");

    assetName_ = equityName();

    Currency ccy = parseCurrencyWithMinors(currency_);
    // Set the strike currency - if we have a minor currency, convert the strike
    if (!strikeCurrency_.empty())
        strike_.setCurrency(strikeCurrency_);
    else if (strike_.currency().empty())
        strike_.setCurrency(currency_);

    Real level = barrier_.levels()[0].value();
    Real rebate = barrier_.rebate() / quantity_;
    QL_REQUIRE(rebate >= 0, "Rebate must be non-negative");
    
    Option::Type type = parseOptionType(option_.callPut());

    // Exercise
    Date expiryDate = parseDate(option_.exerciseDates().front());
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

    Barrier::Type barrierType = parseBarrierType(barrier_.type());

    // Payoff - European Option with strike K
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoffVanillaK(new PlainVanillaPayoff(type, strike_.value()));
    // Payoff - European Option with strike B
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoffVanillaB(new PlainVanillaPayoff(type, level));
    // Payoff - Digital Option with barrier B payoff abs(B - K)
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoffDigital(new CashOrNothingPayoff(type, level, fabs(level - strike_.value())));

    QuantLib::ext::shared_ptr<Instrument> digital = QuantLib::ext::make_shared<VanillaOption>(payoffDigital, exercise);
    QuantLib::ext::shared_ptr<Instrument> vanillaK = QuantLib::ext::make_shared<VanillaOption>(payoffVanillaK, exercise);
    QuantLib::ext::shared_ptr<Instrument> vanillaB = QuantLib::ext::make_shared<VanillaOption>(payoffVanillaB, exercise);

    QuantLib::ext::shared_ptr<StrikedTypePayoff> rebatePayoff;
    if (barrierType == Barrier::Type::UpIn || barrierType == Barrier::Type::DownOut) {
        // Payoff - Up&Out / Down&In Digital Option with barrier B payoff rebate
        rebatePayoff = QuantLib::ext::make_shared<CashOrNothingPayoff>(Option::Put, level, rebate);
    } else if (barrierType == Barrier::Type::UpOut || barrierType == Barrier::Type::DownIn) {
        // Payoff - Up&In / Down&Out Digital Option with barrier B payoff rebate
        rebatePayoff = QuantLib::ext::make_shared<CashOrNothingPayoff>(Option::Call, level, rebate);
    }
    QuantLib::ext::shared_ptr<Instrument> rebateInstrument = QuantLib::ext::make_shared<VanillaOption>(rebatePayoff, exercise);

    // set pricing engines
    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder("EquityOption");
    QL_REQUIRE(builder, "No builder found for EquityOption");
    QuantLib::ext::shared_ptr<EquityEuropeanOptionEngineBuilder> eqOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<EquityEuropeanOptionEngineBuilder>(builder);

    builder = engineFactory->builder("EquityDigitalOption");
    QL_REQUIRE(builder, "No builder found for EquityDigitalOption");
    QuantLib::ext::shared_ptr<EquityDigitalOptionEngineBuilder> eqDigitalOptBuilder =
        QuantLib::ext::dynamic_pointer_cast<EquityDigitalOptionEngineBuilder>(builder);

    digital->setPricingEngine(eqDigitalOptBuilder->engine(assetName_, ccy));
    vanillaK->setPricingEngine(eqOptBuilder->engine(assetName_, ccy, expiryDate));
    vanillaB->setPricingEngine(eqOptBuilder->engine(assetName_, ccy, expiryDate));
    rebateInstrument->setPricingEngine(eqDigitalOptBuilder->engine(assetName_, ccy));
    setSensitivityTemplate(*eqDigitalOptBuilder);

    QuantLib::ext::shared_ptr<CompositeInstrument> qlInstrument = QuantLib::ext::make_shared<CompositeInstrument>();
    qlInstrument->add(rebateInstrument);
    if (type == Option::Call) {
        if (barrierType == Barrier::Type::UpIn || barrierType == Barrier::Type::DownOut) {
            if (level > strike_.value()) {
                qlInstrument->add(vanillaB);
                qlInstrument->add(digital);
            } else {
                qlInstrument->add(vanillaK);
            }
        } else if (barrierType == Barrier::Type::UpOut || barrierType == Barrier::Type::DownIn) {
            if (level > strike_.value()) {
                qlInstrument->add(vanillaK);
                qlInstrument->add(vanillaB, -1);
                qlInstrument->add(digital, -1);
            } else {
                // empty
            }
        } else {
            QL_FAIL("Unknown Barrier Type: " << barrierType);
        }
    } else if (type == Option::Put) {
        if (barrierType == Barrier::Type::UpIn || barrierType == Barrier::Type::DownOut) {
            if (level > strike_.value()) {
                // empty
            } else {
                qlInstrument->add(vanillaK);
                qlInstrument->add(vanillaB, -1);
                qlInstrument->add(digital, -1);
            }
        } else if (barrierType == Barrier::Type::UpOut || barrierType == Barrier::Type::DownIn) {
            if (level > strike_.value()) {
                qlInstrument->add(vanillaK);
            } else {
                qlInstrument->add(vanillaB);
                qlInstrument->add(digital);
            }
        } else {
            QL_FAIL("Unknown Barrier Type: " << barrierType);
        }
    }

    // Add additional premium payments
    Position::Type positionType = parsePositionType(option_.longShort());
    Real bsInd = (positionType == QuantLib::Position::Long ? 1.0 : -1.0);

    std::vector<QuantLib::ext::shared_ptr<Instrument>> additionalInstruments;
    std::vector<Real> additionalMultipliers;
    Date lastPremiumDate =
        addPremiums(additionalInstruments, additionalMultipliers, quantity_ * bsInd, option_.premiumData(), -bsInd, ccy,
                    engineFactory, eqOptBuilder->configuration(MarketContext::pricing));

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(
        new VanillaInstrument(qlInstrument, quantity_ * bsInd, additionalInstruments, additionalMultipliers));

    npvCurrency_ = ccy.code(); 
    notional_ = strike_.value() * quantity_; 
    notionalCurrency_ = strike_.currency(); 
    maturity_ = std::max(lastPremiumDate, expiryDate);

    additionalData_["quantity"] = quantity_;
    additionalData_["strike"] = strike_.value();
    additionalData_["strikeCurrency"] = strike_.currency();
}

void EquityEuropeanBarrierOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* eqNode = XMLUtils::getChildNode(node, "EquityEuropeanBarrierOptionData");
    QL_REQUIRE(eqNode, "No EquityEuropeanBarrierOptionData Node");
    option_.fromXML(XMLUtils::getChildNode(eqNode, "OptionData"));
    XMLNode* tmp = XMLUtils::getChildNode(eqNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(eqNode, "Name");
    equityUnderlying_.fromXML(tmp);
    currency_ = XMLUtils::getChildValue(eqNode, "Currency", true);
    
    strike_.fromXML(eqNode);
    strikeCurrency_ = XMLUtils::getChildValue(eqNode, "StrikeCurrency", false);
    if (!strikeCurrency_.empty())
        WLOG("EquityOption::fromXML: node StrikeCurrency is deprecated, please us StrikeData node");

    barrier_.fromXML(XMLUtils::getChildNode(eqNode, "BarrierData"));
    quantity_ = XMLUtils::getChildValueAsDouble(eqNode, "Quantity", true);
}

XMLNode* EquityEuropeanBarrierOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* eqNode = doc.allocNode("EquityEuropeanBarrierOptionData");
    XMLUtils::appendNode(node, eqNode);

    XMLUtils::appendNode(eqNode, option_.toXML(doc));
    XMLUtils::appendNode(eqNode, barrier_.toXML(doc));
    XMLUtils::appendNode(eqNode, equityUnderlying_.toXML(doc));
    XMLUtils::addChild(doc, eqNode, "Currency", currency_);
    XMLUtils::appendNode(eqNode, strike_.toXML(doc));
    if (!strikeCurrency_.empty())
        XMLUtils::addChild(doc, eqNode, "StrikeCurrency", strikeCurrency_);

    XMLUtils::addChild(doc, eqNode, "Quantity", quantity_);

    return node;
}
} // namespace data
} // namespace oreplus
