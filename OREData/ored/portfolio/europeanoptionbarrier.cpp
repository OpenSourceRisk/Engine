/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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
#include <ored/portfolio/europeanoptionbarrier.hpp>
#include <ored/scripting/utilities.hpp>

namespace ore {
namespace data {

void EuropeanOptionBarrier::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    events_.emplace_back("PremiumDate", premiumDate_);
    events_.emplace_back("OptionExpiry", optionExpiry_);
    events_.emplace_back("SettlementDate", settlementDate_);
    if (barrierStyle_ == "American")
        events_.emplace_back("BarrierSchedule", barrierSchedule_);
    else
        events_.emplace_back("BarrierSchedule", optionExpiry_);

    numbers_.emplace_back("Number", "Quantity", quantity_);
    numbers_.emplace_back("Number", "Strike", strike_);
    numbers_.emplace_back("Number", "PremiumAmount", premiumAmount_);
    numbers_.emplace_back("Number", "BarrierLevel", barrierLevel_);

    Barrier::Type barrierType = parseBarrierType(barrierType_);
    string barrierTypeVal;
    if (barrierType == Barrier::Type::DownIn)
        barrierTypeVal = "1";
    else if (barrierType == Barrier::Type::UpIn)
        barrierTypeVal = "2";
    else if (barrierType == Barrier::Type::DownOut)
        barrierTypeVal = "3";
    else if (barrierType == Barrier::Type::UpOut)
        barrierTypeVal = "4";
    else
        QL_FAIL("Unknown Barrier Type: " << barrierType);
    numbers_.emplace_back("Number", "BarrierType", barrierTypeVal);

    numbers_.emplace_back("Number", "BarrierStyle", barrierStyle_ == "American" ? "1" : "-1");

    Option::Type putCall = parseOptionType(putCall_);
    numbers_.emplace_back("Number", "PutCall", putCall == Option::Call ? "1" : "-1");

    Position::Type longShort = parsePositionType(longShort_);
    numbers_.emplace_back("Number", "LongShort", longShort == Position::Long ? "1" : "-1");

    currencies_.emplace_back("Currency", "PremiumCurrency", premiumCurrency_);
    currencies_.emplace_back("Currency", "PayCcy", payCcy_);

    // check underlying types
    QL_REQUIRE(optionUnderlying_->type() == "Equity" || optionUnderlying_->type() == "Commodity" ||
                   optionUnderlying_->type() == "FX" || optionUnderlying_->type() == "InterestRate",
               "Underlying type " << optionUnderlying_->type() << " not supported");
    QL_REQUIRE(barrierUnderlying_->type() == "Equity" || barrierUnderlying_->type() == "Commodity" ||
                   barrierUnderlying_->type() == "FX" || barrierUnderlying_->type() == "InterestRate",
               "Underlying type " << barrierUnderlying_->type() << " not supported");

    // set product tag accordingly
    if (optionUnderlying_->type() == "InterestRate" && barrierUnderlying_->type() == "InterestRate")
        productTag_ = "MultiUnderlyingIrOption";
    else if (optionUnderlying_->type() == "InterestRate" || barrierUnderlying_->type() == "InterestRate")
        productTag_ = "IrHybrid({AssetClass})";
    else
        productTag_ = "MultiAssetOption({AssetClass})";

    LOG("ProductTag=" << productTag_);

