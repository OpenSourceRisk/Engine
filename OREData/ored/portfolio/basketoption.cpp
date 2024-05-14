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

#include <ored/portfolio/basketoption.hpp>
#include <ored/scripting/utilities.hpp>

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

// clang-format off

    static const std::string vanilla_basket_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "\n"
        "      NUMBER u, basketPrice, ExerciseProbability, Payoff, currentNotional;\n"
        "\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "          basketPrice = basketPrice + Underlyings[u](Expiry) * Weights[u];\n"
        "      END;\n"
        "\n"
        "      Payoff = max(PutCall * (basketPrice - Strike), 0);\n"
        "\n"
        "      Option = LongShort * Notional * PAY(Payoff, Expiry, Settlement, PayCcy);\n"
        "\n"
        "      IF Payoff > 0 THEN\n"
        "          ExerciseProbability = 1;\n"
        "      END;\n"
        "      currentNotional = Notional * Strike;\n";

    static const std::string asian_basket_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "\n"
        "      NUMBER d, u, basketPrice, ExerciseProbability, Payoff;\n"
        "      NUMBER currentNotional;\n"
        "\n"
        "      FOR d IN (1, SIZE(ObservationDates), 1) DO\n"
        "          FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "              basketPrice = basketPrice + Underlyings[u](ObservationDates[d]) * Weights[u];\n"
        "          END;\n"
        "      END;\n"
        "\n"
        "      basketPrice = basketPrice / SIZE(ObservationDates);\n"
        "\n"
        "      Payoff = max(PutCall * (basketPrice - Strike), 0);\n"
        "\n"
        "      Option = LongShort * Notional * PAY(Payoff, Expiry, Settlement, PayCcy);\n"
        "\n"
        "      IF Payoff > 0 THEN\n"
        "          ExerciseProbability = 1;\n"
        "      END;\n"
        "\n"
        "      currentNotional = Notional * Strike;        \n";

    static const std::string average_strike_basket_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "\n"
        "      NUMBER d, u, timeAverageBasketPrice, currentNotional;\n"
        "      FOR d IN (1, SIZE(ObservationDates), 1) DO\n"
        "          FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "              timeAverageBasketPrice = timeAverageBasketPrice\n"
        "                + Underlyings[u](ObservationDates[d]) * Weights[u];\n"
        "          END;\n"
        "      END;\n"
        "      timeAverageBasketPrice = timeAverageBasketPrice / SIZE(ObservationDates);\n"
        "\n"
        "      NUMBER expiryBasketPrice;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "         expiryBasketPrice = expiryBasketPrice + Underlyings[u](Expiry) * Weights[u];\n"
        "      END;\n"
        "\n"
        "      NUMBER Payoff;\n"
        "      Payoff = max(PutCall * (expiryBasketPrice - timeAverageBasketPrice), 0);\n"
        "\n"
        "      Option = LongShort * Notional * PAY(Payoff, Expiry, Settlement, PayCcy);\n"
        "\n"
        "      NUMBER ExerciseProbability;\n"
        "      IF Payoff > 0 THEN\n"
        "          ExerciseProbability = 1;\n"
        "      END;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "        currentNotional = currentNotional + Notional * Underlyings[u](ObservationDates[1]) * Weights[u];\n"
        "      END;\n";

    static const std::string lookback_call_basket_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "\n"
        "      NUMBER d, u, basketPrice, minBasketPrice, currentNotional;\n"
        "      FOR d IN (1, SIZE(ObservationDates), 1) DO\n"
        "          basketPrice = 0;\n"
        "          FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "              basketPrice = basketPrice + Underlyings[u](ObservationDates[d]) * Weights[u];\n"
        "          END;\n"
        "          IF d == 1 THEN\n"
        "              minBasketPrice = basketPrice;\n"
        "          END;\n"
        "          IF basketPrice < minBasketPrice THEN\n"
        "              minBasketPrice = basketPrice;\n"
        "          END;\n"
        "      END;\n"
        "\n"
        "      NUMBER expiryBasketPrice;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "         expiryBasketPrice = expiryBasketPrice + Underlyings[u](Expiry) * Weights[u];\n"
        "      END;\n"
        "\n"
        "      NUMBER Payoff;\n"
        "      Payoff = max(expiryBasketPrice - minBasketPrice, 0);\n"
        "\n"
        "      Option = LongShort * Notional * PAY(Payoff, Expiry, Settlement, PayCcy);\n"
        "\n"
        "      NUMBER ExerciseProbability;\n"
        "      IF Payoff > 0 THEN\n"
        "          ExerciseProbability = 1;\n"
        "      END;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "        currentNotional = currentNotional + Notional * Underlyings[u](ObservationDates[1]) * Weights[u];\n"
        "      END;\n";

    static const std::string lookback_put_basket_option_script =
        "      REQUIRE SIZE(Underlyings) == SIZE(Weights);\n"
        "\n"
        "      NUMBER d, u, basketPrice, maxBasketPrice, currentNotional;\n"
        "      FOR d IN (1, SIZE(ObservationDates), 1) DO\n"
        "          basketPrice = 0;\n"
        "          FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "              basketPrice = basketPrice + Underlyings[u](ObservationDates[d]) * Weights[u];\n"
        "          END;\n"
        "          IF d == 1 THEN\n"
        "              maxBasketPrice = basketPrice;\n"
        "          END;\n"
        "          IF basketPrice > maxBasketPrice THEN\n"
        "              maxBasketPrice = basketPrice;\n"
        "          END;\n"
        "      END;\n"
        "\n"
        "      NUMBER expiryBasketPrice;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "         expiryBasketPrice = expiryBasketPrice + Underlyings[u](Expiry) * Weights[u];\n"
        "      END;\n"
        "\n"
        "      NUMBER Payoff;\n"
        "      Payoff = max(maxBasketPrice - expiryBasketPrice, 0);\n"
        "\n"
        "      Option = LongShort * Notional * PAY(Payoff, Expiry, Settlement, PayCcy);\n"
        "\n"
        "      NUMBER ExerciseProbability;\n"
        "      IF Payoff > 0 THEN\n"
        "          ExerciseProbability = 1;\n"
        "      END;\n"
        "      FOR u IN (1, SIZE(Underlyings), 1) DO\n"
        "        currentNotional = currentNotional + Notional * Underlyings[u](ObservationDates[1]) * Weights[u];\n"
        "      END;";

