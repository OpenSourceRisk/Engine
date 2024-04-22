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

#include <ored/utilities/calendaradjustmentconfig.hpp>
#include <ored/utilities/calendarparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/time/calendar.hpp>
#include <string>
namespace ore {
namespace data {

CalendarAdjustmentConfig::CalendarAdjustmentConfig() {}

void CalendarAdjustmentConfig::addHolidays(const string& calname, const Date& d) {
    additionalHolidays_[normalisedName(calname)].insert(d);
}

void CalendarAdjustmentConfig::addBusinessDays(const string& calname, const Date& d) {
    additionalBusinessDays_[normalisedName(calname)].insert(d);
}

void CalendarAdjustmentConfig::addBaseCalendar(const string& calname, const string& s) {
    baseCalendars_[normalisedName(calname)] = s;
}

const set<Date>& CalendarAdjustmentConfig::getHolidays(const string& calname) const {
    auto holidays = additionalHolidays_.find(normalisedName(calname));
    if (holidays != additionalHolidays_.end()) {
        return holidays->second;
    } else {
        static set<Date> empty;
        return empty;
    }
}

const set<Date>& CalendarAdjustmentConfig::getBusinessDays(const string& calname) const {
    auto businessDays = additionalBusinessDays_.find(normalisedName(calname));
    if (businessDays != additionalBusinessDays_.end()) {
        return businessDays->second;
    } else {
        static set<Date> empty;
        return empty;
    }
}

set<string> CalendarAdjustmentConfig::getCalendars() const {
    set<string> cals;
    for (auto it : additionalHolidays_) {
        cals.insert(it.first);
    }
    for (auto it : additionalBusinessDays_) {
        cals.insert(it.first);
    }
    return cals;
}


const string& CalendarAdjustmentConfig::getBaseCalendar(const string& calname) const {
    auto bc = baseCalendars_.find(normalisedName(calname));

    if (bc != baseCalendars_.end()) {
        return bc->second;
    } else {
        static const string empty = "";
        return empty;
    }
}
string CalendarAdjustmentConfig::normalisedName(const string& c) const { return parseCalendar(c).name(); }

void CalendarAdjustmentConfig::append(const CalendarAdjustmentConfig& c) {
    for (auto it : c.getCalendars()) {
        for (auto h : c.getHolidays(it)) {
            addHolidays(it, h);
        }
        for (auto b : c.getBusinessDays(it)) {
            addBusinessDays(it, b);
        }
    }
};

void CalendarAdjustmentConfig::fromXML(XMLNode* node) {
    XMLUtils::checkNode(node, "CalendarAdjustments");

    //we loop once skipping any new calendars
    for (auto calnode : XMLUtils::getChildrenNodes(node, "Calendar")) {
        string calname = XMLUtils::getAttribute(calnode, "name");
        string baseCalendar = XMLUtils::getChildValue(calnode, "BaseCalendar", false);
        if (baseCalendar != "") {
            // we check here if the baseCalendar is in the map before the new calendars are added
            // this is because we don't want any new calendars to be defined in terms of other new calendars
            parseCalendar(baseCalendar);
            continue;
        }
        Calendar cal = parseCalendar(calname);

        vector<string> holidayDates = XMLUtils::getChildrenValues(calnode, "AdditionalHolidays", "Date");
        for (auto holiday : holidayDates) {
            try {
                Date h = parseDate(holiday);
                addHolidays(calname, h);
                cal.addHoliday(h);
            } catch(std::exception&) {
                ALOG("error parsing holiday " << holiday << " for calendar " << calname);
            }
        }
        vector<string> businessDates = XMLUtils::getChildrenValues(calnode, "AdditionalBusinessDays", "Date");
        for (auto businessDay : businessDates) {
            try {
                Date b = parseDate(businessDay);
                addBusinessDays(calname, b);
                cal.removeHoliday(b);
            } catch(std::exception&) {
                ALOG("error parsing business day " << businessDay << " for calendar " << calname);
            }
        }
    }
    //then loop again adding the new calendars
    for (auto calnode : XMLUtils::getChildrenNodes(node, "Calendar")) {
        string calname = XMLUtils::getAttribute(calnode, "name");
        string baseCalendar = XMLUtils::getChildValue(calnode, "BaseCalendar", false);
        if (baseCalendar == "")
            continue;
        Calendar cal = CalendarParser::instance().addCalendar(baseCalendar, calname);

        vector<string> holidayDates = XMLUtils::getChildrenValues(calnode, "AdditionalHolidays", "Date");
        for (auto holiday : holidayDates) {
            try {
                Date h = parseDate(holiday);
                addHolidays(calname, h);
                cal.addHoliday(h);
            } catch(std::exception&) {
                ALOG("error parsing business day " << holiday << " for calendar " << calname);
            }

        }
        vector<string> businessDates = XMLUtils::getChildrenValues(calnode, "AdditionalBusinessDays", "Date");
        for (auto businessDay : businessDates) {
            try {
                Date b = parseDate(businessDay);
                addBusinessDays(calname, b);
                cal.removeHoliday(b);
            } catch(std::exception&) {
                ALOG("error parsing business day " << businessDay << " for calendar " << calname);
            }
        }

        addBaseCalendar(calname, baseCalendar);
    }

}

XMLNode* CalendarAdjustmentConfig::toXML(XMLDocument& doc) const {
    XMLNode* node = doc.allocNode("CalendarAdjustments");
    for (auto cal : getCalendars()) {
        XMLNode* calendarNode = XMLUtils::addChild(doc, node, "Calendar");
        XMLUtils::addAttribute(doc, calendarNode, "name", cal);
        if (getBaseCalendar(cal) != "")
            XMLUtils::addChild(doc, calendarNode, "BaseCalendar", getBaseCalendar(cal));
        XMLNode* ahd = XMLUtils::addChild(doc, calendarNode, "AdditionalHolidays");
        for (auto hol : getHolidays(cal)) {
            XMLUtils::addChild(doc, ahd, "Date", to_string(hol));
        }
        XMLNode* abd = XMLUtils::addChild(doc, calendarNode, "AdditionalBusinessDays");
        for (auto bd : getBusinessDays(cal)) {
            XMLUtils::addChild(doc, abd, "Date", to_string(bd));
        }
    }
    return node;
}

} // namespace data
} // namespace ore