    // set script
    script_ = {
        {"",
         ScriptedTradeScriptData(
             "           REQUIRE Quantity >= 0;\n"
             "           REQUIRE PremiumDate <= SettlementDate;\n"
             "           REQUIRE OptionExpiry <= SettlementDate;\n"
             "\n"
             "           NUMBER barrierFixing, finalPrice, exercisePayoff, premium;\n"
             "           NUMBER notKnocked, factor, currentNotional, exerciseProbability;\n"
             "\n"
             "           notKnocked = 1;\n"
             "\n"
             "           IF BarrierStyle == -1 THEN\n"
             "             barrierFixing = BarrierUnderlying(OptionExpiry);\n"
             "             finalPrice = OptionUnderlying(OptionExpiry);\n"
             "\n"
             "             IF {BarrierType == 1 AND barrierFixing <= BarrierLevel}\n"
             "             OR {BarrierType == 2 AND barrierFixing >= BarrierLevel}\n"
             "             OR {BarrierType == 3 AND barrierFixing > BarrierLevel}\n"
             "             OR {BarrierType == 4 AND barrierFixing < BarrierLevel} THEN\n"
             "               exercisePayoff = LOGPAY(LongShort * Quantity * max(0, PutCall * (finalPrice - Strike)),\n"
             "                                       OptionExpiry, SettlementDate, PayCcy, 1, ExercisePayoff);\n"
             "             END;\n"
             "\n"
             "           ELSE\n"
             "             NUMBER d;\n"
             "             FOR d IN (2, SIZE(BarrierSchedule), 1) DO\n"
             "               IF BarrierType == 1 OR BarrierType == 3 THEN\n"
             "                 notKnocked = notKnocked * (1 - BELOWPROB(BarrierUnderlying, BarrierSchedule[d-1],\n"
             "                                                          BarrierSchedule[d], BarrierLevel));\n"
             "               ELSE\n"
             "                 notKnocked = notKnocked * (1 - ABOVEPROB(BarrierUnderlying, BarrierSchedule[d-1],\n"
             "                                                          BarrierSchedule[d], BarrierLevel));\n"
             "               END;\n"
             "             END;\n"
             "\n"
             "             IF BarrierType == 1 OR BarrierType == 2 THEN\n"
             "               factor = 1 - notKnocked;\n"
             "             ELSE\n"
             "               factor = notKnocked;\n"
             "             END;\n"
             "\n"
             "             finalPrice = OptionUnderlying(OptionExpiry);\n"
             "             exercisePayoff = LOGPAY(Quantity * factor * max(0, PutCall * (finalPrice - Strike)),\n"
             "                                     OptionExpiry, SettlementDate, PayCcy, 1, ExercisePayoff);\n"
             "           END;\n"
             "\n"
             "           premium = LOGPAY(Quantity * PremiumAmount, PremiumDate,\n"
             "                            PremiumDate, PremiumCurrency, 0, Premium);\n"
             "\n"
             "           IF exercisePayoff != 0 THEN\n"
             "             exerciseProbability = 1;\n"
             "           END;\n"
             "\n"
             "           currentNotional = Quantity * Strike;"
             "           Option = LongShort * (exercisePayoff - premium);\n",
             //
             "Option",
             {{"ExerciseProbability", "exerciseProbability"},
              {"currentNotional", "currentNotional"},
              {"notionalCurrency", "PayCcy"}},
             {})}};

    // build trade

