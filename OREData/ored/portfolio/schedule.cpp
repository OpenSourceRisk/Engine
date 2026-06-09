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
#include <ored/utilities/to_string.hpp>
#include <set>

using namespace QuantLib;

namespace ore {
namespace data {

namespace {
std::vector<Date> everyWeekDayDates(const Date& startDate, const Date& endDate, const Date& firstDate, const QuantLib::Weekday weekday) {
    std::vector<Date> result;
    if (firstDate != Date())
        result.push_back(firstDate);
    Date d = startDate;
    while (d <= endDate && (d.weekday() != weekday || d < firstDate)) {
        ++d;
    }
    if (d.weekday() == weekday && (result.empty() || result.back() != d))
        result.push_back(d);
    while (d + 7 <= endDate) {
        d += 7;
        result.push_back(d);
    }
    return result;
}

std::vector<Date> weeklyDates(const Date& startDate, const Date& endDate, const Date& firstDate,
                                       bool includeWeekend = false) {
    QuantLib::Weekday weekday = includeWeekend ? QuantLib::Sunday : QuantLib::Friday;
    // We want the first period to span from
    //  [startDate, first Friday/SunDay following startDate]
    // or
    //  [firstDate, first Friday/SunDay following firstDate]
    Date effectiveFirstDate = firstDate == Date() ? startDate : firstDate;
    auto dates = everyWeekDayDates(startDate, endDate, effectiveFirstDate, weekday);
    // Handle broken period
    if (!dates.empty()) {
        // If startDate/first Date falls on end of week,
        // the first period is consist of only one day, so first periods should be
        // [startDate, startDate], [startDate+1, next end of the week], ...
        if (effectiveFirstDate.weekday() == weekday) {
            dates.insert(dates.begin(), effectiveFirstDate);
        }
        // add the enddate if the enddate doesnt fall on friday, last broken period
        if (dates.back() < endDate) {
            dates.push_back(endDate);
        }
    }
    return dates;
}

} // namespace

void ScheduleRules::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Rules");
    startDate_ = XMLUtils::getChildValue(node, "StartDate");
    endDate_ = XMLUtils::getChildValue(node, "EndDate", false);
    adjustEndDateToPreviousMonthEnd_ = XMLUtils::getChildValueAsBool(node, "AdjustEndDateToPreviousMonthEnd", false, false);
    if (adjustEndDateToPreviousMonthEnd_ && !endDate_.empty()){
        auto ed = parseDate(endDate_);
        while(!Date::isEndOfMonth(ed))ed--;
        endDate_ = to_string(ed);
    }
    tenor_ = XMLUtils::getChildValue(node, "Tenor") == "1T" ? "0D" : XMLUtils::getChildValue(node, "Tenor");
    was1T_ = XMLUtils::getChildValue(node, "Tenor") == "1T" ? true : false;
    calendar_ = XMLUtils::getChildValue(node, "Calendar");
    convention_ = XMLUtils::getChildValue(node, "Convention");
    termConvention_ = XMLUtils::getChildValue(node, "TermConvention", false);
    if (termConvention_.empty()) {
        termConvention_ = convention_;
    }
    rule_ = XMLUtils::getChildValue(node, "Rule");
    endOfMonth_ = XMLUtils::getChildValue(node, "EndOfMonth");
    endOfMonthConvention_ = XMLUtils::getChildValue(node, "EndOfMonthConvention");
    firstDate_ = XMLUtils::getChildValue(node, "FirstDate");
    lastDate_ = XMLUtils::getChildValue(node, "LastDate");
    removeFirstDate_ = XMLUtils::getChildValueAsBool(node, "RemoveFirstDate", false, false);
    removeLastDate_ = XMLUtils::getChildValueAsBool(node, "RemoveLastDate", false, false);
}

XMLNode* ScheduleRules::toXML(XMLDocument& doc) const {
    XMLNode* rules = doc.allocNode("Rules");
    XMLUtils::addChild(doc, rules, "StartDate", startDate_);
    if (!endDate_.empty())
        XMLUtils::addChild(doc, rules, "EndDate", endDate_);
    XMLUtils::addChild(doc, rules, "Tenor", was1T_ ? "1T" : tenor_);
    XMLUtils::addChild(doc, rules, "Calendar", calendar_);
    XMLUtils::addChild(doc, rules, "Convention", convention_);
    XMLUtils::addChild(doc, rules, "TermConvention", termConvention_);
    XMLUtils::addChild(doc, rules, "Rule", rule_);
    XMLUtils::addChild(doc, rules, "EndOfMonth", endOfMonth_);
    if (!endOfMonthConvention_.empty())
        XMLUtils::addChild(doc, rules, "EndOfMonthConvention", endOfMonthConvention_);
    XMLUtils::addChild(doc, rules, "FirstDate", firstDate_);
    XMLUtils::addChild(doc, rules, "LastDate", lastDate_);
    if(removeFirstDate_)
        XMLUtils::addChild(doc,rules,"RemoveFirstDate", removeFirstDate_);
    if(removeLastDate_)
        XMLUtils::addChild(doc,rules,"RemoveLastDate", removeLastDate_);
    return rules;
}

void ScheduleDates::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Dates");
    calendar_ = XMLUtils::getChildValue(node, "Calendar");
    convention_ = XMLUtils::getChildValue(node, "Convention");
    tenor_ = XMLUtils::getChildValue(node, "Tenor") == "1T" ? "0D" : XMLUtils::getChildValue(node, "Tenor");
    was1T_ = XMLUtils::getChildValue(node, "Tenor") == "1T" ? true : false;
    endOfMonth_ = XMLUtils::getChildValue(node, "EndOfMonth");
    endOfMonthConvention_ = XMLUtils::getChildValue(node, "EndOfMonthConvention");
    dates_ = XMLUtils::getChildrenValues(node, "Dates", "Date");
    includeDuplicateDates_ = XMLUtils::getChildValueAsBool(node, "IncludeDuplicateDates", false, false);
}

