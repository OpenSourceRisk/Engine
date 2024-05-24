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
// #include <ored/utilities/parsers.hpp>

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

    ScheduleRules(const string& startDate, const string& endDate, const string& tenor, const string& calendar,
                  const string& convention, const string& termConvention, const string& rule,
                  const string& endOfMonth = "N", const string& firstDate = "", const string& lastDate = "",
                  const bool removeFirstDate = false, const bool removeLastDate = false,
                  const string& endOfMonthConvention = "")
        : startDate_(startDate), endDate_(endDate), tenor_(tenor), calendar_(calendar), convention_(convention),
          termConvention_(termConvention), rule_(rule), endOfMonth_(endOfMonth),
          endOfMonthConvention_(endOfMonthConvention), firstDate_(firstDate), lastDate_(lastDate),
          removeFirstDate_(removeFirstDate), removeLastDate_(removeLastDate) {}

    //! Check if key attributes are empty
    const bool hasData() const { return !startDate_.empty() && !tenor_.empty(); }

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
    const string& endOfMonthConvention() const { return endOfMonthConvention_; }
    const string& firstDate() const { return firstDate_; }
    const string& lastDate() const { return lastDate_; }
    bool removeFirstDate() const { return removeFirstDate_; }
    bool removeLastDate() const { return removeLastDate_; }
    //@}

    //! \name Modifiers
    //@{
    string& modifyStartDate() { return startDate_; }
    string& modifyEndDate() { return endDate_; }
    string& modifyCalendar() { return calendar_; }
    string& modifyConvention() { return convention_; }
    string& modifyTermConvention() { return termConvention_; }
    string& modifyEndOfMonthConvention() { return endOfMonthConvention_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
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
    string endOfMonthConvention_;
    string firstDate_;
    string lastDate_;
    bool adjustEndDateToPreviousMonthEnd_{false};
    bool removeFirstDate_ = false;
    bool removeLastDate_ = false;
    bool was1T_ = false;
};

//! Serializable object holding schedule Dates data
/*!
  \ingroup tradedata
*/
class ScheduleDates : public XMLSerializable {
public:
    //! Default constructor
    ScheduleDates() {}
    //! Constructor
    ScheduleDates(const string& calendar, const string& convention, const string& tenor, const vector<string>& dates,
                  const string& endOfMonth = "", const string& endOfMonthConvention = "")
        : calendar_(calendar), convention_(convention), tenor_(tenor), endOfMonth_(endOfMonth),
          endOfMonthConvention_(endOfMonthConvention), dates_(dates) {}

    //! Check if key attributes are empty
    bool hasData() const { return dates_.size() > 0 && !tenor_.empty(); }

    //! \name Inspectors
    //@{
    const string& calendar() const { return calendar_; }
    const string& convention() const { return convention_; }
    const string& tenor() const { return tenor_; }
    const string& endOfMonth() const { return endOfMonth_; }
    const string& endOfMonthConvention() const { return endOfMonthConvention_; }
    const vector<string>& dates() const { return dates_; }
    //@}

    //! \name Modifiers
    //@{
    vector<string>& modifyDates() { return dates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    string calendar_;
    string convention_;
    string tenor_;
    string endOfMonth_;
    string endOfMonthConvention_;
    vector<string> dates_;
    bool was1T_ = false;
};

//! Serializable object holding Derived schedule data
/*!
  \ingroup tradedata
*/
class ScheduleDerived : public XMLSerializable {
public:
    //! Default constructor
    ScheduleDerived() {}
    //! Constructor
    ScheduleDerived(const string& baseSchedule, const string& calendar, const string& convention, const string& shift,
                    const bool removeFirstDate = false, const bool removeLastDate = false)
        : baseSchedule_(baseSchedule), calendar_(calendar), convention_(convention), shift_(shift),
          removeFirstDate_(removeFirstDate), removeLastDate_(removeLastDate) {}

    //! \name Inspectors
    //@{
    const string& baseSchedule() const { return baseSchedule_; }
    const string& calendar() const { return calendar_; }
    const string& convention() const { return convention_; }
    const string& shift() const { return shift_; }
    bool removeFirstDate() const { return removeFirstDate_; }
    bool removeLastDate() const { return removeLastDate_; }
    //@}