    ScriptedTrade::build(factory);
}

void EuropeanOptionBarrier::initIndices() {
    indices_.emplace_back("Index", "OptionUnderlying", scriptedIndexName(optionUnderlying_));
    indices_.emplace_back("Index", "BarrierUnderlying", scriptedIndexName(barrierUnderlying_));
}

void EuropeanOptionBarrier::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, "EuropeanOptionBarrierData");
    QL_REQUIRE(tradeDataNode, "EuropeanOptionBarrierData node not found");
    quantity_ = XMLUtils::getChildValue(tradeDataNode, "Quantity", true);
    putCall_ = XMLUtils::getChildValue(tradeDataNode, "PutCall", true);
    longShort_ = XMLUtils::getChildValue(tradeDataNode, "LongShort", true);
    strike_ = XMLUtils::getChildValue(tradeDataNode, "Strike", true);
    premiumAmount_ = XMLUtils::getChildValue(tradeDataNode, "PremiumAmount", true);
    premiumCurrency_ = XMLUtils::getChildValue(tradeDataNode, "PremiumCurrency", true);
    premiumDate_ = XMLUtils::getChildValue(tradeDataNode, "PremiumDate", true);
    optionExpiry_ = XMLUtils::getChildValue(tradeDataNode, "OptionExpiry", true);

    XMLNode* tmp = XMLUtils::getChildNode(tradeDataNode, "OptionUnderlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(tradeDataNode, "Name");
    UnderlyingBuilder optionUnderlyingBuilder("OptionUnderlying", "Name");
    optionUnderlyingBuilder.fromXML(tmp);
    optionUnderlying_ = optionUnderlyingBuilder.underlying();

    tmp = XMLUtils::getChildNode(tradeDataNode, "BarrierUnderlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(tradeDataNode, "Name");
    UnderlyingBuilder barrierUnderlyingBuilder("BarrierUnderlying", "Name");
    barrierUnderlyingBuilder.fromXML(tmp);
    barrierUnderlying_ = barrierUnderlyingBuilder.underlying();

    barrierLevel_ = XMLUtils::getChildValue(tradeDataNode, "BarrierLevel", true);
    barrierType_ = XMLUtils::getChildValue(tradeDataNode, "BarrierType", true);

    barrierStyle_ = XMLUtils::getChildValue(tradeDataNode, "BarrierStyle", true);
    QL_REQUIRE(barrierStyle_ == "American" || barrierStyle_ == "European",
               "Barrier style " << barrierStyle_ << " not supported");
    if (barrierStyle_ == "American") {
        QL_REQUIRE(XMLUtils::getChildNode(tradeDataNode, "BarrierSchedule"), "Missing BarrierSchedule node.");
        barrierSchedule_.fromXML(XMLUtils::getChildNode(tradeDataNode, "BarrierSchedule"));
    }

    settlementDate_ = XMLUtils::getChildValue(tradeDataNode, "SettlementDate", true);
    payCcy_ = XMLUtils::getChildValue(tradeDataNode, "PayCcy", true);

    initIndices();
}

XMLNode* EuropeanOptionBarrier::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode("EuropeanOptionBarrierData");
    XMLUtils::appendNode(node, tradeNode);
    XMLUtils::addChild(doc, tradeNode, "Quantity", quantity_);
    XMLUtils::addChild(doc, tradeNode, "PutCall", putCall_);
    XMLUtils::addChild(doc, tradeNode, "LongShort", longShort_);
    XMLUtils::addChild(doc, tradeNode, "Strike", strike_);
    XMLUtils::addChild(doc, tradeNode, "PremiumAmount", premiumAmount_);
    XMLUtils::addChild(doc, tradeNode, "PremiumCurrency", premiumCurrency_);
    XMLUtils::addChild(doc, tradeNode, "PremiumDate", premiumDate_);
    XMLUtils::addChild(doc, tradeNode, "OptionExpiry", optionExpiry_);
    XMLUtils::appendNode(tradeNode, optionUnderlying_->toXML(doc));
    XMLUtils::appendNode(tradeNode, barrierUnderlying_->toXML(doc));
    XMLUtils::addChild(doc, tradeNode, "BarrierLevel", barrierLevel_);
    XMLUtils::addChild(doc, tradeNode, "BarrierType", barrierType_);
    XMLUtils::addChild(doc, tradeNode, "BarrierStyle", barrierStyle_);
    if (barrierStyle_ == "American") {
        XMLNode* tmp = barrierSchedule_.toXML(doc);
        XMLUtils::setNodeName(doc, tmp, "BarrierSchedule");
        XMLUtils::appendNode(tradeNode, tmp);
    }
    XMLUtils::addChild(doc, tradeNode, "SettlementDate", settlementDate_);
    XMLUtils::addChild(doc, tradeNode, "PayCcy", payCcy_);
    return node;
}

} // namespace data
} // namespace ore
