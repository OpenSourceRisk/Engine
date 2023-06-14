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

#include <ored/portfolio/legdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/instruments/rebatedexercise.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void OptionData::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "OptionData");
    longShort_ = XMLUtils::getChildValue(node, "LongShort", true);
    callPut_ = XMLUtils::getChildValue(node, "OptionType", false);
    payoffType_ = XMLUtils::getChildValue(node, "PayoffType", false);
    payoffType2_ = XMLUtils::getChildValue(node, "PayoffType2", false);
    style_ = XMLUtils::getChildValue(node, "Style", false);
    noticePeriod_ = XMLUtils::getChildValue(node, "NoticePeriod", false);
    noticeCalendar_ = XMLUtils::getChildValue(node, "NoticeCalendar", false);
    noticeConvention_ = XMLUtils::getChildValue(node, "NoticeConvention", false);
    settlement_ = XMLUtils::getChildValue(node, "Settlement", false);
    settlementMethod_ = XMLUtils::getChildValue(node, "SettlementMethod", false);
    payoffAtExpiry_ = XMLUtils::getChildValueAsBool(node, "PayOffAtExpiry", false);
    premiumData_.fromXML(node);
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
    if (payoffType2_ != "")
        XMLUtils::addChild(doc, node, "PayoffType2", payoffType2_);
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
    XMLUtils::appendNode(node, premiumData_.toXML(doc));
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

