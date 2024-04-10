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

#include <ored/portfolio/autocallable_01.hpp>
#include <ored/scripting/utilities.hpp>

namespace ore {
namespace data {

void Autocallable_01::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    numbers_.emplace_back("Number", "NotionalAmount", notionalAmount_);
    numbers_.emplace_back("Number", "DeterminationLevel", determinationLevel_);
    numbers_.emplace_back("Number", "TriggerLevel", triggerLevel_);

    Position::Type position = parsePositionType(position_);
    numbers_.emplace_back("Number", "LongShort", position == Position::Long ? "1" : "-1");

    currencies_.emplace_back("Currency", "PayCcy", payCcy_);

    events_.emplace_back("FixingDates", fixingDates_);
    events_.emplace_back("SettlementDates", settlementDates_);

    numbers_.emplace_back("Number", "AccumulationFactors", accumulationFactors_);
    numbers_.emplace_back("Number", "Cap", cap_);

    // set product tag

    productTag_ = "MultiAssetOption({AssetClass})";

    // set script

    script_ = {{"", ScriptedTradeScriptData(
                        "NUMBER i, terminated, currentNotional;\n"
                        "FOR i IN (1, SIZE(FixingDates), 1) DO\n"
                        "  IF terminated == 0 AND Underlying(FixingDates[i]) <= TriggerLevel THEN\n"
                        "    Option = LOGPAY( LongShort * NotionalAmount * AccumulationFactors[i], FixingDates[i],\n"
                        "                     SettlementDates[i], PayCcy);\n"
                        "    terminated = 1;\n"
                        "  END;\n"
                        "  IF terminated == 0 AND i == SIZE(FixingDates) AND Underlying(FixingDates[i]) > "
                        "DeterminationLevel THEN\n"
                        "    Option = LOGPAY( -LongShort * NotionalAmount * min( Cap, Underlying(FixingDates[i]) -\n"
                        "                                                             DeterminationLevel ),\n"
                        "                     FixingDates[i], SettlementDates[i], PayCcy);\n"
                        "  END;\n"
                        "END;\n",
                        "Option", {{"currentNotional", "NotionalAmount"}, {"notionalCurrency", "PayCcy"}}, {})}};

    // build trade

    ScriptedTrade::build(factory);
}

void Autocallable_01::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // ISDA taxonomy, asset class set in the base class build
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
        additionalData_["isdaBaseProduct"] = string("Exotic");
        additionalData_["isdaSubProduct"] = string("Target");  
    }
    else {
        WLOG("ISDA taxonomy incomplete for trade " << id());
    }
    additionalData_["isdaTransaction"] = string("");
}

void Autocallable_01::initIndices() { indices_.emplace_back("Index", "Underlying", scriptedIndexName(underlying_)); }

void Autocallable_01::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, "Autocallable01Data");
    QL_REQUIRE(tradeDataNode, "Autocallable01Data node not found");
    notionalAmount_ = XMLUtils::getChildValue(tradeDataNode, "NotionalAmount");
    determinationLevel_ = XMLUtils::getChildValue(tradeDataNode, "DeterminationLevel");
    triggerLevel_ = XMLUtils::getChildValue(tradeDataNode, "TriggerLevel");

    XMLNode* tmp = XMLUtils::getChildNode(tradeDataNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(tradeDataNode, "Name");
    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    position_ = XMLUtils::getChildValue(tradeDataNode, "Position", true);
    payCcy_ = XMLUtils::getChildValue(tradeDataNode, "PayCcy", true);
    fixingDates_.fromXML(XMLUtils::getChildNode(XMLUtils::getChildNode(tradeDataNode, "FixingDates"), "ScheduleData"));
    settlementDates_.fromXML(
        XMLUtils::getChildNode(XMLUtils::getChildNode(tradeDataNode, "SettlementDates"), "ScheduleData"));
    accumulationFactors_ = XMLUtils::getChildrenValues(tradeDataNode, "AccumulationFactors", "Factor");
    cap_ = XMLUtils::getChildValue(tradeDataNode, "Cap");
    initIndices();
}

XMLNode* Autocallable_01::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode("Autocallable01Data");
    XMLUtils::appendNode(node, tradeNode);
    XMLUtils::addChild(doc, tradeNode, "NotionalAmount", notionalAmount_);
    XMLUtils::addChild(doc, tradeNode, "DeterminationLevel", determinationLevel_);
    XMLUtils::addChild(doc, tradeNode, "TriggerLevel", triggerLevel_);
    XMLUtils::appendNode(tradeNode, underlying_->toXML(doc));
    XMLUtils::addChild(doc, tradeNode, "Position", position_);
    XMLUtils::addChild(doc, tradeNode, "PayCcy", payCcy_);
    XMLNode* f = doc.allocNode("FixingDates");
    XMLUtils::appendNode(f, fixingDates_.toXML(doc));
    XMLUtils::appendNode(tradeNode, f);
    XMLNode* s = doc.allocNode("SettlementDates");
    XMLUtils::appendNode(s, settlementDates_.toXML(doc));
    XMLUtils::appendNode(tradeNode, s);
    XMLUtils::addChildren(doc, tradeNode, "AccumulationFactors", "Factor", accumulationFactors_);
    XMLUtils::addChild(doc, tradeNode, "Cap", cap_);
    return node;
}

} // namespace data
} // namespace ore