    //! \name Modifiers
    //@{
    string& modifyCalendar() { return calendar_; }
    string& modifyConvention() { return convention_; }
    string& modifyShift() { return shift_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    string baseSchedule_;
    string calendar_;
    string convention_;
    string shift_;
    bool removeFirstDate_ = false;
    bool removeLastDate_ = false;
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
    ScheduleData(const ScheduleDates& dates, const string& name = "") : name_(name) { addDates(dates); }
    //! Constructor with ScheduleRules
    ScheduleData(const ScheduleRules& rules, const string& name = "") : name_(name) { addRules(rules); }
    //! Constructor with ScheduleDerived
    ScheduleData(const ScheduleDerived& derived, const string& name = "") : name_(name) { addDerived(derived); }

    //! Add dates
    void addDates(const ScheduleDates& dates) { dates_.emplace_back(dates); }
    //! Add rules
    void addRules(const ScheduleRules& rules) { rules_.emplace_back(rules); }
    //! Add derived schedules
    void addDerived(const ScheduleDerived& derived) {
        derived_.emplace_back(derived);
        hasDerived_ = true;
    }
    //! Check if has any dates/rules/derived schedules
    bool hasData() const { return dates_.size() > 0 || rules_.size() > 0 || derived_.size() > 0; }
    vector<string> baseScheduleNames();

    //! \name Inspectors
    //@{
    const vector<ScheduleDates>& dates() const { return dates_; }
    const vector<ScheduleRules>& rules() const { return rules_; }
    const vector<ScheduleDerived>& derived() const { return derived_; }
    const string& name() const { return name_; }
    const bool& hasDerived() const { return hasDerived_; }
    //@}

    //! \name Modifiers
    //@{
    vector<ScheduleDates>& modifyDates() { return dates_; }
    vector<ScheduleRules>& modifyRules() { return rules_; }
    vector<ScheduleDerived>& modifyDerived() { return derived_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}
private:
    vector<ScheduleDates> dates_;
    vector<ScheduleRules> rules_;
    vector<ScheduleDerived> derived_;
    string name_ = "";
    bool hasDerived_ = false;
};

// Container class to support building of derived schedules
class ScheduleBuilder {
// USAGE:
//  1. Initialise a ScheduleBuilder obj
//  2. For each Schedule obj that will be built from a given ScheduleData obj:
//      i. Initialise an empty Schedule obj
//     ii. Add the Schedule obj in i. and the corresponding ScheduleData obj into the ScheduleBuilder obj in 1.
//         using the ScheduleBuilder::add() method.
//  3. Once all required schedules are added for the instrument/leg, call ScheduleBuilder::makeSchedules()
//     with the appropriate openEndDateReplacement, if relevant.
//     If successful, each Schedule obj, derived or otherwise, should have been assigned the correct schedule.
// NOTE / WARNING:
//  * The schedules should be initialised at the same, or higher, scope as the call to makeSchedules() (and to add())
      
public:
    void add(QuantLib::Schedule& schedule, const ScheduleData& scheduleData);
    void makeSchedules(const QuantLib::Date& openEndDateReplacement = QuantLib::Null<QuantLib::Date>());

private:
    map<string, pair<ScheduleData, QuantLib::Schedule&>> schedules_;
};

//! Functions
QuantLib::Schedule makeSchedule(const ScheduleData& data,
                                const QuantLib::Date& openEndDateReplacement = QuantLib::Null<QuantLib::Date>(),
                                const map<string, QuantLib::Schedule>& baseSchedules = map<string, QuantLib::Schedule>());
QuantLib::Schedule makeSchedule(const ScheduleDates& dates);
QuantLib::Schedule makeSchedule(const ScheduleRules& rules,
                                const QuantLib::Date& openEndDateReplacement = QuantLib::Null<QuantLib::Date>());
QuantLib::Schedule makeSchedule(const ScheduleDerived& derived, const QuantLib::Schedule& baseSchedule);

} // namespace data
} // namespace ore
