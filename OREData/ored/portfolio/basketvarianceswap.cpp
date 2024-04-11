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
#include <ored/portfolio/basketvarianceswap.hpp>
#include <ored/scripting/utilities.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/to_string.hpp>
#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

void BasketVarianceSwap::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    auto builder = QuantLib::ext::dynamic_pointer_cast<ScriptedTradeEngineBuilder>(factory->builder("ScriptedTrade"));

    // set script parameters

    clear();
    initIndices();

    // check underlying types
    QL_REQUIRE(underlyings_.size() > 0, "No underlyings were provided");
    string type = underlyings_.front()->type();
    for (auto u : underlyings_) {
        QL_REQUIRE(u->type() == type, "All of Underlyings must be from the same asset class.");
    }

    // dates/schedules
    events_.emplace_back("ValuationSchedule", valuationSchedule_);
    events_.emplace_back("SettlementDate", settlementDate_);

    // numbers
    numbers_.emplace_back("Number", "Strike", strike_);
    numbers_.emplace_back("Number", "Notional", notional_);
    numbers_.emplace_back("Number", "Cap", cap_.empty() ? "0" : cap_);
    numbers_.emplace_back("Number", "Floor", floor_.empty() ? "0" : floor_);

    // booleans
    numbers_.emplace_back("Number", "LongShort", parsePositionType(longShort_) == Position::Long ? "1" : "-1");
    numbers_.emplace_back("Number", "SquaredPayoff", squaredPayoff_ ? "1" : "-1");

    // currencies
    currencies_.emplace_back("Currency", "PayCcy", currency_);

    // set product tag accordingly
    if (type == "InterestRate") {
        productTag_ = "MultiUnderlyingIrOption";
    } else {
        productTag_ = "MultiAssetOptionAD({AssetClass})";
    }
    
    LOG("ProductTag=" << productTag_);

    // set script

    script_ = {// clang-format off
        {"", ScriptedTradeScriptData(
                "REQUIRE {Notional >= 0} AND {Strike >= 0};\n"
                "REQUIRE {Cap >= 0} AND {Floor >= 0};\n"
                "\n"
                "NUMBER i, n;\n"
                "n = SIZE(Underlyings);\n"
                "\n"
                "NUMBER sumOfWeights;\n"
                "FOR i IN (1, n, 1) DO\n"
                "  sumOfWeights = sumOfWeights + Weights[i];\n"
                "END;\n"
                "REQUIRE sumOfWeights == 1;\n"
                "\n"
                "NUMBER d, expectedN, currPrice[n], prevPrice[n];\n"
                "NUMBER realisedVariance, basketVariation, realisedVariation;\n"
                "NUMBER strike, cap, floor, currentNotional, payoff;\n"
                "\n"
                "FOR d IN (2, SIZE(ValuationSchedule), 1) DO\n"
                "  basketVariation = 0;\n"
                "  FOR i IN (1, n, 1) DO\n"
                "    currPrice[i] = Underlyings[i](ValuationSchedule[d]);\n"
                "    prevPrice[i] = Underlyings[i](ValuationSchedule[d-1]);\n"
                "    basketVariation = basketVariation + Weights[i] * ln(currPrice[i]/prevPrice[i]);\n"
                "  END;\n"
                "  realisedVariance = realisedVariance + pow(basketVariation, 2);\n"
                "END;\n"
                "\n"
                "expectedN = SIZE(ValuationSchedule) - 1;\n"
                "realisedVariance = (252/expectedN) * realisedVariance;\n"
                "\n"
                "IF SquaredPayoff == 1 THEN\n"
                "  realisedVariation = realisedVariance;\n"
                "  currentNotional = pow(100, 2) * Notional / (2 * 100 * Strike);\n"
                "  strike = pow(Strike, 2);\n"
                "ELSE\n"
                "  realisedVariation = sqrt(realisedVariance);\n"
                "  currentNotional = 100 * Notional;\n"
                "  strike = Strike;\n"
                "END;\n"
                "\n"
                "IF Floor > 0 THEN\n"
                "  IF SquaredPayoff == 1 THEN\n"
                "    floor = pow(Floor, 2);\n"
                "  ELSE\n"
                "    floor = Floor;\n"
                "  END;\n"
                "  realisedVariation = max(floor * strike, realisedVariation);\n"
                "END;\n"
                "IF Cap > 0 THEN\n"
                "  IF SquaredPayoff == 1 THEN\n"
                "    cap = pow(Cap, 2);\n"
                "  ELSE\n"
                "    cap = Cap;\n"
                "  END;\n"
                "  realisedVariation = min(cap * strike, realisedVariation);\n"
                "END;\n"
                "\n"
                "payoff = LongShort * currentNotional * (realisedVariation - strike);\n"
                "\n"
                "Swap = PAY(payoff, ValuationSchedule[SIZE(ValuationSchedule)],\n"
                "           SettlementDate, PayCcy);\n",
             "Swap",
             {{"RealisedVariance", "realisedVariance"},
              {"currentNotional", "currentNotional"},
              {"notionalCurrency", "PayCcy"}},
            {})}};
        // clang-format on

    // build trade

    ScriptedTrade::build(factory);
}