XMLNode* ScheduleDates::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Dates");
    XMLUtils::addChild(doc, node, "Calendar", calendar_);
    if (!convention_.empty())
        XMLUtils::addChild(doc, node, "Convention", convention_);
    XMLUtils::addChild(doc, node, "Tenor", was1T_ ? "1T" : tenor_);
    if (!endOfMonth_.empty())
        XMLUtils::addChild(doc, node, "EndOfMonth", endOfMonth_);
    if (!endOfMonthConvention_.empty())
        XMLUtils::addChild(doc, node, "EndOfMonthConvention", endOfMonthConvention_);
    XMLUtils::addChildren(doc, node, "Dates", "Date", dates_);
    if (includeDuplicateDates_){
        XMLUtils::addChild(doc, node, "IncludeDuplicateDates", includeDuplicateDates_);
    }
    return node;
}

void ScheduleDerived::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "Derived");
    baseSchedule_ = XMLUtils::getChildValue(node, "BaseSchedule");
    shift_ = XMLUtils::getChildValue(node, "Shift", false);
    calendar_ = XMLUtils::getChildValue(node, "Calendar", false);
    convention_ = XMLUtils::getChildValue(node, "Convention", false);
    removeFirstDate_ = XMLUtils::getChildValueAsBool(node, "RemoveFirstDate", false, false);
    removeLastDate_ = XMLUtils::getChildValueAsBool(node, "RemoveLastDate", false, false);
    if (auto tmp = XMLUtils::getChildNode(node, "ShiftUnit"))
        shiftUnit_ = parseDateDeltaUnit(XMLUtils::getNodeValue(tmp));
    if (auto tmp = XMLUtils::getChildNode(node, "ShiftAnchor"))
        shiftAnchor_ = parseDateDeltaAnchor(XMLUtils::getNodeValue(tmp));
}

