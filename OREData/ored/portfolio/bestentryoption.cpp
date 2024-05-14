/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <ored/portfolio/builders/scriptedtrade.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/portfolio/bestentryoption.hpp>
#include <ored/scripting/utilities.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

void BestEntryOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    // clang-format off

    static const std::string script =
        "NUMBER payoff, initialIndex, triggerEvent, strikeIndexObs, strikeIndexLevel, d, resetMinValue;\n"
        "\n"
        "triggerEvent = 0;\n"
        "\n"
        "strikeIndexLevel = Underlying(StrikeDate);\n"
        "\n"
        "resetMinValue = strikeIndexLevel * ResetMinimum;\n"
        "\n"
        "strikeIndexObs = Underlying(StrikeObservationDates[1]);\n"
        "\n"
        "FOR d IN (1, SIZE(StrikeObservationDates), 1) DO\n"
        "  IF Underlying(StrikeObservationDates[d]) < TriggerLevel * strikeIndexLevel THEN\n"
        "    triggerEvent = 1;\n"
        "    strikeIndexObs = min(strikeIndexObs, Underlying(StrikeObservationDates[d]));\n"
        "  END;\n"
        "END;\n"
        "\n"
        "IF triggerEvent == 1 THEN\n"
        "  initialIndex = max(ResetMinimum * strikeIndexLevel, strikeIndexObs);\n"
        "ELSE\n"
        "  initialIndex = strikeIndexLevel;\n"
        "END;\n"
        "\n"
        "IF Underlying(ExpiryDate) > Strike * initialIndex THEN\n"
        "  payoff = LongShort * Notional * Multiplier * min(Cap, max(0, (Underlying(ExpiryDate) - initialIndex)/initialIndex));\n"
        "ELSE\n"
        "  payoff = -1* LongShort * Notional * (Strike * initialIndex - Underlying(ExpiryDate))/initialIndex;\n"
        "END;\n"
        "\n"
        "Option = PAY(payoff, ExpiryDate, SettlementDate, Currency) - PAY(Premium, PremiumDate, PremiumDate, Currency);\n";

    // clang-format on

    numbers_.emplace_back("Number", "Notional", notional_);
    numbers_.emplace_back("Number", "Multiplier", !multiplier_.empty() ? multiplier_ : "1");
    numbers_.emplace_back("Number", "Cap", cap_);
    numbers_.emplace_back("Number", "ResetMinimum", resetMinimum_);

    numbers_.emplace_back("Number", "Strike", strike_);

    numbers_.emplace_back("Number", "LongShort", longShort_ == "Long" ? "1" : "-1");

    numbers_.emplace_back("Number", "TriggerLevel", triggerLevel_);

    events_.emplace_back("ExpiryDate", expiryDate_);
    QL_REQUIRE(parseDate(expiryDate_) <= parseDate(settlementDate_),
               "Expiry date must be before settlement date");

    currencies_.emplace_back("Currency", "Currency", currency_);
    numbers_.emplace_back("Number", "Premium", !premium_.empty() ? premium_ : "0");
    events_.emplace_back("PremiumDate", !premiumDate_.empty() ? premiumDate_ : settlementDate_);

    events_.emplace_back("SettlementDate", settlementDate_);
    events_.emplace_back("StrikeDate", strikeDate_);
    QL_REQUIRE(parseDate(strikeDate_) < parseDate(expiryDate_), "Strike date must be before expiry date");

    if (observationDates_.hasData()) {
        events_.emplace_back("StrikeObservationDates", observationDates_);
    }

    // set product tag

    productTag_ = "SingleAssetOption({AssetClass})";

    // set script
    
    script_[""] = ScriptedTradeScriptData(script, "Option", 
        {{"initialIndex", "initialIndex"}, {"strikeIndexLevel", "strikeIndexLevel"}, 
        {"payoffAmount", "payoff"}, {"resetMinimumValue", "resetMinValue"},
                                           {"lowestStrikeObs", "strikeIndexObs"},
                                           {"Cap", "Cap"},
                                           {"TriggerEvent", "triggerEvent"}}, 
        {});

    // build trade

    ScriptedTrade::build(factory);
}

