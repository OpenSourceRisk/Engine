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
    payoffAtExpiry_ = XMLUtils::getChildValueAsBool(node, "PayOffAtExpiry", false, true);
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

    XMLNode* exDatesNode = XMLUtils::getChildNode(node, "ExerciseDates");
    XMLNode* exScheduleNode = XMLUtils::getChildNode(node, "ExerciseSchedule");
    QL_REQUIRE(!(exDatesNode && exScheduleNode),
               "Cannot specify both ExerciseDates and ExerciseSchedule. Only one must be used.");
    if (exDatesNode) {
        exerciseDates_ = XMLUtils::getChildrenValues(node, "ExerciseDates", "ExerciseDate");
    }
    if (exScheduleNode) {
        exerciseDatesSchedule_.fromXML(exScheduleNode);
    }

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

XMLNode* OptionData::toXML(XMLDocument& doc) const {
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

    if (exerciseDatesSchedule_.hasData()) {
        XMLNode* scheduleDataNode = exerciseDatesSchedule_.toXML(doc);
        XMLUtils::setNodeName(doc, scheduleDataNode, "ExerciseSchedule");
        XMLUtils::appendNode(node, scheduleDataNode);
    } else {
        XMLUtils::addChildren(doc, node, "ExerciseDates", "ExerciseDate", exerciseDates_);
    }

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

ExerciseBuilder::ExerciseBuilder(const OptionData& optionData, const std::vector<Leg> legs,
                                 bool removeNoticeDatesAfterLastAccrualStart) {

    // for american style exercise, never remove notice dates after last accrual start

    if (optionData.style() == "American")
        removeNoticeDatesAfterLastAccrualStart = false;

    // only keep a) future exercise dates and b) exercise dates that exercise into a whole
    // accrual period of the underlying; TODO handle exercises into broken periods?

    // determine last accrual start date present in the underlying legs

    Date lastAccrualStartDate = Date::minDate();
    for (auto const& l : legs) {
        for (auto const& c : l) {
            if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c))
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
    if (optionData.exerciseDatesSchedule().hasData()) {
        Schedule schedule = makeSchedule(optionData.exerciseDatesSchedule());
        sortedExerciseDates = schedule.dates();
    } else {
        // For backward compatibility
        for (auto const& d : optionData.exerciseDates())
            sortedExerciseDates.push_back(parseDate(d));
    }
    std::sort(sortedExerciseDates.begin(), sortedExerciseDates.end());

    // check that we have exactly two exercise dates for american style

    QL_REQUIRE(optionData.style() != "American" || sortedExerciseDates.size() == 2,
               "ExerciseBuilder: expected 2 exercise dates for style 'American', got " << sortedExerciseDates.size());

    // build vector of alive exercise dates and corresponding notice dates

    std::vector<bool> isExerciseDateAlive(sortedExerciseDates.size(), false);

    Date today = Settings::instance().evaluationDate();

    for (Size i = 0; i < sortedExerciseDates.size(); i++) {
        Date noticeDate = noticeCal.advance(sortedExerciseDates[i], -noticePeriod, noticeBdc);
        // keep two alive notice dates always for american style exercise
        if (optionData.style() == "American" && i == 0) {
            noticeDate = std::max(today + 1, noticeDate);
            sortedExerciseDates[0] = std::max(today + 1, sortedExerciseDates[0]);
        }
        if (noticeDate > today && (noticeDate <= lastAccrualStartDate || !removeNoticeDatesAfterLastAccrualStart)) {
            isExerciseDateAlive[i] = true;
            noticeDates_.push_back(noticeDate);
            exerciseDates_.push_back(sortedExerciseDates[i]);
            DLOG("Got notice date " << QuantLib::io::iso_date(noticeDate) << " using notice period " << noticePeriod
                                    << ", convention " << noticeBdc << ", calendar " << noticeCal.name()
                                    << " from exercise date " << exerciseDates_.back());
        }
        if (noticeDate > lastAccrualStartDate && removeNoticeDatesAfterLastAccrualStart)
            WLOG("Remove notice date " << ore::data::to_string(noticeDate) << " (exercise date "
                                       << sortedExerciseDates[i] << ") after last accrual start date "
                                       << ore ::data::to_string(lastAccrualStartDate));
    }

    // build exercise instance if we have alive notice dates

    if (!noticeDates_.empty()) {
        if (optionData.style() == "European") {
            QL_REQUIRE(exerciseDates_.size() == 1, "Got 'European' option style, but "
                                                       << exerciseDates_.size()
                                                       << " exercise dates. Should the style be 'Bermudan'?");
            exercise_ = QuantLib::ext::make_shared<EuropeanExercise>(noticeDates_.back());
        } else if (optionData.style() == "Bermudan" || optionData.style().empty()) {
            // Note: empty exercise style defaults to Bermudan for backwards compatibility
            exercise_ = QuantLib::ext::make_shared<BermudanExercise>(noticeDates_);
        } else if (optionData.style() == "American") {
            QL_REQUIRE(noticeDates_.size() == 2, "ExerciseBuilder: internal error, style is american but got "
                                                     << noticeDates_.size() << " notice dates, expected 2.");
            exercise_ = QuantLib::ext::make_shared<AmericanExercise>(noticeDates_.front(), noticeDates_.back(),
                                                             optionData.payoffAtExpiry());
        } else {
            QL_FAIL("ExerciseBuilder: style '"
                    << optionData.style() << "' not recognized. Expected one of 'European', 'Bermudan', 'American'");
        }
    }

    // check if the exercise right was executed and if so set cash settlement amount

    if (optionData.exerciseData()) {
        Date d = optionData.exerciseData()->date();
        Real p = optionData.exerciseData()->price();
        auto nextDate = std::lower_bound(sortedExerciseDates.begin(), sortedExerciseDates.end(), d);
        if (nextDate != sortedExerciseDates.end()) {
            isExercised_ = true;
            exerciseDateIndex_ = std::distance(sortedExerciseDates.begin(), nextDate);
            // Note: we set the exercise date to the notification date here
            exerciseDate_ = optionData.style() == "American" ? d : *nextDate;
            DLOG("Option is exercised, exercise date = " << exerciseDate_);
            if (optionData.settlement() == "Cash") {
                Date cashSettlementDate = d; // default to exercise date
                if (optionData.paymentData()) {
                    if (optionData.paymentData()->rulesBased()) {
                        cashSettlementDate = optionData.paymentData()->calendar().advance(
                            d, optionData.paymentData()->lag(), Days, optionData.paymentData()->convention());
                    } else {
                        auto const& dates = optionData.paymentData()->dates();
                        auto nextDate = std::lower_bound(dates.begin(), dates.end(), d);
                        if (nextDate != dates.end())
                            cashSettlementDate = *nextDate;
                    }
                }
                if (p != Null<Real>())
                    cashSettlement_ = QuantLib::ext::make_shared<QuantLib::SimpleCashFlow>(p, cashSettlementDate);
                DLOG("Option is cash settled, amount " << p << " paid on " << cashSettlementDate);
            }
        }
    }

    // build fee and rebated exercise instance, if any fees are present

    if (!optionData.exerciseFees().empty()) {

        QL_REQUIRE(optionData.style() != "American" || optionData.exerciseFees().size() == 1,
                   "ExerciseBuilder: for style 'American' at most one exercise fee is allowed");

        // build an exercise date "schedule" by adding the maximum possible date at the end

        std::vector<Date> exDatesPlusInf(sortedExerciseDates);
        exDatesPlusInf.push_back(Date::maxDate());
        vector<double> allRebates = buildScheduledVectorNormalised(optionData.exerciseFees(),
                                                                   optionData.exerciseFeeDates(), exDatesPlusInf, 0.0);

        // flip the sign of the fee to get a rebate

        for (auto& r : allRebates)
            r = -r;

        vector<string> feeType = buildScheduledVectorNormalised<string>(
            optionData.exerciseFeeTypes(), optionData.exerciseFeeDates(), exDatesPlusInf, "");

        // convert relative to absolute fees if required

        for (Size i = 0; i < allRebates.size(); ++i) {

            // default to Absolute

            if (feeType[i].empty())
                feeType[i] = "Absolute";

            if (feeType[i] == "Percentage") {

                // get next coupon after exercise to determine relevant notional

                std::set<std::pair<Date, Real>> notionals;
                for (auto const& l : legs) {
                    for (auto const& c : l) {
                        if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
                            if (cpn->accrualStartDate() >= sortedExerciseDates[i])
                                notionals.insert(std::make_pair(cpn->accrualStartDate(), cpn->nominal()));
                        }
                    }
                }

                if (notionals.empty())
                    allRebates[i] = 0.0; // notional is zero
                else {
                    Real feeNotional = notionals.begin()->second;
                    DLOG("Convert percentage rebate "
                         << allRebates[i] << " to absolute rebate " << allRebates[i] * feeNotional << " using nominal "
                         << feeNotional << " for exercise date " << QuantLib::io::iso_date(sortedExerciseDates[i]));
                    allRebates[i] *= feeNotional; // multiply percentage fee by relevant notional
                }

            } else {
                QL_REQUIRE(feeType[i] == "Absolute", "fee type must be Absolute or Relative");
            }
        }

        // set fee settlement conventions

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

        // set fee settlement amount if option is exercised

        if (isExercised_) {
            feeSettlement_ = QuantLib::ext::make_shared<QuantLib::SimpleCashFlow>(
                -allRebates[exerciseDateIndex_], feeSettlCal.advance(exerciseDate_, feeSettlPeriod, feeSettlBdc));
            DLOG("Settlement fee for exercised option is " << feeSettlement_->amount() << " paid on "
                                                           << feeSettlement_->date() << ".");
        }

        // update exercise instance with rebate information

        if (exercise_ != nullptr) {
            vector<double> rebates;
            for (Size i = 0; i < sortedExerciseDates.size(); ++i) {
                if (isExerciseDateAlive[i])
                    rebates.push_back(allRebates[i]);
            }
            if (optionData.style() == "American") {
                // Note: we compute the settl date relative to notification, not exercise here
                exercise_ = QuantLib::ext::make_shared<QuantExt::RebatedExercise>(*exercise_, rebates.front(), feeSettlPeriod,
                                                                          feeSettlCal, feeSettlBdc);
                auto dbgEx = QuantLib::ext::static_pointer_cast<QuantExt::RebatedExercise>(exercise_);
                DLOG("Got rebate " << dbgEx->rebate(0) << " for American exercise with fee settle period "
                                   << feeSettlPeriod << ", cal " << feeSettlCal << ", bdc " << feeSettlBdc);
            } else {
                exercise_ = QuantLib::ext::make_shared<QuantExt::RebatedExercise>(*exercise_, exerciseDates_, rebates,
                                                                          feeSettlPeriod, feeSettlCal, feeSettlBdc);
                auto dbgEx = QuantLib::ext::static_pointer_cast<QuantExt::RebatedExercise>(exercise_);
                for (Size i = 0; i < exerciseDates_.size(); ++i) {
                    DLOG("Got rebate " << dbgEx->rebate(i) << " with payment date "
                                       << QuantLib::io::iso_date(dbgEx->rebatePaymentDate(i))
                                       << " (exercise date=" << QuantLib::io::iso_date(exerciseDates_[i])
                                       << ") using rebate settl period " << feeSettlPeriod << ", calendar "
                                       << feeSettlCal << ", convention " << feeSettlBdc);
                }
            }
        }
    } // if exercise fees are given
} // ExerciseBuilder()

} // namespace data
} // namespace ore
