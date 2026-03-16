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

#include <ored/portfolio/doubledigitaloption.hpp>
#include <ored/scripting/utilities.hpp>
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace data {

std::pair<std::string, std::string> getLowerAndUpperBound(const std::string& type, const std::string& binaryLevelA,
                                                          const std::string& binaryLevelB) {
    if (type == "Call") {
        return {binaryLevelA, ore::data::to_string(QL_MAX_REAL)};
    } else if (type == "Put") {
        return {ore::data::to_string(QL_MIN_REAL), binaryLevelA};
    } else if (type == "Collar") {
        return {binaryLevelA, binaryLevelB};
    } else {
        QL_FAIL("DoubleDigitalOption got unexpected option type '"
                << type << "'. Valid values are 'Call', 'Put' and 'Collar'.");
    }
}

void DoubleDigitalOption::build(const QuantLib::ext::shared_ptr<EngineFactory>& factory) {

    // set script parameters

    clear();
    initIndices();

    events_.emplace_back("Expiry", expiry_);
    events_.emplace_back("Settlement", settlement_);

    
    std::string lowerBound1, upperBound1, lowerBound2, upperBound2;
    std::tie(lowerBound1, upperBound1) = getLowerAndUpperBound(type1_, binaryLevel1_, binaryLevelUpper1_);
    std::tie(lowerBound2, upperBound2) = getLowerAndUpperBound(type2_, binaryLevel2_, binaryLevelUpper2_);

    numbers_.emplace_back("Number", "BinaryPayout", binaryPayout_);
    numbers_.emplace_back("Number", "LowerBound1", lowerBound1);
    numbers_.emplace_back("Number", "UpperBound1", upperBound1);
    numbers_.emplace_back("Number", "LowerBound2", lowerBound2);
    numbers_.emplace_back("Number", "UpperBound2", upperBound2);

    Position::Type position = parsePositionType(position_);
    numbers_.emplace_back("Number", "LongShort", position == Position::Long ? "1" : "-1");

    currencies_.emplace_back("Currency", "PayCcy", payCcy_);

    // check underlying types
    QL_REQUIRE(underlying1_->type() == "Equity" || underlying1_->type() == "Commodity" ||
                   underlying1_->type() == "FX" || underlying1_->type() == "InterestRate",
               "underlying type " << underlying1_->type() << " not supported");
    QL_REQUIRE(underlying2_->type() == "Equity" || underlying2_->type() == "Commodity" ||
                   underlying2_->type() == "FX" || underlying2_->type() == "InterestRate",
               "underlying type " << underlying2_->type() << " not supported");
    if (underlying3_) {
        QL_REQUIRE(underlying3_->type() == "Equity" || underlying3_->type() == "Commodity" ||
                       underlying3_->type() == "FX" || underlying3_->type() == "InterestRate",
                   "underlying type " << underlying3_->type() << " not supported");

        QL_REQUIRE(underlying1_->type() == underlying3_->type(),
                   "Underlying1 and Underlying3 must belong to the same asset class. Got "
                       << underlying1_->type() << " and " << underlying3_->type());
    }
    if (underlying4_) {
        QL_REQUIRE(underlying4_->type() == "Equity" || underlying4_->type() == "Commodity" ||
                       underlying4_->type() == "FX" || underlying4_->type() == "InterestRate",
                   "underlying type " << underlying4_->type() << " not supported");
        
        QL_REQUIRE(underlying2_->type() == underlying4_->type(),
                   "Underlying2 and Underlying4 must belong to the same asset class. Got "
                       << underlying2_->type() << " and " << underlying4_->type());
    }

    // set product tag accordingly
    if (underlying1_->type() == "InterestRate" && underlying2_->type() == "InterestRate")
        productTag_ = "MultiUnderlyingIrOption";
    else if (underlying1_->type() == "InterestRate" || underlying2_->type() == "InterestRate")
        productTag_ = "IrHybrid({AssetClass})";
    else
        productTag_ = "MultiAssetOption({AssetClass})";

    LOG("ProductTag=" << productTag_);

    // set script

    string underlying1Str, underlying2Str;
    underlying1Str = underlying3_ ? "(Underlying1(Expiry) - Underlying3(Expiry))" : "Underlying1(Expiry)";
    underlying2Str = underlying4_ ? "(Underlying2(Expiry) - Underlying4(Expiry))" : "Underlying2(Expiry)";

    // clang-format off
    script_ = {
        {"", ScriptedTradeScriptData("NUMBER ExerciseProbability;\n"
                                     "IF " + underlying1Str + " >= LowerBound1 AND " + underlying1Str + " <= UpperBound1 AND\n"
                                     "   " + underlying2Str + " >= LowerBound2 AND " + underlying2Str + " <= UpperBound2 THEN\n"
                                     "     Option = LongShort * LOGPAY( BinaryPayout, Expiry, Settlement, PayCcy);\n"
                                     "     ExerciseProbability = 1;\n"
                                     "END;\n",
                                     "Option",
                                     {{"ExerciseProbability", "ExerciseProbability"},
                                      {"currentNotional", "BinaryPayout"},
                                      {"notionalCurrency", "PayCcy"}},
                                     {})}};
    // clang-format on

    // build trade

    ScriptedTrade::build(factory);
}

void DoubleDigitalOption::initIndices() {
    indices_.emplace_back("Index", "Underlying1", scriptedIndexName(underlying1_));
    indices_.emplace_back("Index", "Underlying2", scriptedIndexName(underlying2_));
    if (underlying3_)
        indices_.emplace_back("Index", "Underlying3", scriptedIndexName(underlying3_));
    if (underlying4_)
        indices_.emplace_back("Index", "Underlying4", scriptedIndexName(underlying4_));
}

void DoubleDigitalOption::fromXML(XMLNode* node) {
    Trade::fromXML(node);
    XMLNode* tradeDataNode = XMLUtils::getChildNode(node, "DoubleDigitalOptionData");
    QL_REQUIRE(tradeDataNode, "DoubleDigitalOptionData node not found");
    expiry_ = XMLUtils::getChildValue(tradeDataNode, "Expiry", true);
    settlement_ = XMLUtils::getChildValue(tradeDataNode, "Settlement", true);
    binaryPayout_ = XMLUtils::getChildValue(tradeDataNode, "BinaryPayout", true);
    binaryLevel1_ = XMLUtils::getChildValue(tradeDataNode, "BinaryLevel1", true);
    binaryLevel2_ = XMLUtils::getChildValue(tradeDataNode, "BinaryLevel2", true);
    type1_ = XMLUtils::getChildValue(tradeDataNode, "Type1", true);
    type2_ = XMLUtils::getChildValue(tradeDataNode, "Type2", true);
    position_ = XMLUtils::getChildValue(tradeDataNode, "Position", true);

    binaryLevelUpper1_ =
        XMLUtils::getChildValue(tradeDataNode, "BinaryLevelUpper1", type1_ == "Collar");
    binaryLevelUpper2_ =
        XMLUtils::getChildValue(tradeDataNode, "BinaryLevelUpper2", type2_ == "Collar");

    QL_REQUIRE((type1_ != "Collar" && binaryLevelUpper1_.empty()) ||
                   (type1_ == "Collar" && !binaryLevelUpper1_.empty()),
               "A non empty upper bound 'BinaryLevelUpper1' is required if and only if a type1 is set to 'Collar', "
               "please check trade xml.");

    QL_REQUIRE((type2_ != "Collar" && binaryLevelUpper2_.empty()) ||
                   (type2_ == "Collar" && !binaryLevelUpper2_.empty()),
               "A non empty upper bound 'BinaryLevelUpper2' is required if and only if a type2 is set to 'Collar', "
               "please check trade xml.");
  
    XMLNode* tmp = XMLUtils::getChildNode(tradeDataNode, "Underlying1");
    if (!tmp)
        tmp = XMLUtils::getChildNode(tradeDataNode, "Name1");
    UnderlyingBuilder underlyingBuilder1("Underlying1", "Name1");
    underlyingBuilder1.fromXML(tmp);
    underlying1_ = underlyingBuilder1.underlying();

    tmp = XMLUtils::getChildNode(tradeDataNode, "Underlying2");
    if (!tmp)
        tmp = XMLUtils::getChildNode(tradeDataNode, "Name2");
    UnderlyingBuilder underlyingBuilder2("Underlying2", "Name2");
    underlyingBuilder2.fromXML(tmp);
    underlying2_ = underlyingBuilder2.underlying();

    tmp = XMLUtils::getChildNode(tradeDataNode, "Underlying3");
    if (!tmp)
        tmp = XMLUtils::getChildNode(tradeDataNode, "Name3");
    if (tmp) {
        UnderlyingBuilder underlyingBuilder3("Underlying3", "Name3");
        underlyingBuilder3.fromXML(tmp);
        underlying3_ = underlyingBuilder3.underlying();
    }

    tmp = XMLUtils::getChildNode(tradeDataNode, "Underlying4");
    if (!tmp)
        tmp = XMLUtils::getChildNode(tradeDataNode, "Name4");
    if (tmp) {
        UnderlyingBuilder underlyingBuilder4("Underlying4", "Name4");
        underlyingBuilder4.fromXML(tmp);
        underlying4_ = underlyingBuilder4.underlying();
    }

    payCcy_ = XMLUtils::getChildValue(tradeDataNode, "PayCcy", true);

    initIndices();
}

XMLNode* DoubleDigitalOption::toXML(XMLDocument& doc) const {
    XMLNode* node = Trade::toXML(doc);
    XMLNode* tradeNode = doc.allocNode("DoubleDigitalOptionData");
    XMLUtils::appendNode(node, tradeNode);
    XMLUtils::addChild(doc, tradeNode, "Expiry", expiry_);
    XMLUtils::addChild(doc, tradeNode, "Settlement", settlement_);
    XMLUtils::addChild(doc, tradeNode, "BinaryPayout", binaryPayout_);
    XMLUtils::addChild(doc, tradeNode, "BinaryLevel1", binaryLevel1_);
    XMLUtils::addChild(doc, tradeNode, "BinaryLevel2", binaryLevel2_);
    if (!binaryLevelUpper1_.empty()) {
        XMLUtils::addChild(doc, tradeNode, "BinaryLevelUpper1", binaryLevelUpper1_);
    }
    if (!binaryLevelUpper2_.empty()) {
        XMLUtils::addChild(doc, tradeNode, "BinaryLevelUpper2", binaryLevelUpper2_);
    }
    XMLUtils::addChild(doc, tradeNode, "Type1", type1_);
    XMLUtils::addChild(doc, tradeNode, "Type2", type2_);
    XMLUtils::addChild(doc, tradeNode, "Position", position_);
    XMLUtils::appendNode(tradeNode, underlying1_->toXML(doc));
    XMLUtils::appendNode(tradeNode, underlying2_->toXML(doc));
    if (underlying3_)
        XMLUtils::appendNode(tradeNode, underlying3_->toXML(doc));
    if (underlying4_)
        XMLUtils::appendNode(tradeNode, underlying4_->toXML(doc));
    XMLUtils::addChild(doc, tradeNode, "PayCcy", payCcy_);
    return node;
}

} // namespace data
} // namespace ore
