/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <ored/portfolio/optiondata.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void OptionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "OptionData");
    longShort_ = XMLUtils::getChildValue(node, "LongShort", true);
    callPut_ = XMLUtils::getChildValue(node, "OptionType", true);
    style_ = XMLUtils::getChildValue(node, "Style");
    payoffAtExpiry_ = XMLUtils::getChildValueAsBool(node, "PayOffAtExpiry", false);
    exerciseDates_ = XMLUtils::getChildrenValues(node, "ExerciseDates", "ExerciseDate");
    noticePeriod_ = XMLUtils::getChildValue(node, "NoticePeriod", false);
    settlement_ = XMLUtils::getChildValue(node, "Settlement", false);
    settlementMethod_ = XMLUtils::getChildValue(node, "SettlementMethod", false);
    premium_ = XMLUtils::getChildValueAsDouble(node, "PremiumAmount", false);
    premiumCcy_ = XMLUtils::getChildValue(node, "PremiumCurrency", false);
    premiumPayDate_ = XMLUtils::getChildValue(node, "PremiumPayDate", false);
    exerciseFees_ = XMLUtils::getChildrenValuesAsDoubles(node, "ExerciseFees", "ExerciseFee", false);
    exercisePrices_ = XMLUtils::getChildrenValuesAsDoubles(node, "ExercisePrices", "ExercisePrice", false);
}

XMLNode* OptionData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("OptionData");
    XMLUtils::addChild(doc, node, "LongShort", longShort_);
    XMLUtils::addChild(doc, node, "OptionType", callPut_);
    XMLUtils::addChild(doc, node, "Style", style_);
    XMLUtils::addChild(doc, node, "PayOffAtExpiry", payoffAtExpiry_);
    XMLUtils::addChildren(doc, node, "ExerciseDates", "ExerciseDate", exerciseDates_);
    XMLUtils::addChild(doc, node, "NoticePeriod", noticePeriod_);
    XMLUtils::addChild(doc, node, "Settlement", settlement_);
    XMLUtils::addChild(doc, node, "SettlementMethod", settlementMethod_);
    XMLUtils::addChild(doc, node, "PremiumAmount", premium_);
    XMLUtils::addChild(doc, node, "PremiumCurrency", premiumCcy_);
    XMLUtils::addChild(doc, node, "PremiumPayDate", premiumPayDate_);
    XMLUtils::addChildren(doc, node, "ExerciseFees", "ExerciseFee", exerciseFees_);
    XMLUtils::addChildren(doc, node, "ExercisePrices", "ExerciseFee", exercisePrices_);
    return node;
}
} // namespace data
} // namespace ore