XMLNode* ScheduleDerived::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("Derived");
    XMLUtils::addChild(doc, node, "BaseSchedule", baseSchedule_);
    if (!shift_.empty())
        XMLUtils::addChild(doc, node, "Shift", shift_);
    if (!calendar_.empty())
        XMLUtils::addChild(doc, node, "Calendar", calendar_);
    if (!convention_.empty())
        XMLUtils::addChild(doc, node, "Convention", convention_);
    if(removeFirstDate_)
        XMLUtils::addChild(doc, node, "RemoveFirstDate", removeFirstDate_);
    if(removeLastDate_)
        XMLUtils::addChild(doc, node, "RemoveLastDate", removeLastDate_);
    if (shiftUnit_)
        XMLUtils::addChild(doc, node, "ShiftUnit", to_string(*shiftUnit_));
    if (shiftAnchor_)
        XMLUtils::addChild(doc, node, "ShiftAnchor", to_string(*shiftAnchor_));
    return node;
}

vector<string> ScheduleData::baseScheduleNames() {
    vector<string> baseScheduleNames;
    for (auto& dv : derived_)
        baseScheduleNames.push_back(dv.baseSchedule());
    return baseScheduleNames;
}

void ScheduleData::fromXML(XMLNode* node) {
    QL_REQUIRE(node, "ScheduleData::fromXML(): no node given");
    name_ = XMLUtils::getNodeName(node);
    for (auto& r : XMLUtils::getChildrenNodes(node, "Rules")) {
        rules_.emplace_back();
        rules_.back().fromXML(r);
    }
    for (auto& d : XMLUtils::getChildrenNodes(node, "Dates")) {
        dates_.emplace_back();
        dates_.back().fromXML(d);
    }
    for (auto& dv : XMLUtils::getChildrenNodes(node, "Derived")) {
        derived_.emplace_back();
        derived_.back().fromXML(dv);
        if (!hasDerived_)
            hasDerived_ = true;
    }
}

XMLNode* ScheduleData::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("ScheduleData");
    for (auto& r : rules_)
        XMLUtils::appendNode(node, r.toXML(doc));
    for (auto& d : dates_)
        XMLUtils::appendNode(node, d.toXML(doc));
    for (auto& dv : derived_)
        XMLUtils::appendNode(node, dv.toXML(doc));
    return node;
}

void ScheduleBuilder::add(Schedule& schedule, const ScheduleData& data) {
    string name = data.name();
    schedules_.insert(pair<string, pair<ScheduleData, Schedule&>>({name, {data, schedule}}));
}

void ScheduleBuilder::makeSchedules(const Date& openEndDateReplacement, bool unadjusted) {
    BaseScheduleCache builtSchedules;
    map<string, ScheduleData> derivedSchedules;

    // First, we build all the rules-based and dates-based schedules
    for (auto& s : schedules_) {
        string schName = s.first;
        ScheduleData& schData = s.second.first;
        Schedule& sch = s.second.second;

        if (!schData.hasDerived()) {
            sch = makeSchedule(schData, openEndDateReplacement, {}, unadjusted);
            builtSchedules[schName] = { schData, sch };
        } else {
            derivedSchedules[schName] = schData;
        }
    }

    // We then keep looping through the list of derived schedules and building these from the list of built schedules.
    bool calculated;
    while (derivedSchedules.size() > 0) {
        calculated = false;
        for (auto& ds : derivedSchedules) {
            string dsName = ds.first;
            ScheduleData& dsSchedData = ds.second;
            vector<string> baseNames = dsSchedData.baseScheduleNames();
            for (string& bn : baseNames) {
                QL_REQUIRE(builtSchedules.find(bn) != builtSchedules.end(), "Could not find base schedule \" " <<
                    bn << "\" for derived schedule \" " << dsName << "\"");
            }
            Schedule schedule;
            schedule = makeSchedule(dsSchedData, openEndDateReplacement, builtSchedules);
            schedules_.find(dsName)->second.second = schedule;
            builtSchedules[dsName] = { dsSchedData, schedule };
            derivedSchedules.erase(dsName);
            calculated = true;
            break;
        }

        // If we go through the whole list without having built a schedule, then assume that we cannot build them
        // anymore.
        if (!calculated) {
            for (auto& ds : derivedSchedules)
                ALOG("makeSchedules(): could not find base schedule \"" << ds.first << "\"");
            QL_FAIL("makeSchedules(): failed to build at least one derived schedule");
            break;
        }
    }
}