// clang-format on

void BasketOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    QL_REQUIRE(optionData_.exerciseDates().size() == 1, "expected exactly one exercise date");
    events_.emplace_back("Expiry", optionData_.exerciseDates().front());
    events_.emplace_back("Settlement", settlement_.empty() ? optionData_.exerciseDates().front() : settlement_);

    numbers_.emplace_back("Number", "Notional", notional_);
    auto positionType = parsePositionType(optionData_.longShort());
    numbers_.emplace_back("Number", "LongShort", positionType == Position::Long ? "1" : "-1");

    std::string ccy = parseCurrencyWithMinors(currency_).code();
    // if no strike ccy, set it to option ccy
    string strike;
    if (!tradeStrike_.empty()) {
        if (tradeStrike_.currency().empty())
            tradeStrike_.setCurrency(ccy);
        strike = boost::lexical_cast<std::string>(tradeStrike_.value());
    }
    currencies_.emplace_back("Currency", "PayCcy", ccy);

    QL_REQUIRE(optionData_.payoffType2().empty() || optionData_.payoffType2() == "Arithmetic",
               "BasketOption does not support payoffType2 = '" << optionData_.payoffType2()
                                                               << "', expected 'Arithmetic'");

    std::string scriptToUse;
    if (optionData_.payoffType() == "Vanilla") {
        scriptToUse = vanilla_basket_option_script;
        numbers_.emplace_back("Number", "PutCall", parseOptionType(optionData_.callPut()) == Option::Call ? "1" : "-1");
        numbers_.emplace_back("Number", "Strike", strike);
    } else if (optionData_.payoffType() == "Asian") {
        scriptToUse = asian_basket_option_script;
        numbers_.emplace_back("Number", "PutCall", parseOptionType(optionData_.callPut()) == Option::Call ? "1" : "-1");
        events_.emplace_back("ObservationDates", observationDates_);
        numbers_.emplace_back("Number", "Strike", strike);
    } else if (optionData_.payoffType() == "AverageStrike") {
        scriptToUse = average_strike_basket_option_script;
        numbers_.emplace_back("Number", "PutCall", parseOptionType(optionData_.callPut()) == Option::Call ? "1" : "-1");
        events_.emplace_back("ObservationDates", observationDates_);
    } else if (optionData_.payoffType() == "LookbackCall") {
        scriptToUse = lookback_call_basket_option_script;
        events_.emplace_back("ObservationDates", observationDates_);
    } else if (optionData_.payoffType() == "LookbackPut") {
        scriptToUse = lookback_put_basket_option_script;
        events_.emplace_back("ObservationDates", observationDates_);
    } else {
        QL_FAIL("payoff type '" << optionData_.payoffType() << "' not recognised");
    }

    // set product tag

    productTag_ = "MultiAssetOption({AssetClass})";

    // set script

    script_ = {
        {"", ScriptedTradeScriptData(scriptToUse, "Option",
                                     {{"currentNotional", "currentNotional"}, {"notionalCurrency", "PayCcy"}}, {})}};

    // build trade

    ScriptedTrade::build(factory, optionData_.premiumData(), positionType == QuantLib::Position::Long ? -1.0 : 1.0);
}

