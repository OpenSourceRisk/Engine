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
    dates_ = XMLUtils::getChildrenValues(node, "Dates", "Date");
}

XMLNode* ScheduleDates::toXML(XMLDocument& doc) {
    XMLNode* node = doc.allocNode("Dates");
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
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
    vector<Date> scheduleDates(data.dates().size());
    for (Size i = 0; i < data.dates().size(); i++)
        scheduleDates[i] = parseDate(data.dates()[i]);
    // dummy parameters will not be needed any more from QL 1.12 on
    return Schedule(scheduleDates, calendar, Unadjusted, boost::none, boost::none, boost::none, boost::none,
                    std::vector<bool>(scheduleDates.size() - 1, true));
}

Schedule makeSchedule(const ScheduleRules& data) {
    Calendar calendar = parseCalendar(data.calendar());
    Date startDate = parseDate(data.startDate());
    Date endDate = parseDate(data.endDate());
    // Handle trivial case here
    // dummy parameters will not be needed any more from QL 1.12 on
    if (startDate == endDate)
        return Schedule(vector<Date>(1, startDate), calendar, Unadjusted, boost::none, boost::none, boost::none,
                        boost::none, std::vector<bool>(0));

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
        // 2) Build vector of dates, checking that they match up and
        // that we have a consistant calendar.
        const Schedule& s0 = schedules.front();
        vector<Date> dates = s0.dates();
        Calendar cal = s0.calendar();
        BusinessDayConvention convention = s0.businessDayConvention();
        boost::optional<BusinessDayConvention> termDateConvention = boost::none;
        if (s0.hasTerminationDateBusinessDayConvention())
            termDateConvention = s0.terminationDateBusinessDayConvention();
        boost::optional<Period> tenor = boost::none;
        if (s0.hasTenor())
            tenor = s0.tenor();
        boost::optional<DateGeneration::Rule> rule = boost::none;
        if (s0.hasRule())
            rule = s0.rule();
        boost::optional<bool> endOfMonth = boost::none;
        if (s0.hasEndOfMonth())
            endOfMonth = boost::none;
        std::vector<bool> isRegular(s0.dates().size() - 1, false);
        if (s0.hasIsRegular())
            isRegular = s0.isRegular();
        for (Size i = 1; i < schedules.size(); ++i) {
            const Schedule& s = schedules[i];
            QL_REQUIRE(s.calendar() == cal || s.calendar() == NullCalendar(),
                       "Inconsistant calendar for schedule " << i << " " << s.calendar() << " expected " << cal);
            QL_REQUIRE(dates.back() == s.dates().front(), "Dates mismatch");
            // add them
            dates.insert(dates.end(), s.dates().begin() + 1, s.dates().end());
            // set convention, termDateConvention, tenor, rule to those from last rule based schedule
            // we have to chose one, this seems better than disregarding the information in total?
            if (s.hasTenor() && s.hasRule()) {
                convention = s.businessDayConvention();
                if (s.hasTerminationDateBusinessDayConvention())
                    termDateConvention = s.terminationDateBusinessDayConvention();
                if (s.hasEndOfMonth())
                    endOfMonth = s.endOfMonth();
                tenor = s.tenor();
                rule = s.rule();
            }
            // add isRegular information, if available, otherwise set it to false
            if (s.hasIsRegular()) {
                isRegular.insert(isRegular.end(), s.isRegular().begin(), s.isRegular().end());
            } else {
                for (Size ii = 0; ii < s.dates().size() - 1; ++ii)
                    isRegular.push_back(false);
            }
        }
        // 3) Build schedule
        return Schedule(dates, cal, convention, termDateConvention, tenor, rule, endOfMonth, isRegular);
    }
}
} // namespace data
} // namespace ore
