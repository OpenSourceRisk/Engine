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

#include <ored/utilities/parsers.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void OptionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "OptionData");
    longShort_ = XMLUtils::getChildValue(node, "LongShort", true);
    callPut_ = XMLUtils::getChildValue(node, "OptionType", false);
    payoffType_ = XMLUtils::getChildValue(node, "PayoffType", false);
    style_ = XMLUtils::getChildValue(node, "Style", false);
    noticePeriod_ = XMLUtils::getChildValue(node, "NoticePeriod", false);
    noticeCalendar_ = XMLUtils::getChildValue(node, "NoticeCalendar", false);
    noticeConvention_ = XMLUtils::getChildValue(node, "NoticeConvention", false);
    settlement_ = XMLUtils::getChildValue(node, "Settlement", false);
    settlementMethod_ = XMLUtils::getChildValue(node, "SettlementMethod", false);
    payoffAtExpiry_ = XMLUtils::getChildValueAsBool(node, "PayOffAtExpiry", false);
    premium_ = XMLUtils::getChildValueAsDouble(node, "PremiumAmount", false);
    premiumCcy_ = XMLUtils::getChildValue(node, "PremiumCurrency", false);
    premiumPayDate_ = XMLUtils::getChildValue(node, "PremiumPayDate", false);
    exerciseFeeTypes_.clear();
    exerciseFeeDates_.clear();
    vector<std::reference_wrapper<vector<string>>> attrs;
    attrs.push_back(exerciseFeeTypes_);
    attrs.push_back(exerciseFeeDates_);
    exerciseFees_ = XMLUtils::getChildrenValuesWithAttributes<Real>(node, "ExerciseFees", "ExerciseFee",
                                                                    {"type", "startDate"}, attrs, &parseReal);
    exerciseFeeSettlementPeriod_ = XMLUtils::getChildValue(node, "ExerciseFeeSettlementPeriod", false);
    exerciseFeeSettlementCalendar_ = XMLUtils::getChildValue(node, "ExerciseFeeSettlementCalendar", false);
    exerciseFeeSettlementConvention_ = XMLUtils::getChildValue(node, "ExerciseFeeSettlementConvention", false);
    exercisePrices_ = XMLUtils::getChildrenValuesAsDoubles(node, "ExercisePrices", "ExercisePrice", false);
    exerciseDates_ = XMLUtils::getChildrenValues(node, "ExerciseDates", "ExerciseDate", false);

    automaticExercise_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(node, "AutomaticExercise"))
        automaticExercise_ = parseBool(XMLUtils::getNodeValue(n));

    exerciseData_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(node, "ExerciseData")) {
        exerciseData_ = OptionExerciseData();
        exerciseData_->fromXML(n);
    }

    paymentData_ = boost::none;
    if (XMLNode* n = XMLUtils::getChildNode(node, "PaymentData")) {
        paymentData_ = OptionPaymentData();
        paymentData_->fromXML(n);
    }
}

XMLNode* OptionData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("OptionData");
    XMLUtils::addChild(doc, node, "LongShort", longShort_);
    if (callPut_ != "")
        XMLUtils::addChild(doc, node, "OptionType", callPut_);
    if (payoffType_ != "")
        XMLUtils::addChild(doc, node, "PayoffType", payoffType_);
    if (style_ != "")
        XMLUtils::addChild(doc, node, "Style", style_);
    XMLUtils::addChild(doc, node, "NoticePeriod", noticePeriod_);
    if (noticeCalendar_ != "")
        XMLUtils::addChild(doc, node, "NoticeCalendar", noticeCalendar_);
    if (noticeConvention_ != "")
        XMLUtils::addChild(doc, node, "NoticeConvention", noticeConvention_);
    if (settlement_ != "")
        XMLUtils::addChild(doc, node, "Settlement", settlement_);
    if (settlementMethod_ != "")
        XMLUtils::addChild(doc, node, "SettlementMethod", settlementMethod_);
    XMLUtils::addChild(doc, node, "PayOffAtExpiry", payoffAtExpiry_);
    XMLUtils::addChild(doc, node, "PremiumAmount", premium_);
    XMLUtils::addChild(doc, node, "PremiumCurrency", premiumCcy_);
    XMLUtils::addChild(doc, node, "PremiumPayDate", premiumPayDate_);
    XMLUtils::addChildrenWithOptionalAttributes(doc, node, "ExerciseFees", "ExerciseFee", exerciseFees_,
                                                {"type", "startDate"}, {exerciseFeeTypes_, exerciseFeeDates_});
    if (exerciseFeeSettlementPeriod_ != "")
        XMLUtils::addChild(doc, node, "ExerciseFeeSettlementPeriod", exerciseFeeSettlementPeriod_);
    if (exerciseFeeSettlementCalendar_ != "")
        XMLUtils::addChild(doc, node, "ExerciseFeeSettlementCalendar", exerciseFeeSettlementCalendar_);
    if (exerciseFeeSettlementConvention_ != "")
        XMLUtils::addChild(doc, node, "ExerciseFeeSettlementConvention", exerciseFeeSettlementConvention_);
    XMLUtils::addChildren(doc, node, "ExercisePrices", "ExercisePrice", exercisePrices_);
    XMLUtils::addChildren(doc, node, "ExerciseDates", "ExerciseDate", exerciseDates_);

    if (automaticExercise_)
        XMLUtils::addChild(doc, node, "AutomaticExercise", *automaticExercise_);

    if (exerciseData_) {
        XMLUtils::appendNode(node, exerciseData_->toXML(doc));
    }

    if (paymentData_) {
        XMLUtils::appendNode(node, paymentData_->toXML(doc));
    }

    return node;
}
} // namespace data
} // namespace ore