Schedule makeSchedule(const ScheduleDates& data, bool unadjusted) {
    QL_REQUIRE(data.dates().size() > 0, "Must provide at least 1 date for Schedule");
    Calendar calendar = unadjusted ? NullCalendar() : parseCalendar(data.calendar());
    BusinessDayConvention convention = unadjusted ? Unadjusted :
        (!data.convention().empty() ? parseBusinessDayConvention(data.convention()) : ModifiedFollowing);
    // Avoid compiler warning on gcc
    // https://www.boost.org/doc/libs/1_74_0/libs/optional/doc/html/boost_optional/tutorial/
    // gotchas/false_positive_with__wmaybe_uninitialized.html
    QuantLib::ext::optional<Period> tenor = QuantLib::ext::nullopt;
    if (!data.tenor().empty())
        tenor = parsePeriod(data.tenor());
    bool endOfMonth = false;
    if (!data.endOfMonth().empty())
        endOfMonth = parseBool(data.endOfMonth());
    ext::optional<BusinessDayConvention> endOfMonthConvention = QuantLib::ext::nullopt;
    if (!data.endOfMonthConvention().empty() && !unadjusted)
        endOfMonthConvention = parseBusinessDayConvention(data.endOfMonthConvention());

    // Ensure that Schedule ctor is passed a vector of unique ordered dates.
    std::vector<Date> dates;
    for (const auto& d : data.dates()) {
        dates.push_back(calendar.adjust(parseDate(d), convention));
    }
    std::sort(dates.begin(), dates.end());

    if (!data.includeDuplicateDates()) {
        auto last = std::unique(dates.begin(), dates.end());
        dates.erase(last, dates.end());
    }

    return QuantLib::Schedule(dates, calendar, convention, QuantLib::ext::nullopt, tenor, QuantLib::ext::nullopt,
        endOfMonth, vector<bool>(0), false, false, endOfMonthConvention);
}

