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

#include <ored/portfolio/performanceoption_01.hpp>
#include <ored/scripting/utilities.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

void PerformanceOption_01::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    numbers_.emplace_back("Number", "NotionalAmount", notionalAmount_);
    numbers_.emplace_back("Number", "ParticipationRate", participationRate_);

    events_.emplace_back("ValuationDate", valuationDate_);
    events_.emplace_back("SettlementDate", settlementDate_);

    numbers_.emplace_back("Number", "StrikePrices", strikePrices_);

    numbers_.emplace_back("Number", "Strike", strike_);

    Position::Type position = parsePositionType(position_);
    numbers_.emplace_back("Number", "LongShort", position == Position::Long ? "1" : "-1");

    currencies_.emplace_back("Currency", "PayCcy", payCcy_);

    // set product tag

    productTag_ = "MultiAssetOption({AssetClass})";

    // set script

    // clang-format off
    script_ = {{"", ScriptedTradeScriptData(std::string("NUMBER i, p, currentNotional;\n") +
                        "FOR i IN (1, SIZE(Underlyings), 1) DO\n" +
                        (strikeIncluded_ ? "  p = p + Weights[i] * ( Underlyings[i](ValuationDate) / StrikePrices[i] - Strike );\n" :
			 "  p = p + Weights[i] * Underlyings[i](ValuationDate) / StrikePrices[i];\n") +
                        "END;\n"
                        "Option = LOGPAY( LongShort * NotionalAmount * ParticipationRate *\n" +
                        (strikeIncluded_ ? "                 max( p, 0 ), ValuationDate, SettlementDate, PayCcy );\n" :
			 "                 max( p - Strike, 0 ), ValuationDate, SettlementDate, PayCcy );\n") +
			 "currentNotional = NotionalAmount * ParticipationRate;\n",
                        "Option", {{"currentNotional", "currentNotional"}, {"notionalCurrency", "PayCcy"}}, {})}};
    // clang-format on

    // build trade

    ScriptedTrade::build(factory);
}

void PerformanceOption_01::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // ISDA taxonomy
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
    additionalData_["isdaTransaction"] = string("Basket");
}

void PerformanceOption_01::initIndices() {
    std::vector<std::string> underlyingNames, weights;
    for (auto const& u : underlyings_) {
        underlyingNames.push_back(scriptedIndexName(u));
        QL_REQUIRE(u->weight() != Null<Real>(), "underlying '" << u->name() << "' has no weight");
        weights.push_back(boost::lexical_cast<std::string>(u->weight()));
    }
    indices_.emplace_back("Index", "Underlyings", underlyingNames);
    numbers_.emplace_back("Number", "Weights", weights);
}

void PerformanceOption_01::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, "PerformanceOption01Data");
    QL_REQUIRE(tradeDataNode, "PerformanceOption01Data node not found");
    notionalAmount_ = XMLUtils::getChildValue(tradeDataNode, "NotionalAmount", true);
    participationRate_ = XMLUtils::getChildValue(tradeDataNode, "ParticipationRate", true);
    valuationDate_ = XMLUtils::getChildValue(tradeDataNode, "ValuationDate", true);
    settlementDate_ = XMLUtils::getChildValue(tradeDataNode, "SettlementDate", true);
    auto underlyingsNode = XMLUtils::getChildNode(tradeDataNode, "Underlyings");
    QL_REQUIRE(underlyingsNode, "No Underlyings node");
    auto underlyings = XMLUtils::getChildrenNodes(underlyingsNode, "Underlying");
    for (auto const& n : underlyings) {
        UnderlyingBuilder underlyingBuilder;
        underlyingBuilder.fromXML(n);
        underlyings_.push_back(underlyingBuilder.underlying());
    }
    strikePrices_ = XMLUtils::getChildrenValues(tradeDataNode, "StrikePrices", "StrikePrice", true);
    strike_ = XMLUtils::getChildValue(tradeDataNode, "Strike", true);
    strikeIncluded_ = XMLUtils::getChildValueAsBool(tradeDataNode, "StrikeIncluded", false, true);
    position_ = XMLUtils::getChildValue(tradeDataNode, "Position", true);
    payCcy_ = XMLUtils::getChildValue(tradeDataNode, "PayCcy", true);
    initIndices();
}

XMLNode* PerformanceOption_01::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode("PerformanceOption01Data");
    XMLUtils::appendNode(node, tradeNode);
    XMLUtils::addChild(doc, tradeNode, "NotionalAmount", notionalAmount_);
    XMLUtils::addChild(doc, tradeNode, "ParticipationRate", participationRate_);
    XMLUtils::addChild(doc, tradeNode, "ValuationDate", valuationDate_);
    XMLUtils::addChild(doc, tradeNode, "SettlementDate", settlementDate_);
    XMLNode* underlyingsNode = doc.allocNode("Underlyings");
    XMLUtils::appendNode(tradeNode, underlyingsNode);
    for (auto& n : underlyings_) {
        XMLUtils::appendNode(underlyingsNode, n->toXML(doc));
    }
    XMLUtils::addChildren(doc, tradeNode, "StrikePrices", "StrikePrice", strikePrices_);
    XMLUtils::addChild(doc, tradeNode, "Strike", strike_);
    XMLUtils::addChild(doc, tradeNode, "StrikeIncluded", strikeIncluded_);
    XMLUtils::addChild(doc, tradeNode, "Position", position_);
    XMLUtils::addChild(doc, tradeNode, "PayCcy", payCcy_);
    return node;
}

} // namespace data
} // namespace ore