void BestEntryOption::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // asset class set in the base class already
    std::string assetClass = boost::any_cast<std::string>(additionalData_["isdaAssetClass"]);
    if (assetClass == "Equity") {
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    } else if (assetClass == "Commodity") {
        // isda taxonomy missing for this class, using the same as equity
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    } else if (assetClass == "Foreign Exchange") {
        additionalData_["isdaBaseProduct"] = string("Complex Exotic");
        additionalData_["isdaSubProduct"] = string("Generic");
    } else {
        WLOG("ISDA taxonomy incomplete for trade " << id());
    }
    additionalData_["isdaTransaction"] = string("");
}

void BestEntryOption::initIndices() { indices_.emplace_back("Index", "Underlying", scriptedIndexName(underlying_)); }

void BestEntryOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(tradeDataNode, "BestEntryOptionData node not found");
    longShort_ = XMLUtils::getChildValue(tradeDataNode, "LongShort", true);
    notional_ = XMLUtils::getChildValue(tradeDataNode, "Notional", true);
    multiplier_ = XMLUtils::getChildValue(tradeDataNode, "Multiplier", false);
    strike_ = XMLUtils::getChildValue(tradeDataNode, "Strike", true);
    cap_ = XMLUtils::getChildValue(tradeDataNode, "Cap", true);
    triggerLevel_ = XMLUtils::getChildValue(tradeDataNode, "TriggerLevel", true);
    resetMinimum_ = XMLUtils::getChildValue(tradeDataNode, "ResetMinimum", true);
    currency_ = XMLUtils::getChildValue(tradeDataNode, "Currency", true);

    XMLNode* underlying = XMLUtils::getChildNode(tradeDataNode, "Underlying");
    QL_REQUIRE(underlying, "Underlying node not found");

    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(underlying);
    underlying_ = underlyingBuilder.underlying();

    XMLNode* observationNode = XMLUtils::getChildNode(tradeDataNode, "StrikeObservationDates");
    QL_REQUIRE(observationNode, "No strike observation dates provided");
    observationDates_.fromXML(observationNode);

    expiryDate_ = XMLUtils::getChildValue(tradeDataNode, "ExpiryDate", true);
    settlementDate_ = XMLUtils::getChildValue(tradeDataNode, "SettlementDate");
    strikeDate_ = XMLUtils::getChildValue(tradeDataNode, "StrikeDate", true);
    premium_ = XMLUtils::getChildValue(tradeDataNode, "Premium", false);
    premiumDate_ = XMLUtils::getChildValue(tradeDataNode, "PremiumDate", false);
    currency_ = XMLUtils::getChildValue(tradeDataNode, "Currency", true);

    initIndices();
}

XMLNode* BestEntryOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, tradeNode);
    XMLUtils::addChild(doc, tradeNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, tradeNode, "Notional", notional_);
    XMLUtils::addChild(doc, tradeNode, "Multiplier", multiplier_);
    XMLUtils::addChild(doc, tradeNode, "Strike", strike_);
    XMLUtils::addChild(doc, tradeNode, "Cap", cap_);
    XMLUtils::addChild(doc, tradeNode, "TriggerLevel", triggerLevel_);
    XMLUtils::addChild(doc, tradeNode, "ResetMinimum", resetMinimum_);

    XMLUtils::appendNode(tradeNode, underlying_->toXML(doc));

    XMLNode* observationNode = observationDates_.toXML(doc);
    XMLUtils::setNodeName(doc, observationNode, "StrikeObservationDates");
    XMLUtils::appendNode(tradeNode, observationNode);

    XMLUtils::addChild(doc, tradeNode, "ExpiryDate", expiryDate_);
    XMLUtils::addChild(doc, tradeNode, "StrikeDate", strikeDate_);
    XMLUtils::addChild(doc, tradeNode, "Currency", currency_);
    XMLUtils::addChild(doc, tradeNode, "Premium", premium_);
    XMLUtils::addChild(doc, tradeNode, "PremiumDate", premiumDate_);
    XMLUtils::addChild(doc, tradeNode, "SettlementDate", settlementDate_);

    return node;
}

} // namespace data
} // namespace ore