Schedule makeSchedule(const ScheduleDerived& data, const pair<ScheduleData, Schedule>& baseScheduleInfo,
    const Date& openEndDateReplacement, bool unadjusted) {

    const string& strCal = data.calendar();
    if (strCal.empty())
        DLOG("No calendar provided in Schedule, attempting to use a null calendar.");
    auto calendar = strCal.empty() || unadjusted ? NullCalendar() : parseCalendar(strCal);

    const string& strConv = data.convention();
    auto convention = strConv.empty() || unadjusted ? BusinessDayConvention::Unadjusted :
        parseBusinessDayConvention(strConv);

    const string& strShift = data.shift();
    auto shift = strShift.empty() ? Period(0, Days) : parsePeriod(strShift);

    // If shift unit is calendar days, need to make sure that the shift is in day units.
    const auto& shiftUnit = data.shiftUnit();
    if (shiftUnit && *shiftUnit == QuantExt::DateDeltaUnit::CalendarDays) {
        QL_REQUIRE(shift.units() == Days, "makeSchedule: when making derived schedule, the shift unit is calendar "
            "days but the shift does not have day units, it has " << shift.units() << ".");
    }

    // If shift anchor is unadjusted, we build a temporary unadjusted version of the base schedule to shift from.
    ext::optional<Schedule> unadjustedSchedule;
    const auto& shiftAnchor = data.shiftAnchor();
    if (shiftAnchor && *shiftAnchor == QuantExt::DateDeltaAnchor::Unadjusted) {
        unadjustedSchedule = Schedule();
        ScheduleBuilder scheduleBuilder;
        scheduleBuilder.add(*unadjustedSchedule, baseScheduleInfo.first);
        scheduleBuilder.makeSchedules(openEndDateReplacement, true);
    }

    const Schedule& baseSchedule = baseScheduleInfo.second;
    const Schedule& anchorSchedule = unadjustedSchedule ? *unadjustedSchedule : baseSchedule;

    const vector<Date>& baseDates = anchorSchedule.dates();
    vector<Date> derivedDates;
    derivedDates.reserve(baseDates.size());
    for (const Date& d : baseDates) {
        if (shiftUnit && *shiftUnit == QuantExt::DateDeltaUnit::CalendarDays) {
            derivedDates.push_back(calendar.adjust(d + shift, convention));
        } else {
            derivedDates.push_back(calendar.advance(d, shift, convention));
        }
    }

    ext::optional<BusinessDayConvention> endOfMonthConvention = QuantLib::ext::nullopt;
    if (baseSchedule.hasEndOfMonthBusinessDayConvention() && !unadjusted)
        endOfMonthConvention = baseSchedule.endOfMonthBusinessDayConvention();

    return QuantLib::Schedule(
        vector<Date>(derivedDates.begin(), derivedDates.end()), calendar, convention, ext::nullopt,
        baseSchedule.hasTenor() ? ext::optional<Period>(baseSchedule.tenor()) : ext::nullopt, ext::nullopt,
        baseSchedule.hasEndOfMonth() ? ext::optional<bool>(baseSchedule.endOfMonth()) : ext::nullopt,
        std::vector<bool>(0), data.removeFirstDate(), data.removeLastDate(), endOfMonthConvention);
}

