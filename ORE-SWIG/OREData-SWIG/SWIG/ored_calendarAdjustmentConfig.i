/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#ifndef ored_CalendarAdjustmentConfig_i
#define ored_CalendarAdjustmentConfig_i

%{

using ore::data::CalendarAdjustmentConfig;
using std::map;
using std::set;
using std::string;
using std::vector;
using QuantLib::DateGeneration;
using QuantLib::Calendar;
%}

%shared_ptr(CalendarAdjustmentConfig)
class CalendarAdjustmentConfig {
  public:

    CalendarAdjustmentConfig();

    void addHolidays(const string& calname, const Date& d);

    void addBusinessDays(const string& calname, const Date& d);

    void addBaseCalendar(const string& calname, const string& d);

    const set<Date>& getHolidays(const string& calname);

    const set<Date>& getBusinessDays(const string& calname);

    set<string> getCalendars() const;

    const string& getBaseCalendar(const string& calname);

    void append(const CalendarAdjustmentConfig& c);

    void fromFile(const std::string& name);

};


#endif