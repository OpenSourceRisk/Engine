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

/*! \file portfolio/schedule.hpp
    \brief trade schedule data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/time/schedule.hpp>

namespace ore {
namespace data {

//! Serializable object holding schedule Rules data
/*!
  \ingroup tradedata
*/
class ScheduleRules : public XMLSerializable {
public:
    //! Default constructor
    ScheduleRules() {}
    //! Constructor
    ScheduleRules(const string& startDate, const string& endDate, const string& tenor, const string& calendar,
                  const string& convention, const string& termConvention, const string& rule,
                  const string& endOfMonth = "N", const string& firstDate = "", const string& lastDate = "")
        : startDate_(startDate), endDate_(endDate), tenor_(tenor), calendar_(calendar), convention_(convention),
          termConvention_(termConvention), rule_(rule), endOfMonth_(endOfMonth), firstDate_(firstDate),
          lastDate_(lastDate) {}

    //! \name Inspectors
    //@{
    const string& startDate() const { return startDate_; }
    // might be empty, indicating a perpetual schedule
    const string& endDate() const { return endDate_; }
    const string& tenor() const { return tenor_; }
    const string& calendar() const { return calendar_; }
    const string& convention() const { return convention_; }
    const string& termConvention() const { return termConvention_; }
    const string& rule() const { return rule_; }
    const string& endOfMonth() const { return endOfMonth_; }
    const string& firstDate() const { return firstDate_; }
    const string& lastDate() const { return lastDate_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    string startDate_;
    string endDate_;
    string tenor_;
    string calendar_;
    string convention_;
    string termConvention_;
    string rule_;
    string endOfMonth_;
    string firstDate_;
    string lastDate_;
};

//! Serializable object holding schedule dates data
/*!
  \ingroup tradedata
*/
class ScheduleDates : public XMLSerializable {
public:
    //! Default constructor
    ScheduleDates() {}
    //! Constructor
    ScheduleDates(const string& calendar, const string& convention, const string& tenor, const vector<string>& dates,
                  const string& endOfMonth = "")
        : calendar_(calendar), convention_(convention), tenor_(tenor), endOfMonth_(endOfMonth), dates_(dates) {}

    //! \name Inspectors
    //@{
    const string& calendar() const { return calendar_; }
    const string& convention() const { return convention_; }
    const string& tenor() const { return tenor_; }
    const string& endOfMonth() const { return endOfMonth_; }
    const vector<string>& dates() const { return dates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    string calendar_;
    string convention_;
    string tenor_;
    string endOfMonth_;
    vector<string> dates_;
};

//! Serializable schedule data
/*!
  \ingroup tradedata
*/
class ScheduleData : public XMLSerializable {
public:
    //! Default constructor
    ScheduleData() {}
    //! Constructor with ScheduleDates
    ScheduleData(const ScheduleDates& dates) { addDates(dates); }
    //! Constructor with ScheduleRules
    ScheduleData(const ScheduleRules& rules) { addRules(rules); }

    //! Add dates
    void addDates(const ScheduleDates& dates) { dates_.emplace_back(dates); }
    //! Add rules
    void addRules(const ScheduleRules& rules) { rules_.emplace_back(rules); }
    //! Check if has any dates/rules
    bool hasData() const { return dates_.size() > 0 || rules_.size() > 0; }

    //! \name Inspectors
    //@{
    const vector<ScheduleDates>& dates() const { return dates_; }
    const vector<ScheduleRules>& rules() const { return rules_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    vector<ScheduleDates> dates_;
    vector<ScheduleRules> rules_;
};

//! Functions
QuantLib::Schedule makeSchedule(const ScheduleData& data,
                                const QuantLib::Date& openEndDateReplacement = QuantLib::Null<QuantLib::Date>());
QuantLib::Schedule makeSchedule(const ScheduleDates& dates);
QuantLib::Schedule makeSchedule(const ScheduleRules& rules,
                                const QuantLib::Date& openEndDateReplacement = QuantLib::Null<QuantLib::Date>());
} // namespace data
} // namespace ore
