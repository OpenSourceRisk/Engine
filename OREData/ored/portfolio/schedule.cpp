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

#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

void ScheduleRules::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Rules");
    startDate_ = XMLUtils::getChildValue(node, "StartDate");
    endDate_ = XMLUtils::getChildValue(node, "EndDate");
    tenor_ = XMLUtils::getChildValue(node, "Tenor");
    calendar_ = XMLUtils::getChildValue(node, "Calendar");
    convention_ = XMLUtils::getChildValue(node, "Convention");
    termConvention_ = XMLUtils::getChildValue(node, "TermConvention");
    rule_ = XMLUtils::getChildValue(node, "Rule");
    endOfMonth_ = XMLUtils::getChildValue(node, "EndOfMonth");
    firstDate_ = XMLUtils::getChildValue(node, "FirstDate");
    lastDate_ = XMLUtils::getChildValue(node, "LastDate");
}

XMLNode* ScheduleRules::toXML(XMLDocument& doc) {
    XMLNode* rules = doc.allocNode("Rules");
    XMLUtils::addChild(doc, rules, "StartDate", startDate_);
    XMLUtils::addChild(doc, rules, "EndDate", endDate_);
    XMLUtils::addChild(doc, rules, "Tenor", tenor_);
    XMLUtils::addChild(doc, rules, "Calendar", calendar_);
    XMLUtils::addChild(doc, rules, "Convention", convention_);
    XMLUtils::addChild(doc, rules, "TermConvention", termConvention_);
    XMLUtils::addChild(doc, rules, "Rule", rule_);
    XMLUtils::addChild(doc, rules, "EndOfMonth", endOfMonth_);
    XMLUtils::addChild(doc, rules, "FirstDate", firstDate_);
    XMLUtils::addChild(doc, rules, "LastDate", lastDate_);
    return rules;
}

void ScheduleDates::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Dates");
    calendar_ = XMLUtils::getChildValue(node, "Calendar");
    convention_ = XMLUtils::getChildValue(node, "Convention");
    tenor_ = XMLUtils::getChildValue(node, "Tenor");
    dates_ = XMLUtils::getChildrenValues(node, "Dates", "Date");
}

XMLNode* ScheduleDates::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Dates");
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
    XMLUtils::addChild(doc, node, "Convention", convention_);
    XMLUtils::addChild(doc, node, "Tenor", tenor_);
    XMLUtils::addChildren(doc, node, "Dates", "Date", dates_);
    return node;
}

void ScheduleData::fromXML(XMLNode* node) {
    for (auto& r : XMLUtils::getChildrenNodes(node, "Rules")) {
        rules_.emplace_back();
        rules_.back().fromXML(r);
    }
    for (auto& d : XMLUtils::getChildrenNodes(node, "Dates")) {
        dates_.emplace_back();
        dates_.back().fromXML(d);
    }
}

XMLNode* ScheduleData::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("ScheduleData");
    for (auto& r : rules_)
        XMLUtils::appendNode(node, r.toXML(doc));
    for (auto& d : dates_)
        XMLUtils::appendNode(node, d.toXML(doc));
    return node;
}

Schedule makeSchedule(const ScheduleDates& data) {
    QL_REQUIRE(data.dates().size() > 0, "Must provide at least 1 date for Schedule");
    Calendar calendar = parseCalendar(data.calendar());
    BusinessDayConvention convention = Unadjusted;
    if (data.convention() != "")
        convention = parseBusinessDayConvention(data.convention());
    boost::optional<Period> tenor = boost::none;
    if (data.tenor() != "")
        tenor = parsePeriod(data.tenor());
    vector<Date> scheduleDates(data.dates().size());
    for (Size i = 0; i < data.dates().size(); i++)
        scheduleDates[i] = calendar.adjust(parseDate(data.dates()[i]), convention);
    return Schedule(scheduleDates, calendar, convention, boost::none, tenor);
}

Schedule makeSchedule(const ScheduleRules& data) {
    Calendar calendar = parseCalendar(data.calendar());
    Date startDate = parseDate(data.startDate());
    Date endDate = parseDate(data.endDate());
    // Handle trivial case here
    if (startDate == endDate)
        return Schedule(vector<Date>(1, startDate), calendar);

    QL_REQUIRE(startDate < endDate, "StartDate " << startDate << " is ahead of EndDate " << endDate);

    Date firstDate = parseDate(data.firstDate());
    Date lastDate = parseDate(data.lastDate());
    Period tenor = parsePeriod(data.tenor());

    // defaults
    BusinessDayConvention bdc = ModifiedFollowing;
    BusinessDayConvention bdcEnd = ModifiedFollowing;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;

    // now check the strings, if they are empty we take defaults
    if (!data.convention().empty())
        bdc = parseBusinessDayConvention(data.convention());
    if (!data.termConvention().empty())
        bdcEnd = parseBusinessDayConvention(data.termConvention());
    else
        bdcEnd = bdc; // except here
    if (!data.rule().empty())
        rule = parseDateGenerationRule(data.rule());
    if (!data.endOfMonth().empty())
        endOfMonth = parseBool(data.endOfMonth());

    return Schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);
}

