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

/*! \file ored/utilities/calendaradjustmentconfig.hpp
    \brief Interface for calendar modifications, additional holidays and business days
    \ingroup utilities
*/

#pragma once

#include <map>
#include <ored/utilities/xmlutils.hpp>
#include <ql/patterns/singleton.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/utilities/dataparsers.hpp>
#include <set>
#include <string>
#include <vector>

namespace ore {
namespace data {
using QuantLib::Calendar;
using QuantLib::Date;
using std::map;
using std::set;
using std::string;
using std::vector;

class CalendarAdjustmentConfig : public XMLSerializable {
public:
    // default constructor
    CalendarAdjustmentConfig();
    //! This method adds d to the list of holidays for cal name.
    void addHolidays(const string& calname, const Date& d);

    //! This method adds d to the list of business days for cal name.
    void addBusinessDays(const string& calname, const Date& d);

    //! This method adds s as a base calendar for cal name.
    void addBaseCalendar(const string& calname, const string& d);

    //! Returns all the holidays for a given cal name
    const set<Date>& getHolidays(const string& calname) const;

    //! Returns all the business days for a given calname
    const set<Date>& getBusinessDays(const string& calname) const;

    set<string> getCalendars() const;

    const string& getBaseCalendar(const string& calname) const;

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    //! add all holidays and business days from c to this instance
    void append(const CalendarAdjustmentConfig& c);

private:
    map<string, string> baseCalendars_;
    map<string, set<Date>> additionalHolidays_;
    map<string, set<Date>> additionalBusinessDays_;

    string normalisedName(const string&) const;
};

} // namespace data
} // namespace ore