void BasketOption::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // ISDA taxonomy
    // asset class set in the base class already
    std::string assetClass = boost::any_cast<std::string>(additionalData_["isdaAssetClass"]);
    if (assetClass == "Equity") {
        additionalData_["isdaBaseProduct"] = string("Option");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    } else if (assetClass == "Foreign Exchange") {
        additionalData_["isdaBaseProduct"] = string("Complex Exotic");
        additionalData_["isdaSubProduct"] = string("Generic");
    } else if (assetClass == "Commodity") {
        // isda taxonomy missing for this class, using the same as equity
        additionalData_["isdaBaseProduct"] = string("Option");
        additionalData_["isdaSubProduct"] = string("Price Return Basic Performance");
    } else {
        WLOG("ISDA taxonomy incomplete for trade " << id());
    }
    additionalData_["isdaTransaction"] = string("Basket");
}

void BasketOption::initIndices() {
    std::vector<std::string> underlyings, weights;
    for (auto const& u : underlyings_) {
        underlyings.push_back(scriptedIndexName(u));
        QL_REQUIRE(u->weight() != Null<Real>(), "underlying '" << u->name() << "' has no weight");
        weights.push_back(boost::lexical_cast<std::string>(u->weight()));
    }

    indices_.emplace_back("Index", "Underlyings", underlyings);
    numbers_.emplace_back("Number", "Weights", weights);
}

void BasketOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(dataNode, tradeType() + "Data node not found");
    notional_ = XMLUtils::getChildValue(dataNode, "Notional", true);
    optionData_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));
    currency_ = XMLUtils::getChildValue(dataNode, "Currency");
    tradeStrike_.fromXML(dataNode, false);
    auto underlyingsNode = XMLUtils::getChildNode(dataNode, "Underlyings");
    QL_REQUIRE(underlyingsNode, "No Underlyings node");
    auto underlyings = XMLUtils::getChildrenNodes(underlyingsNode, "Underlying");
    for (auto const& n : underlyings) {
        UnderlyingBuilder underlyingBuilder;
        underlyingBuilder.fromXML(n);
        underlyings_.push_back(underlyingBuilder.underlying());
    }

    settlement_ = XMLUtils::getChildValue(dataNode, "Settlement");
    if (XMLNode* n = XMLUtils::getChildNode(dataNode, "ObservationDates"))
        observationDates_.fromXML(n);
    initIndices();
}

XMLNode* BasketOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, dataNode);
    XMLUtils::addChild(doc, dataNode, "Notional", notional_);
    XMLUtils::addChild(doc, dataNode, "Currency", currency_);
    if (!tradeStrike_.empty())
        XMLUtils::appendNode(dataNode, tradeStrike_.toXML(doc));
    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    for (auto& n : underlyings_) {
        XMLUtils::appendNode(underlyingsNode, n->toXML(doc));
    }
    XMLUtils::appendNode(dataNode, underlyingsNode);
    XMLUtils::appendNode(dataNode, optionData_.toXML(doc));
    if (!settlement_.empty())
        XMLUtils::addChild(doc, dataNode, "Settlement", settlement_);
    if (observationDates_.hasData()) {
        auto tmp = observationDates_.toXML(doc);
        XMLUtils::setNodeName(doc, tmp, "ObservationDates");
        XMLUtils::appendNode(dataNode, tmp);
    }
    return node;
}

} // namespace data
} // namespace ore