namespace {
// helper function used in makeSchedule below
template <class T>
void updateData(const std::string& s, T& t, bool& hasT, bool& hasConsistentT, const std::function<T(string)>& parser) {
    if (s != "") {
        T tmp = parser(s);
        if (hasT) {
            hasConsistentT = hasConsistentT && (tmp == t);
        } else {
            t = tmp;
            hasT = true;
        }
    }
}
} // namespace

Schedule makeSchedule(const ScheduleData& data) {
    // build all the date and rule based sub-schedules we have
    vector<Schedule> schedules;
    for (auto& d : data.dates())
        schedules.push_back(makeSchedule(d));
    for (auto& r : data.rules())
        schedules.push_back(makeSchedule(r));
    QL_REQUIRE(!schedules.empty(), "No dates or rules to build Schedule from");
    if (schedules.size() == 1)
        // if we have just one, use that (most common case)
        return schedules.front();
    else {
        // if we have multiple, combine them

        // 1) sort by start date
        std::sort(schedules.begin(), schedules.end(),
                  [](const Schedule& lhs, const Schedule& rhs) -> bool { return lhs.startDate() < rhs.startDate(); });

        // 2) check if meta data is present, and if yes if it is consistent across schedules;
        //    the only exception is the term date convention, this is taken from the last schedule always
        BusinessDayConvention convention;
        Calendar calendar;
        Period tenor;
        DateGeneration::Rule rule;
        bool endOfMonth;
        bool hasCalendar = false, hasConvention = false, hasTenor = false, hasRule = false, hasEndOfMonth = false,
             hasConsistentCalendar = true, hasConsistentConvention = true, hasConsistentTenor = true,
             hasConsistentRule = true, hasConsistentEndOfMonth = true;
        for (auto& d : data.dates()) {
            updateData<Calendar>(d.calendar(), calendar, hasCalendar, hasConsistentCalendar, parseCalendar);
            updateData<BusinessDayConvention>(d.convention(), convention, hasConvention, hasConsistentConvention,
                                              parseBusinessDayConvention);
            updateData<Period>(d.tenor(), tenor, hasTenor, hasConsistentTenor, parsePeriod);
        }
        for (auto& d : data.rules()) {
            updateData<Calendar>(d.calendar(), calendar, hasCalendar, hasConsistentCalendar, parseCalendar);
            updateData<BusinessDayConvention>(d.convention(), convention, hasConvention, hasConsistentConvention,
                                              parseBusinessDayConvention);
            updateData<Period>(d.tenor(), tenor, hasTenor, hasConsistentTenor, parsePeriod);
            updateData<bool>(d.endOfMonth(), endOfMonth, hasEndOfMonth, hasConsistentEndOfMonth, parseBool);
            updateData<DateGeneration::Rule>(d.rule(), rule, hasRule, hasConsistentRule, parseDateGenerationRule);
        }

        // 3) combine dates and fill isRegular flag
        const Schedule& s0 = schedules.front();
        vector<Date> dates = s0.dates();
        std::vector<bool> isRegular(s0.dates().size() - 1, false);
        if (s0.hasIsRegular())
            isRegular = s0.isRegular();
        // will be removed, if next schedule's front date is matching the last date of current schedule
        isRegular.push_back(false);
        for (Size i = 1; i < schedules.size(); ++i) {
            const Schedule& s = schedules[i];
            QL_REQUIRE(dates.back() <= s.dates().front(), "Dates mismatch");
            // if the end points match up, skip one to avoid duplicates, otherwise take both
            Size offset = dates.back() == s.dates().front() ? 1 : 0;
            isRegular.erase(isRegular.end() - offset,
                            isRegular.end()); // correct for superfluous flags from previous schedule
            // add isRegular information, if available, otherwi≈cdse assume irregular periods
            if (s.hasIsRegular()) {
                isRegular.insert(isRegular.end(), s.isRegular().begin(), s.isRegular().end());
            } else {
                for (Size ii = 0; ii < s.dates().size() - 1; ++ii)
                    isRegular.push_back(false);
            }
            if (i < schedules.size() - 1) {
                // will be removed if next schedule's front date is matching last date of current schedule
                isRegular.push_back(false);
            }
            // add the dates
            dates.insert(dates.end(), s.dates().begin() + offset, s.dates().end());
        }

        // 4) Build schedule
        return Schedule(
            dates, hasCalendar && hasConsistentCalendar ? calendar : NullCalendar(),
            hasConvention && hasConsistentConvention ? convention : Unadjusted,
            schedules.back().hasTerminationDateBusinessDayConvention()
                ? boost::optional<BusinessDayConvention>(schedules.back().terminationDateBusinessDayConvention())
                : boost::none,
            hasTenor && hasConsistentTenor ? boost::optional<Period>(tenor) : boost::none,
            hasRule && hasConsistentRule ? boost::optional<DateGeneration::Rule>(rule) : boost::none,
            hasEndOfMonth && hasConsistentEndOfMonth ? boost::optional<bool>(endOfMonth) : boost::none, isRegular);
    }
}
} // namespace data
} // namespace ore
