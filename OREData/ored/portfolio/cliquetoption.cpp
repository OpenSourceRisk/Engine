/*
  Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/cliquetoption.hpp>
#include <ored/portfolio/cliquetoption.hpp>
#include <qle/instruments/cliquetoption.hpp>

#include <ored/portfolio/referencedata.hpp>

using namespace ore::data;
using namespace QuantLib;
using namespace std;

namespace {

Real getRealOrNull(XMLNode* node, const string& name) {
    string tmp = XMLUtils::getChildValue(node, name, false);
    if (tmp == "")
        return Null<Real>();
    else
        return parseReal(tmp);
}

} // anonymous namespace

namespace ore {
namespace data {

void CliquetOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    // ISDA taxonomy
    if (underlying_->type() == "Equity") {
        additionalData_["isdaAssetClass"] = string("Equity");
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    } else if (underlying_->type() == "Commodity") {
        // assuming that Commoditiy is treated like Equity
        additionalData_["isdaAssetClass"] = string("Commodity");
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    } else if (underlying_->type() == "FX") {
        additionalData_["isdaAssetClass"] = string("Foreign Exchange");
        additionalData_["isdaBaseProduct"] = string("Complex Exotic");
        additionalData_["isdaSubProduct"] = string("Generic");
    } else {
        WLOG("ISDA taxonomy not set for trade " << id());
    }

    additionalData_["isdaTransaction"] = string("");  

    Currency ccy = parseCurrency(currency_);

    QL_REQUIRE(tradeActions().empty(), "TradeActions not supported for VanillaOption");

    // Payoff
    Option::Type type = parseOptionType(callPut_);
    QuantLib::ext::shared_ptr<PercentageStrikePayoff> payoff = QuantLib::ext::make_shared<PercentageStrikePayoff>(type, moneyness_);

    Schedule schedule;
    schedule = makeSchedule(scheduleData_);
    Date expiryDate = schedule.dates().back();

    // Vanilla European/American.
    // If price adjustment is necessary we build a simple EU Option
    QuantLib::ext::shared_ptr<QuantLib::Instrument> instrument;
    QuantLib::ext::shared_ptr<EuropeanExercise> exerciseDate = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);

    Date paymentDate = schedule.calendar().advance(expiryDate, settlementDays_, Days);

    for (auto d : schedule.dates())
        valuationDates_.insert(schedule.calendar().adjust(d, schedule.businessDayConvention()));

    Position::Type position = parsePositionType(longShort_);

    // Create QuantExt Cliquet Option
    QuantLib::ext::shared_ptr<Instrument> cliquet = QuantLib::ext::make_shared<QuantExt::CliquetOption>(
        payoff, exerciseDate, valuationDates_, paymentDate, cliquetNotional_, position, localCap_, localFloor_,
        globalCap_, globalFloor_, premium_, parseDate(premiumPayDate_), premiumCcy_);

    QuantLib::ext::shared_ptr<EngineBuilder> builder = engineFactory->builder(tradeType_);
    QL_REQUIRE(builder, "No builder found for " << tradeType_);
    QuantLib::ext::shared_ptr<CliquetOptionEngineBuilder> cliquetOptionBuilder =
        QuantLib::ext::dynamic_pointer_cast<CliquetOptionEngineBuilder>(builder);

    cliquet->setPricingEngine(cliquetOptionBuilder->engine(name(), ccy));
    setSensitivityTemplate(*cliquetOptionBuilder);

    instrument_ = QuantLib::ext::shared_ptr<InstrumentWrapper>(new VanillaInstrument(cliquet));

    npvCurrency_ = currency_;
    maturity_ = expiryDate;
    notional_ = cliquetNotional_;
    notionalCurrency_ = currency_;

    // add required fixings (all valuation dates)
    for (auto const& d : valuationDates_) {
        requiredFixings_.addFixingDate(d, "EQ-" + name(), paymentDate);
    }

    additionalData_["notional"] = cliquetNotional_;
}

void CliquetOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);

    XMLNode* clNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(clNode, "No EquityCliquetOptionData Node");

    XMLNode* tmp = XMLUtils::getChildNode(clNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(node, "Name");
    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    currency_ = XMLUtils::getChildValue(clNode, "Currency", true);
    cliquetNotional_ = XMLUtils::getChildValueAsDouble(clNode, "Notional", true);
    longShort_ = XMLUtils::getChildValue(clNode, "LongShort", true);
    callPut_ = XMLUtils::getChildValue(clNode, "OptionType", true);
    XMLNode* scheduleNode = XMLUtils::getChildNode(clNode, "ScheduleData");
    scheduleData_.fromXML(scheduleNode);
    moneyness_ = XMLUtils::getChildValueAsDouble(clNode, "Moneyness", false);
    localCap_ = getRealOrNull(clNode, "LocalCap");
    localFloor_ = getRealOrNull(clNode, "LocalFloor");
    globalCap_ = getRealOrNull(clNode, "GlobalCap");
    globalFloor_ = getRealOrNull(clNode, "GlobalFloor");
    settlementDays_ = XMLUtils::getChildValueAsInt(clNode, "SettlementDays", false);
    premium_ = XMLUtils::getChildValueAsDouble(clNode, "Premium", false);
    premiumCcy_ = XMLUtils::getChildValue(clNode, "PremiumCurrency", false);
    premiumPayDate_ = XMLUtils::getChildValue(clNode, "PremiumPaymentDate", false);
}

XMLNode* CliquetOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* clNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, clNode);
    XMLUtils::appendNode(clNode, underlying_->toXML(doc));
    XMLUtils::addChild(doc, clNode, "Currency", currency_);
    XMLUtils::addChild(doc, clNode, "Notional", cliquetNotional_);
    XMLUtils::addChild(doc, clNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, clNode, "OptionType", callPut_);
    XMLUtils::appendNode(clNode, scheduleData_.toXML(doc));
    if (moneyness_ != Null<Real>())
        XMLUtils::addChild(doc, clNode, "Moneyness", moneyness_);
    if (localCap_ != Null<Real>())
        XMLUtils::addChild(doc, clNode, "LocalCap", localCap_);
    if (localFloor_ != Null<Real>())
        XMLUtils::addChild(doc, clNode, "LocalFloor", localFloor_);
    if (globalCap_ != Null<Real>())
        XMLUtils::addChild(doc, clNode, "GlobalCap", globalCap_);
    if (globalFloor_ != Null<Real>())
        XMLUtils::addChild(doc, clNode, "GlobalFloor", globalFloor_);
    if (settlementDays_ != Null<Size>())
        XMLUtils::addChild(doc, clNode, "SettlementDays", static_cast<int>(settlementDays_));
    if (premium_ != Null<Real>())
        XMLUtils::addChild(doc, clNode, "Premium", premium_);
    if (premiumCcy_ != "")
        XMLUtils::addChild(doc, clNode, "PremiumCurrency", premiumCcy_);
    if (premiumPayDate_ != "")
        XMLUtils::addChild(doc, clNode, "PremiumPaymentDate", premiumPayDate_);
    return node;
}

} // namespace data
} // namespace ore