void BasketVarianceSwap::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // ISDA taxonomy
    // asset class set in the base class already
    std::string assetClass = boost::any_cast<std::string>(additionalData_["isdaAssetClass"]);
    if (assetClass == "Equity") {
        additionalData_["isdaBaseProduct"] = string("Swap");
        additionalData_["isdaSubProduct"] = string("Parameter Return Variance");
    } else if (assetClass == "Foreign Exchange") {
        additionalData_["isdaBaseProduct"] = string("Complex Exotic");
        additionalData_["isdaSubProduct"] = string("Generic");
    } else if (assetClass == "Commodity") {
        // isda taxonomy missing for this class, using the same as equity
        additionalData_["isdaBaseProduct"] = string("Other");
        additionalData_["isdaSubProduct"] = string("Parameter Return Variance");
    } else {
        WLOG("ISDA taxonomy incomplete for trade " << id());
    }
    additionalData_["isdaTransaction"] = string("Basket");
}

void BasketVarianceSwap::initIndices() {
    std::vector<string> underlyings, weights;
    for (auto const& u : underlyings_) {
        underlyings.push_back(scriptedIndexName(u));
        QL_REQUIRE(u->weight() != Null<Real>(), "underlying '" << u->name() << "' has no weight");
        weights.push_back(boost::lexical_cast<std::string>(u->weight()));
    }
    indices_.emplace_back("Index", "Underlyings", underlyings);
    numbers_.emplace_back("Number", "Weights", weights);
}

void BasketVarianceSwap::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(tradeDataNode, "BasketVarianceSwapData node not found");
    longShort_ = XMLUtils::getChildValue(tradeDataNode, "LongShort", true);
    notional_ = XMLUtils::getChildValue(tradeDataNode, "Notional", true);
    strike_ = XMLUtils::getChildValue(tradeDataNode, "Strike", true);
    cap_ = XMLUtils::getChildValue(tradeDataNode, "Cap", false);
    floor_ = XMLUtils::getChildValue(tradeDataNode, "Floor", false);

    XMLNode* valuationSchedule = XMLUtils::getChildNode(tradeDataNode, "ValuationSchedule");
    QL_REQUIRE(valuationSchedule, "No valuation schedule provided");
    valuationSchedule_.fromXML(valuationSchedule);
    
    XMLNode* underlyingsNode = XMLUtils::getChildNode(tradeDataNode, "Underlyings");
    QL_REQUIRE(underlyingsNode, "Could not find an Underlyings node.");
    auto underlyings = XMLUtils::getChildrenNodes(underlyingsNode, "Underlying");
    for (auto const& u : underlyings) {
        UnderlyingBuilder underlyingBuilder;
        underlyingBuilder.fromXML(u);
        underlyings_.push_back(underlyingBuilder.underlying());
    }

    settlementDate_ = XMLUtils::getChildValue(tradeDataNode, "SettlementDate", false);

    string squaredPayoff = XMLUtils::getChildValue(tradeDataNode, "SquaredPayoff", false);
    squaredPayoff_ = squaredPayoff.empty() ? false : parseBool(squaredPayoff);

    currency_ = XMLUtils::getChildValue(tradeDataNode, "Currency", true);

    initIndices();
}

XMLNode* BasketVarianceSwap::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, tradeNode);
    XMLUtils::addChild(doc, tradeNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, tradeNode, "Currency", currency_);
    XMLUtils::addChild(doc, tradeNode, "Notional", notional_);
    XMLUtils::addChild(doc, tradeNode, "Strike", strike_);

    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    for (auto const& u : underlyings_)
        XMLUtils::appendNode(underlyingsNode, u->toXML(doc));
    XMLUtils::appendNode(tradeNode, underlyingsNode);

    XMLNode* valuationSchedule = valuationSchedule_.toXML(doc);
    XMLUtils::setNodeName(doc, valuationSchedule, "ValuationSchedule");
    XMLUtils::appendNode(tradeNode, valuationSchedule);
    
    XMLUtils::addChild(doc, tradeNode, "SettlementDate", settlementDate_);
    if (!cap_.empty()) {
        XMLUtils::addChild(doc, tradeNode, "Cap", cap_);
    }
    if (!floor_.empty()) {
        XMLUtils::addChild(doc, tradeNode, "Floor", floor_);
    }
    XMLUtils::addChild(doc, tradeNode, "SquaredPayoff", squaredPayoff_);

    return node;
}

} // namespace data
} // namespace ore
