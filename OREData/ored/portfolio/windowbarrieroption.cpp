/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <ored/portfolio/windowbarrieroption.hpp>
#include <ored/scripting/utilities.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/lexical_cast.hpp>

namespace ore {
namespace data {

// clang-format off

static const std::string window_barrier_script =
    "REQUIRE BarrierType == 1 OR BarrierType == 2 OR BarrierType == 3 OR BarrierType == 4;\n"
    "\n"
    "NUMBER i, Payoff, TriggerProbability, ExerciseProbability, isUp, currentNotional;\n"
    "\n"
    "IF BarrierType == 1 OR BarrierType == 3 THEN\n"
    "  TriggerProbability = BELOWPROB(Underlying, StartDate, EndDate, BarrierLevel);\n"
    "ELSE\n"
    "  TriggerProbability = ABOVEPROB(Underlying, StartDate, EndDate, BarrierLevel);\n"
    "END;\n"
    "\n"
    "Payoff = Quantity * PutCall * (Underlying(Expiry) - Strike);\n"
    "IF Payoff > 0.0 THEN\n"
    "  IF BarrierType == 1 OR BarrierType == 2 THEN\n"
    "    Option = PAY(Payoff * TriggerProbability, Expiry, Settlement, PayCcy);\n"
    "    ExerciseProbability = TriggerProbability;\n"
    "  ELSE\n"
    "    Option = PAY(Payoff * (1 - TriggerProbability), Expiry, Settlement, PayCcy);\n"
    "    ExerciseProbability = (1 - TriggerProbability);\n"
    "  END;\n"
    "END;\n"
    "\n"
    "Option = LongShort * Option;\n"
    "currentNotional = Quantity * Strike;\n";

// clang-format on

void WindowBarrierOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    if (strike_.currency().empty())
        strike_.setCurrency(currency_);

    currencies_.emplace_back("Currency", "PayCcy", currency_);
    numbers_.emplace_back("Number", "Quantity", fixingAmount_);
    numbers_.emplace_back("Number", "Strike", boost::lexical_cast<std::string>(strike_.value()));
    events_.emplace_back("StartDate", startDate_);
    events_.emplace_back("EndDate", endDate_);

    auto positionType = parsePositionType(optionData_.longShort());
    numbers_.emplace_back("Number", "LongShort", positionType == Position::Long ? "1" : "-1");
    numbers_.emplace_back("Number", "PutCall",
                          parseOptionType(optionData_.callPut()) == Option::Type::Call ? "1" : "-1");

    QL_REQUIRE(optionData_.exerciseDates().size() == 1, "WindowBarrierOption: one exercise date required, got " << optionData_.exerciseDates().size());
    events_.emplace_back("Expiry", optionData_.exerciseDates().front());

    std::string settlementDate = optionData_.exerciseDates().front();
    if(optionData_.paymentData()) {
        QL_REQUIRE(optionData_.paymentData()->dates().size() == 1,
                   "WindowBarrierOption: exactly one payment date required under PaymentData/Dates/Date");
    }
    events_.emplace_back("Settlement", settlementDate);

    std::string barrierType;
    QL_REQUIRE(barrier_.style().empty() || barrier_.style() == "American",
               "expected barrier style American, got " << barrier_.style());
    QL_REQUIRE(barrier_.levels().size() == 1,
               "WindowBarrierOption: exactly one barrier level required, got " << barrier_.levels().size());
    if (barrier_.type() == "UpAndOut")
        barrierType = "4";
    else if (barrier_.type() == "UpAndIn")
        barrierType = "2";
    else if (barrier_.type() == "DownAndOut")
        barrierType = "3";
    else if (barrier_.type() == "DownAndIn")
        barrierType = "1";
    else {
        QL_FAIL("WindowBarrierOption: invalid barrier level " << barrier_.type());
    }
    numbers_.emplace_back("Number", "BarrierType", barrierType);
    numbers_.emplace_back("Number", "BarrierLevel", boost::lexical_cast<std::string>(barrier_.levels().front().value()));

    // set product tag

    productTag_ = "SingleAssetOption({AssetClass})";

    // set script

    script_[""] = ScriptedTradeScriptData(
        window_barrier_script, "Option",
        {{"currentNotional", "currentNotional"},
         {"notionalCurrency", "PayCcy"},
         {"TriggerProbability", "TriggerProbability"},
         {"ExerciseProbability", "ExerciseProbability"}},
        {}, {}, {ScriptedTradeScriptData::CalibrationData("Underlying", {"Strike", "BarrierLevel"})});

    // build trade

    ScriptedTrade::build(factory, optionData_.premiumData(), positionType == QuantLib::Position::Long ? -1.0 : 1.0);

    additionalData_["isdaTransaction"] = string("");
}

void WindowBarrierOption::setIsdaTaxonomyFields() {
    ScriptedTrade::setIsdaTaxonomyFields();

    // ISDA taxonomy, asset class set in the base class build
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
        additionalData_["isdaBaseProduct"] = string("Exotic");
        additionalData_["isdaSubProduct"] = string("Target");
    } else {
        WLOG("ISDA taxonomy incomplete for trade " << id());
    }
}

void WindowBarrierOption::initIndices() {
    indices_.emplace_back("Index", "Underlying", scriptedIndexName(underlying_));
}

void WindowBarrierOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* dataNode = XMLUtils::getChildNode(node, tradeType() + "Data");
    QL_REQUIRE(dataNode, tradeType() + "Data node not found");
    fixingAmount_ = XMLUtils::getChildValue(dataNode, "FixingAmount");

    currency_ = XMLUtils::getChildValue(dataNode, "Currency");
    strike_.fromXML(dataNode, true, false);

    XMLNode* tmp = XMLUtils::getChildNode(dataNode, "Underlying");
    if (!tmp)
        tmp = XMLUtils::getChildNode(dataNode, "Name");
    UnderlyingBuilder underlyingBuilder;
    underlyingBuilder.fromXML(tmp);
    underlying_ = underlyingBuilder.underlying();

    optionData_.fromXML(XMLUtils::getChildNode(dataNode, "OptionData"));
    startDate_ = XMLUtils::getChildValue(dataNode, "StartDate");
    endDate_ = XMLUtils::getChildValue(dataNode, "EndDate");

    auto barrierNode = XMLUtils::getChildNode(dataNode, "BarrierData");
    QL_REQUIRE(barrierNode, "No BarrierData node");
    barrier_.fromXML(barrierNode);
    initIndices();
}

XMLNode* WindowBarrierOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* dataNode = doc.allocNode(tradeType() + "Data");
    XMLUtils::appendNode(node, dataNode);
    XMLUtils::addChild(doc, dataNode, "FixingAmount", fixingAmount_);
    XMLUtils::addChild(doc, dataNode, "Currency", currency_);

    XMLUtils::appendNode(dataNode, strike_.toXML(doc));

    XMLUtils::appendNode(dataNode, underlying_->toXML(doc));
    XMLUtils::appendNode(dataNode, optionData_.toXML(doc));
    XMLUtils::addChild(doc, dataNode, "StartDate", startDate_);
    XMLUtils::addChild(doc, dataNode, "EndDate", endDate_);
    XMLUtils::appendNode(dataNode, barrier_.toXML(doc));

    return node;
}

} // namespace data
} // namespace ore