Schedule makeSchedule(const ScheduleRules& data, const Date& openEndDateReplacement, bool unadjusted) {
    QL_REQUIRE(!data.endDate().empty() || openEndDateReplacement != Null<Date>(),
               "makeSchedule(): Schedule does not have an end date, this is not supported in this context / for this "
               "trade type. Please provide an end date.");
    QL_REQUIRE(!data.endDate().empty() || data.lastDate().empty(),
               "makeSchedule(): If no end date is given, a last date is not allowed either. Please remove the last "
               "date from the schedule.");
    Calendar calendar = unadjusted ? NullCalendar() : parseCalendar(data.calendar());
    if (calendar == NullCalendar() && !unadjusted)
        DLOG("No calendar provided in Schedule, attempting to use a null calendar.");
    Date startDate = parseDate(data.startDate());
    Date endDate = data.endDate().empty() ? openEndDateReplacement : parseDate(data.endDate());
    // Handle trivial case here
    if (startDate == endDate)
        return Schedule(vector<Date>(1, startDate), calendar);

    QL_REQUIRE(startDate < endDate, "StartDate " << startDate << " is ahead of EndDate " << endDate);

    Date firstDate = parseDate(data.firstDate());
    Date lastDate = parseDate(data.lastDate());
    if (firstDate != Date() && lastDate != Date())
        QL_REQUIRE(firstDate <= lastDate, "Schedule::makeSchedule firstDate must be before lastDate");

    Period tenor = parsePeriod(data.tenor());

    // defaults
    BusinessDayConvention bdc = unadjusted ? Unadjusted : ModifiedFollowing;
    BusinessDayConvention bdcEnd = unadjusted ? Unadjusted : ModifiedFollowing;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    ext::optional<BusinessDayConvention> endOfMonthConvention = QuantLib::ext::nullopt;

    // now check the strings, if they are empty we take defaults
    if (!data.convention().empty() && !unadjusted)
        bdc = parseBusinessDayConvention(data.convention());
    if (!data.termConvention().empty() && !unadjusted)
        bdcEnd = parseBusinessDayConvention(data.termConvention());
    else
        bdcEnd = bdc; // except here

    if (!data.endOfMonth().empty())
        endOfMonth = parseBool(data.endOfMonth());
    if (!data.endOfMonthConvention().empty() && !unadjusted)
        endOfMonthConvention = parseBusinessDayConvention(data.endOfMonthConvention());

    if (!data.rule().empty()) {

        // handle special rules outside the QuantLib date generation rules

        if (data.rule() == "EveryThursday") {
            auto dates = everyWeekDayDates(startDate, endDate, firstDate, QuantLib::Thursday);
            for (auto& d : dates)
                d = calendar.adjust(d, bdc);
            return QuantLib::Schedule(dates, calendar, bdc, bdcEnd, tenor, rule, endOfMonth, std::vector<bool>(0),
                false, false, endOfMonthConvention);
        } else if (data.rule() == "BusinessWeek" || data.rule() == "CalendarWeek") {
            auto dates = weeklyDates(startDate, endDate, firstDate, data.rule() == "CalendarWeek");
            for (auto& d : dates)
                d = calendar.adjust(d, bdc);
            return QuantLib::Schedule(dates, calendar, bdc, bdcEnd, tenor, rule, endOfMonth, std::vector<bool>(0),
                data.removeFirstDate(), data.removeLastDate(), endOfMonthConvention);
        }

        // parse rule for further processing below

        rule = parseDateGenerationRule(data.rule());
    }

    // handling of date generation rules that require special adjustments

    if ((rule == DateGeneration::CDS || rule == DateGeneration::CDS2015) &&
        (firstDate != Null<Date>() || lastDate != Null<Date>())) {
        // Special handling of first date and last date in combination with CDS and CDS2015 rules:
        // To be able to construct CDS schedules with front or back stub periods, we overwrite the
        // first (last) date of the schedule built in QL with a given first (last) date
        // The schedule builder in QL itself is not capable of doing this, it just throws an exception
        // if a first (last) date is given in combination with a CDS / CDS2015 date generation rule.
        std::vector<Date> dates = QuantLib::Schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth,
            Date(), Date(), false, false, endOfMonthConvention).dates();
        QL_REQUIRE(!dates.empty(),
                   "got empty CDS or CDS2015 schedule, startDate = " << startDate << ", endDate = " << endDate);
        if (firstDate != Date())
            dates.front() = firstDate;
        if (lastDate != Date())
            dates.back() = lastDate;
        return QuantLib::Schedule(dates, calendar, bdc, bdcEnd, tenor, rule, endOfMonth, std::vector<bool>(0),
                        data.removeFirstDate(), data.removeLastDate(), endOfMonthConvention);
    }

    // default handling (QuantLib scheduler)

    return QuantLib::Schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate,
                              data.removeFirstDate(), data.removeLastDate(), endOfMonthConvention);
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
// local wrapper function to get around optional parameter in parseCalendar
Calendar parseCalendarTemp(const string& s) { return parseCalendar(s); }
} // namespace