ExerciseBuilder::ExerciseBuilder(const OptionData& optionData, const std::vector<Leg> legs) {

    // only keep a) future exercise dates and b) exercise dates that exercise into a whole
    // accrual period of the underlying; TODO handle exercises into broken periods?

    // determine last accrual start date present in the underlying legs

    Date lastAccrualStartDate = Date::minDate();
    for (auto const& l : legs) {
        for (auto const& c : l) {
            if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c))
                lastAccrualStartDate = std::max(lastAccrualStartDate, cpn->accrualStartDate());
        }
    }

    // get notice period, calendar, bdc

    Period noticePeriod = optionData.noticePeriod().empty() ? 0 * Days : parsePeriod(optionData.noticePeriod());
    Calendar noticeCal =
        optionData.noticeCalendar().empty() ? NullCalendar() : parseCalendar(optionData.noticeCalendar());
    BusinessDayConvention noticeBdc =
        optionData.noticeConvention().empty() ? Unadjusted : parseBusinessDayConvention(optionData.noticeConvention());

    // build vector of sorted exercise dates

    std::vector<QuantLib::Date> sortedExerciseDates;
    for (auto const& d : optionData.exerciseDates())
        sortedExerciseDates.push_back(parseDate(d));
    std::sort(sortedExerciseDates.begin(), sortedExerciseDates.end());

    // build vector of alive exercise dates and corresponding native dates

    std::vector<bool> isExerciseDateAlive(sortedExerciseDates.size(), false);

    for (Size i = 0; i < sortedExerciseDates.size(); i++) {
        Date noticeDate = noticeCal.advance(sortedExerciseDates[i], -noticePeriod, noticeBdc);
        if (noticeDate > Settings::instance().evaluationDate() && noticeDate <= lastAccrualStartDate) {
            isExerciseDateAlive[i] = true;
            noticeDates_.push_back(noticeDate);
            exerciseDates_.push_back(sortedExerciseDates[i]);
            DLOG("Got notice date " << QuantLib::io::iso_date(noticeDate) << " using notice period " << noticePeriod
                                    << ", convention " << noticeBdc << ", calendar " << noticeCal.name()
                                    << " from exercise date " << exerciseDates_.back());
        }
        if (noticeDate > lastAccrualStartDate)
            WLOG("Remove notice date " << ore::data::to_string(noticeDate) << " (exercise date "
                                       << sortedExerciseDates[i] << ") after last accrual start date "
                                       << ore ::data::to_string(lastAccrualStartDate));
    }

    // if we do not have any notice dates left, we are done (a nullptr will be returned by exercise())

    if (noticeDates_.empty())
        return;

    // build exercise instance

    if (optionData.style() == "European")
        exercise_ = boost::make_shared<EuropeanExercise>(sortedExerciseDates.back());
    else 
        exercise_ = boost::make_shared<BermudanExercise>(noticeDates_);


    // build fee and rebated exercise instance, if any fees are present

    if (!optionData.exerciseFees().empty()) {

        // build an exercise date "schedule" by adding the maximum possible date at the end

        std::vector<Date> exDatesPlusInf(sortedExerciseDates);
        exDatesPlusInf.push_back(Date::maxDate());
        vector<double> allRebates = buildScheduledVectorNormalised(optionData.exerciseFees(),
                                                                   optionData.exerciseFeeDates(), exDatesPlusInf, 0.0);

        // filter on alive rebates, so that we can a vector of rebates corresponding to the exerciseDates vector

        vector<double> rebates;
        for (Size i = 0; i < sortedExerciseDates.size(); ++i) {
            if (isExerciseDateAlive[i])
                rebates.push_back(allRebates[i]);
        }

        // flip the sign of the fee to get a rebate

        for (auto& r : rebates)
            r = -r;
        vector<string> feeType = buildScheduledVectorNormalised<string>(
            optionData.exerciseFeeTypes(), optionData.exerciseFeeDates(), exDatesPlusInf, "");

        // convert relative to absolute fees if required

        for (Size i = 0; i < rebates.size(); ++i) {

            // default to Absolute

            if (feeType[i].empty())
                feeType[i] = "Absolute";

            if (feeType[i] == "Percentage") {

                // get next coupon after exercise to determine relevant notional

                std::set<std::pair<Date, Real>> notionals;
                for (auto const& l : legs) {
                    for (auto const& c : l) {
                        if (auto cpn = boost::dynamic_pointer_cast<Coupon>(c)) {
                            if (cpn->accrualStartDate() >= exerciseDates_[i])
                                notionals.insert(std::make_pair(cpn->accrualStartDate(), cpn->nominal()));
                        }
                    }
                }

                if (notionals.empty())
                    rebates[i] = 0.0; // notional is zero
                else {
                    Real feeNotional = notionals.begin()->second;
                    DLOG("Convert percentage rebate "
                         << rebates[i] << " to absolute rebate " << rebates[i] * feeNotional << " using nominal "
                         << feeNotional << " for exercise date " << QuantLib::io::iso_date(exerciseDates_[i]));
                    rebates[i] *= feeNotional; // multiply percentage fee by relevant notional
                }

            } else {
                QL_REQUIRE(feeType[i] == "Absolute", "fee type must be Absolute or Relative");
            }
        }

        Period feeSettlPeriod = optionData.exerciseFeeSettlementPeriod().empty()
                                    ? 0 * Days
                                    : parsePeriod(optionData.exerciseFeeSettlementPeriod());

        Calendar feeSettlCal = optionData.exerciseFeeSettlementCalendar().empty()
                                   ? NullCalendar()
                                   : parseCalendar(optionData.exerciseFeeSettlementCalendar());

        BusinessDayConvention feeSettlBdc =
            optionData.exerciseFeeSettlementConvention().empty()
                ? Unadjusted
                : parseBusinessDayConvention(optionData.exerciseFeeSettlementConvention());

        exercise_ = boost::make_shared<QuantExt::RebatedExercise>(*exercise_, exerciseDates_, rebates, feeSettlPeriod,
                                                                  feeSettlCal, feeSettlBdc);

        // log rebates

        auto dbgEx = boost::static_pointer_cast<QuantExt::RebatedExercise>(exercise_);

        for (Size i = 0; i < exerciseDates_.size(); ++i) {
            DLOG("Got rebate " << dbgEx->rebate(i) << " with payment date "
                               << QuantLib::io::iso_date(dbgEx->rebatePaymentDate(i)) << " (exercise date="
                               << QuantLib::io::iso_date(exerciseDates_[i]) << ") using rebate settl period "
                               << feeSettlPeriod << ", calendar " << feeSettlCal << ", convention " << feeSettlBdc);
        }
    }
}

} // namespace data
} // namespace ore
