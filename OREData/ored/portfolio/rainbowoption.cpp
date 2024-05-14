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

#include <ored/portfolio/rainbowoption.hpp>
#include <ored/scripting/utilities.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

// clang-format off

    static const std::string best_of_asset_or_cash_rainbow_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "      NUMBER u, thisPrice, bestPrice, Payoff, currentNotional;\n"
        "      NUMBER expUnderValue[SIZE(Underlyings)];\n"
        "      bestPrice = Strike;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "          expUnderValue[u] = Underlyings[u](Expiry);\n"
        "          thisPrice = Underlyings[u](Expiry) * Weights[u];\n"
        "          IF thisPrice > bestPrice THEN\n"
        "              bestPrice = thisPrice;\n"
        "          END;\n"
        "      END;\n"
        "      Option = LongShort * Notional * PAY(bestPrice, Expiry, Settlement, PayCcy);\n"
        "      currentNotional = Notional * Strike;\n";

    static const std::string worst_of_asset_or_cash_rainbow_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "      NUMBER u, thisPrice, worstPrice, Payoff, currentNotional;\n"
        "      NUMBER expUnderValue[SIZE(Underlyings)];\n"
        "      worstPrice = Strike;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "          expUnderValue[u] = Underlyings[u](Expiry);\n"
        "          thisPrice = Underlyings[u](Expiry) * Weights[u];\n"
        "          IF thisPrice < worstPrice THEN\n"
        "              worstPrice = thisPrice;\n"
        "          END;\n"
        "      END;\n"
        "      Option = LongShort * Notional * PAY(worstPrice, Expiry, Settlement, PayCcy);\n"
        "      currentNotional = Notional * Strike;\n";

    static const std::string max_rainbow_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "\n"
        "      NUMBER u, thisPrice, maxPrice, Payoff, ExerciseProbability, currentNotional;\n"
        "      NUMBER expUnderValue[SIZE(Underlyings)];\n"
        "      maxPrice = 0;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "          expUnderValue[u] = Underlyings[u](Expiry);\n"
        "          thisPrice = Underlyings[u](Expiry) * Weights[u];\n"
        "          IF thisPrice > maxPrice THEN\n"
        "              maxPrice = thisPrice;\n"
        "          END;\n"
        "      END;\n"
        "\n"
        "      Payoff = max(PutCall * (maxPrice - Strike), 0);\n"
        "\n"
        "      Option = LongShort * Notional * PAY(Payoff, Expiry, Settlement, PayCcy);\n"
        "\n"
        "      IF Payoff > 0 THEN\n"
        "          ExerciseProbability = 1;\n"
        "      END;\n"
        "      currentNotional = Notional * Strike;\n";

    static const std::string min_rainbow_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "      REQUIRE SIZE(Underlyings) > 0;\n"
        "\n"
        "      NUMBER u, thisPrice, minPrice, Payoff, ExerciseProbability, currentNotional;\n"
        "      NUMBER expUnderValue[SIZE(Underlyings)];\n"
        "      minPrice = Underlyings[1](Expiry) * Weights[1];\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "          expUnderValue[u] = Underlyings[u](Expiry);\n"
        "          thisPrice = Underlyings[u](Expiry) * Weights[u];\n"
        "          IF thisPrice < minPrice THEN\n"
        "              minPrice = thisPrice;\n"
        "          END;\n"
        "      END;\n"
        "\n"
        "      Payoff = max(PutCall * (minPrice - Strike), 0);\n"
        "\n"
        "      Option = LongShort * Notional * PAY(Payoff, Expiry, Settlement, PayCcy);\n"
        "\n"
        "      IF Payoff > 0 THEN\n"
        "          ExerciseProbability = 1;\n"
        "      END;\n"
        "      currentNotional = Notional * Strike;\n";

// clang-format on

void RainbowOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    QL_REQUIRE(optionData_.exerciseDates().size() == 1, "expected exactly one exercise date");
    events_.emplace_back("Expiry", optionData_.exerciseDates().front());
    events_.emplace_back("Settlement", settlement_.empty() ? optionData_.exerciseDates().front() : settlement_);

    numbers_.emplace_back("Number", "Notional", notional_);
    numbers_.emplace_back("Number", "LongShort",
                          parsePositionType(optionData_.longShort()) == Position::Long ? "1" : "-1");

    currencies_.emplace_back("Currency", "PayCcy", currency_);

    numbers_.emplace_back("Number", "Strike", strike_);

    std::string scriptToUse;
    if (optionData_.payoffType() == "BestOfAssetOrCash") {
        scriptToUse = best_of_asset_or_cash_rainbow_option_script;
    } else if (optionData_.payoffType() == "WorstOfAssetOrCash") {
        scriptToUse = worst_of_asset_or_cash_rainbow_option_script;
    } else if (optionData_.payoffType() == "MaxRainbow") {
        scriptToUse = max_rainbow_option_script;
        numbers_.emplace_back("Number", "PutCall", parseOptionType(optionData_.callPut()) == Option::Call ? "1" : "-1");
    } else if (optionData_.payoffType() == "MinRainbow") {
        scriptToUse = min_rainbow_option_script;
        numbers_.emplace_back("Number", "PutCall", parseOptionType(optionData_.callPut()) == Option::Call ? "1" : "-1");
    } else {
        QL_FAIL("payoff type '" << optionData_.payoffType() << "' not recognised");
    }

    // set product tag

    productTag_ = "MultiAssetOption({AssetClass})";

    // set script

    script_ = {{"", ScriptedTradeScriptData(scriptToUse, "Option",
                                            {{"currentNotional", "currentNotional"},
                                             {"notionalCurrency", "PayCcy"},
                                             {"expectedUnderlyingValue", "expUnderValue"}},
                                            {})}};

    // build trade

    ScriptedTrade::build(factory);
}

void RainbowOption::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // ISDA taxonomy
    // asset class set in the base class already
    std::string assetClass = boost::any_cast<std::string>(additionalData_["isdaAssetClass"]);
    if (assetClass == "Equity") {
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");  
    }
    else if (assetClass == "Commodity") {
        // isda taxonomy missing for this class, using the same as equity
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");  
    }
    else if (assetClass == "Foreign Exchange") {
        additionalData_["isdaBaseProduct"] = string("Complex Exotic");
        additionalData_["isdaSubProduct"] = string("Generic");  
    }
    else {
        WLOG("ISDA taxonomy incomplete for trade " << id());
    }                
    additionalData_["isdaTransaction"] = string("Basket");  
}

void RainbowOption::initIndices() {
    std::vector<std::string> underlyings, weights;
    for (auto const& u : underlyings_) {
        underlyings.push_back(scriptedIndexName(u));
        QL_REQUIRE(u->weight() != Null<Real>(), "underlying '" << u->name() << "' has no weight");
        weights.push_back(boost::lexical_cast<std::string>(u->weight()));
    }
    indices_.emplace_back("Index", "Underlyings", underlyings);
    numbers_.emplace_back("Number", "Weights", weights);
}

void RainbowOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(dataNode, tradeType() + "Data node not found");
    currency_ = XMLUtils::getChildValue(dataNode, "Currency", true);
    notional_ = XMLUtils::getChildValue(dataNode, "Notional", true);
    strike_ = XMLUtils::getChildValue(dataNode, "Strike", true);
    auto underlyingsNode = XMLUtils::getChildNode(dataNode, "Underlyings");
    QL_REQUIRE(underlyingsNode, "No Underlyings node");
    auto underlyings = XMLUtils::getChildrenNodes(underlyingsNode, "Underlying");
    for (auto const& n : underlyings) {
        UnderlyingBuilder underlyingBuilder;
        underlyingBuilder.fromXML(n);
        underlyings_.push_back(underlyingBuilder.underlying());
    }
    optionData_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));
    settlement_ = XMLUtils::getChildValue(dataNode, "Settlement");
    initIndices();
}

XMLNode* RainbowOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, dataNode);
    XMLUtils::addChild(doc, dataNode, "Currency", currency_);
    XMLUtils::addChild(doc, dataNode, "Notional", notional_);
    XMLUtils::addChild(doc, dataNode, "Strike", strike_);
    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    for (auto& n : underlyings_) {
        XMLUtils::appendNode(underlyingsNode, n->toXML(doc));
    }
    XMLUtils::appendNode(dataNode, underlyingsNode);
    XMLUtils::appendNode(dataNode, optionData_.toXML(doc));
    if (!settlement_.empty())
        XMLUtils::addChild(doc, dataNode, "Settlement", settlement_);
    return node;
}

} // namespace data
} // namespace ore