Schedule makeSchedule(const ScheduleData& data, const Date& openEndDateReplacement,
    const BaseScheduleCache& baseSchedules, bool unadjusted) {

    if(!data.hasData())
        return Schedule();
    // only the last rule-based schedule is allowed to have an open end date, check this
    for (Size i = 1; i < data.rules().size(); ++i) {
        QL_REQUIRE(!data.rules()[i - 1].endDate().empty(),
                   "makeSchedule(): only last schedule is allowed to have an open end date");
    }
    // build all the date and rule based sub-schedules we have
    vector<Schedule> schedules;
    for (auto& d : data.dates())
        schedules.push_back(makeSchedule(d, unadjusted));
    for (auto& r : data.rules())
        schedules.push_back(makeSchedule(r, openEndDateReplacement, unadjusted));
    if (!baseSchedules.empty())
        for (auto& dv : data.derived()) {
            auto baseSchedule = baseSchedules.find(dv.baseSchedule());
            QL_REQUIRE(baseSchedule != baseSchedules.end(), "makeSchedule(): could not find base schedule \"" <<
                dv.baseSchedule() << "\"");
            schedules.push_back(makeSchedule(dv, baseSchedule->second, openEndDateReplacement, unadjusted));
    }
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
        BusinessDayConvention convention = Null<BusinessDayConvention>(),
                              termConvention = Unadjusted; // initialization prevents gcc warning
        Calendar calendar;
        Period tenor;
        DateGeneration::Rule rule = DateGeneration::Zero; // initialization prevents gcc warning
        bool endOfMonth = false;                          // initialization prevents gcc warning
        BusinessDayConvention endOfMonthConvention = Null<BusinessDayConvention>();
        bool hasCalendar = false, hasConvention = false, hasTermConvention = false, hasTenor = false, hasRule = false,
             hasEndOfMonth = false, hasEndOfMonthConvention = false, hasConsistentCalendar = true,
             hasConsistentConvention = true, hasConsistentTenor = true, hasConsistentRule = true,
             hasConsistentEndOfMonth = true, hasConsistentEndOfMonthConvention = true;
        for (auto& d : data.dates()) {
            updateData<Calendar>(d.calendar(), calendar, hasCalendar, hasConsistentCalendar, parseCalendarTemp);
            updateData<BusinessDayConvention>(d.convention(), convention, hasConvention, hasConsistentConvention,
                                              parseBusinessDayConvention);
            updateData<Period>(d.tenor(), tenor, hasTenor, hasConsistentTenor, parsePeriod);
        }
        for (auto& d : data.rules()) {
            updateData<Calendar>(d.calendar(), calendar, hasCalendar, hasConsistentCalendar, parseCalendarTemp);
            updateData<BusinessDayConvention>(d.convention(), convention, hasConvention, hasConsistentConvention,
                                              parseBusinessDayConvention);
            updateData<Period>(d.tenor(), tenor, hasTenor, hasConsistentTenor, parsePeriod);
            updateData<bool>(d.endOfMonth(), endOfMonth, hasEndOfMonth, hasConsistentEndOfMonth, parseBool);
            updateData<BusinessDayConvention>(d.endOfMonthConvention(), endOfMonthConvention, hasEndOfMonthConvention,
                                              hasConsistentEndOfMonthConvention, parseBusinessDayConvention);
            updateData<DateGeneration::Rule>(d.rule(), rule, hasRule, hasConsistentRule, parseDateGenerationRule);
            if (d.termConvention() != "") {
                hasTermConvention = true;
                termConvention = parseBusinessDayConvention(d.termConvention());
            }
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
            // add isRegular information, if available, otherwise assume irregular periods
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
        return QuantLib::Schedule(
            dates, hasCalendar && hasConsistentCalendar ? calendar : NullCalendar(),
            hasConvention && hasConsistentConvention ? convention : Unadjusted,
            hasTermConvention ? ext::optional<BusinessDayConvention>(termConvention) : QuantLib::ext::nullopt,
            hasTenor && hasConsistentTenor ? ext::optional<Period>(tenor) : QuantLib::ext::nullopt,
            hasRule && hasConsistentRule ? ext::optional<DateGeneration::Rule>(rule) : QuantLib::ext::nullopt,
            hasEndOfMonth && hasConsistentEndOfMonth ? ext::optional<bool>(endOfMonth) : QuantLib::ext::nullopt, isRegular,
            false, false,
            hasEndOfMonthConvention && hasConsistentEndOfMonthConvention
                ? ext::optional<BusinessDayConvention>(endOfMonthConvention)
                : QuantLib::ext::nullopt);
    }
}
} // namespace data
} // namespace ore
