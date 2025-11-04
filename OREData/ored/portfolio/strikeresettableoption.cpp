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
#include <ored/portfolio/strikeresettableoption.hpp>
#include <ored/scripting/utilities.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

void StrikeResettableOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    // clang-format off

    static const std::string script =
        "NUMBER payoff, strike, d, notional;\n"
        "\n"
        "notional = Quantity * ResetStrike;\n"
        "strike = InitialStrike;\n"
        "\n"
        "FOR d IN (1, SIZE(ObservationDates), 1) DO\n"
        "  IF (Underlying(ObservationDates[d]) - TriggerPrice) * TriggerType >= 0 THEN\n"
        "    strike = ResetStrike;\n"
        "  END;\n"
        "END;\n"
        "\n"
        "payoff = Quantity * max(0, (Underlying(ExpiryDate) - strike) * OptionType);"
        "\n"
        "Option = LongShort * (PAY(payoff, ExpiryDate, SettlementDate, Currency) - PAY(Premium, PremiumDate, PremiumDate, Currency));\n";

    // clang-format on

    numbers_.emplace_back("Number", "LongShort", longShort_ == "Long" ? "1" : "-1");
    numbers_.emplace_back("Number", "OptionType", optionType_ == "Call" ? "1" : "-1");
    numbers_.emplace_back("Number", "InitialStrike", strike_);
    numbers_.emplace_back("Number", "ResetStrike", resetStrike_);
    numbers_.emplace_back("Number", "Quantity", quantity_);
    numbers_.emplace_back("Number", "TriggerType", triggerType_ == "Up" ? "1" : "-1");
    numbers_.emplace_back("Number", "TriggerPrice", triggerPrice_);

    currencies_.emplace_back("Currency", "Currency", currency_);

    events_.emplace_back("ExpiryDate", expiryDate_);
    QL_REQUIRE(parseDate(expiryDate_) <= parseDate(settlementDate_),
               "Expiry date must be before settlement date");
    events_.emplace_back("SettlementDate", settlementDate_);

    numbers_.emplace_back("Number", "Premium", !premium_.empty() ? premium_ : "0");
    events_.emplace_back("PremiumDate", !premiumDate_.empty() ? premiumDate_ : settlementDate_);

    if (observationDates_.hasData()) {
        events_.emplace_back("ObservationDates", observationDates_);
    }

    // set product tag

    productTag_ = "SingleAssetOption({AssetClass})";

    // set script
    
    script_[""] = ScriptedTradeScriptData(script, "Option",
                                          {{"strike", "InitialStrike"},
                                           {"quantity", "Quantity"},
                                           {"underlyingSecurityId", "Underlying"},
                                           {"strikeCurrency", "Currency"},
                                           {"FinalStrike", "strike"},
                                           {"payoffAmount", "payoff"},
                                           {"currentNotional", "notional"},
                                           {"notionalCurrency", "Currency"}},
                                          {});

    // build trade

    ScriptedTrade::build(factory);
}

void StrikeResettableOption::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // asset class set in the base class already
    std::string assetClass = QuantLib::ext::any_cast<std::string>(additionalData_["isdaAssetClass"]);
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

void StrikeResettableOption::initIndices() { indices_.emplace_back("Index", "Underlying", scriptedIndexName(underlying_)); }

void StrikeResettableOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(tradeDataNode, "StrikeResettableOptionData node not found");
    longShort_ = XMLUtils::getChildValue(tradeDataNode, "LongShort", true);
    optionType_ = XMLUtils::getChildValue(tradeDataNode, "OptionType", true);
    currency_ = XMLUtils::getChildValue(tradeDataNode, "Currency", true);
    strike_ = XMLUtils::getChildValue(tradeDataNode, "Strike", true);
    resetStrike_ = XMLUtils::getChildValue(tradeDataNode, "ResetStrike", true);
    quantity_ = XMLUtils::getChildValue(tradeDataNode, "Quantity", true);
    triggerType_ = XMLUtils::getChildValue(tradeDataNode, "TriggerType", true);
    triggerPrice_ = XMLUtils::getChildValue(tradeDataNode, "TriggerPrice", true);

    expiryDate_ = XMLUtils::getChildValue(tradeDataNode, "ExpiryDate", true);
    settlementDate_ = XMLUtils::getChildValue(tradeDataNode, "SettlementDate", true);

    XMLNode* underlying = XMLUtils::getChildNode(tradeDataNode, "Underlying");
    QL_REQUIRE(underlying, "Underlying node not found");

    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(underlying);
    underlying_ = underlyingBuilder.underlying();

    XMLNode* observationNode = XMLUtils::getChildNode(tradeDataNode, "ObservationDates");
    QL_REQUIRE(observationNode, "No observation dates provided");
    observationDates_.fromXML(observationNode);

    premium_ = XMLUtils::getChildValue(tradeDataNode, "Premium", false);
    premiumDate_ = XMLUtils::getChildValue(tradeDataNode, "PremiumDate", false);

    initIndices();
}

XMLNode* StrikeResettableOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, tradeNode);
    XMLUtils::addChild(doc, tradeNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, tradeNode, "OptionType", optionType_);
    XMLUtils::addChild(doc, tradeNode, "Currency", currency_);
    XMLUtils::addChild(doc, tradeNode, "Strike", strike_);
    XMLUtils::addChild(doc, tradeNode, "ResetStrike", resetStrike_);
    XMLUtils::addChild(doc, tradeNode, "Quantity", quantity_);

    XMLUtils::addChild(doc, tradeNode, "TriggerType", triggerType_);
    XMLUtils::addChild(doc, tradeNode, "TriggerPrice", triggerPrice_);

    XMLUtils::addChild(doc, tradeNode, "ExpiryDate", expiryDate_);
    XMLUtils::addChild(doc, tradeNode, "SettlementDate", settlementDate_);

    XMLUtils::appendNode(tradeNode, underlying_->toXML(doc));

    XMLNode* observationNode = observationDates_.toXML(doc);
    XMLUtils::setNodeName(doc, observationNode, "ObservationDates");
    XMLUtils::appendNode(tradeNode, observationNode);

    XMLUtils::addChild(doc, tradeNode, "Premium", premium_);
    XMLUtils::addChild(doc, tradeNode, "PremiumDate", premiumDate_);

    return node;
}

} // namespace data
} // namespace ore
