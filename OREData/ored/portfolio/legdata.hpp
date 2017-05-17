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

/*! \file portfolio/legdata.hpp
    \brief leg data model and serialization
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/cashflow.hpp>
#include <ql/indexes/iborindex.hpp>

#include <vector>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {
//! Serializable Cashflow Leg Data
/*!
  \ingroup tradedata
*/

class CashflowData : public XMLSerializable {
public:
    //! Default constructor
    CashflowData() {}
    //! Constructor
    CashflowData(const vector<double>& amounts, const vector<string>& dates) : amounts_(amounts), dates_(dates) {}

    //! \name Inspectors
    //@{
    const vector<double>& amounts() const { return amounts_; }
    const vector<string>& dates() const { return dates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    vector<double> amounts_;
    vector<string> dates_;
};

//! Serializable Fixed Leg Data
/*!
  \ingroup tradedata
*/
class FixedLegData : public XMLSerializable {
public:
    //! Default constructor
    FixedLegData() {}
    //! Constructor
    FixedLegData(const vector<double>& rates, const vector<string>& rateDates = vector<string>())
        : rates_(rates), rateDates_(rateDates) {}

    //! \name Inspectors
    //@{
    const vector<double>& rates() const { return rates_; }
    const vector<string>& rateDates() const { return rateDates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    vector<double> rates_;
    vector<string> rateDates_;
};

//! Serializable Floating Leg Data
/*!
  \ingroup tradedata
*/
class FloatingLegData : public XMLSerializable {
public:
    //! Default constructor
    FloatingLegData() : fixingDays_(0), isInArrears_(true) {}
    //! Constructor
    FloatingLegData(const string& index, int fixingDays, bool isInArrears, const vector<double>& spreads,
                    const vector<string>& spreadDates = vector<string>(), const vector<double>& caps = vector<double>(),
                    const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
                    const vector<string>& floorDates = vector<string>(),
                    const vector<double>& gearings = vector<double>(),
                    const vector<string>& gearingDates = vector<string>())
        : index_(index), fixingDays_(fixingDays), isInArrears_(isInArrears), spreads_(spreads),
          spreadDates_(spreadDates), caps_(caps), capDates_(capDates), floors_(floors), floorDates_(floorDates),
          gearings_(gearings), gearingDates_(gearingDates) {}

    //! \name Inspectors
    //@{
    const string& index() const { return index_; }
    int fixingDays() const { return fixingDays_; }
    bool isInArrears() const { return isInArrears_; }
    const vector<double>& spreads() const { return spreads_; }
    const vector<string>& spreadDates() const { return spreadDates_; }
    const vector<double>& caps() const { return caps_; }
    const vector<string>& capDates() const { return capDates_; }
    const vector<double>& floors() const { return floors_; }
    const vector<string>& floorDates() const { return floorDates_; }
    const vector<double>& gearings() const { return gearings_; }
    const vector<string>& gearingDates() const { return gearingDates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    string index_;
    int fixingDays_;
    bool isInArrears_;
    vector<double> spreads_;
    vector<string> spreadDates_;
    vector<double> caps_;
    vector<string> capDates_;
    vector<double> floors_;
    vector<string> floorDates_;
    vector<double> gearings_;
    vector<string> gearingDates_;
};

//! Serializable object holding leg data
class LegData : public XMLSerializable {
public:
    //! Default constructor
    LegData()
        : isPayer_(true), notionalInitialExchange_(false), notionalFinalExchange_(false),
          notionalAmortizingExchange_(false), isNotResetXCCY_(true), foreignAmount_(0.0), fixingDays_(0) {}

    //! Constructor with CashflowData
    LegData(bool isPayer, const string& currency, CashflowData& data,
            const vector<string>& notionalDates = vector<string>(), const string& paymentConvention = "F",
            bool notionalInitialExchange = false, bool notionalFinalExchange = false,
            bool notionalAmortizingExchange = false)
        : isPayer_(isPayer), currency_(currency), legType_("Cashflow"), cashflowData_(data),
          notionalDates_(notionalDates), paymentConvention_(paymentConvention),
          notionalInitialExchange_(notionalInitialExchange), notionalFinalExchange_(notionalFinalExchange),
          notionalAmortizingExchange_(notionalAmortizingExchange) {}

    //! Constructor with FloatingLegData
    LegData(bool isPayer, const string& currency, FloatingLegData& data, ScheduleData& schedule,
            const string& dayCounter, const vector<double>& notionals,
            const vector<string>& notionalDates = vector<string>(), const string& paymentConvention = "F",
            bool notionalInitialExchange = false, bool notionalFinalExchange = false,
            bool notionalAmortizingExchange = false, bool isNotResetXCCY = true, const string& foreignCurrency = "",
            double foreignAmount = 0, const string& fxIndex = "", int fixingDays = 0)
        : isPayer_(isPayer), currency_(currency), legType_("Floating"), floatingLegData_(data), schedule_(schedule),
          dayCounter_(dayCounter), notionals_(notionals), notionalDates_(notionalDates),
          paymentConvention_(paymentConvention), notionalInitialExchange_(notionalInitialExchange),
          notionalFinalExchange_(notionalFinalExchange), notionalAmortizingExchange_(notionalAmortizingExchange),
          isNotResetXCCY_(isNotResetXCCY), foreignCurrency_(foreignCurrency), foreignAmount_(foreignAmount),
          fxIndex_(fxIndex), fixingDays_(fixingDays) {}

    //! Constructor with FixedLegData
    LegData(bool isPayer, const string& currency, FixedLegData& data, ScheduleData& schedule, const string& dayCounter,
            const vector<double>& notionals, const vector<string>& notionalDates = vector<string>(),
            const string& paymentConvention = "F", bool notionalInitialExchange = false,
            bool notionalFinalExchange = false, bool notionalAmortizingExchange = false, bool isNotResetXCCY = true,
            const string& foreignCurrency = "", double foreignAmount = 0, const string& fxIndex = "",
            int fixingDays = 0)
        : isPayer_(isPayer), currency_(currency), legType_("Fixed"), fixedLegData_(data), schedule_(schedule),
          dayCounter_(dayCounter), notionals_(notionals), notionalDates_(notionalDates),
          paymentConvention_(paymentConvention), notionalInitialExchange_(notionalInitialExchange),
          notionalFinalExchange_(notionalFinalExchange), notionalAmortizingExchange_(notionalAmortizingExchange),
          isNotResetXCCY_(isNotResetXCCY), foreignCurrency_(foreignCurrency), foreignAmount_(foreignAmount),
          fxIndex_(fxIndex), fixingDays_(fixingDays) {}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Inspectors
    //@{
    bool isPayer() const { return isPayer_; }
    const string& currency() const { return currency_; }
    const ScheduleData& schedule() const { return schedule_; }
    const vector<double>& notionals() const { return notionals_; }
    const vector<string>& notionalDates() const { return notionalDates_; }
    const string& dayCounter() const { return dayCounter_; }
    const string& paymentConvention() const { return paymentConvention_; }
    bool notionalInitialExchange() const { return notionalInitialExchange_; }
    bool notionalFinalExchange() const { return notionalFinalExchange_; }
    bool notionalAmortizingExchange() const { return notionalAmortizingExchange_; }
    const string& legType() const { return legType_; }
    const CashflowData& cashflowData() const { return cashflowData_; }
    const FloatingLegData& floatingLegData() const { return floatingLegData_; }
    const FixedLegData& fixedLegData() const { return fixedLegData_; }
    bool isNotResetXCCY() const { return isNotResetXCCY_; }
    const string& foreignCurrency() const { return foreignCurrency_; }
    double foreignAmount() const { return foreignAmount_; }
    const string& fxIndex() const { return fxIndex_; }
    int fixingDays() const { return fixingDays_; }
    //@}
private:
    bool isPayer_;
    string currency_;
    string legType_;
    CashflowData cashflowData_;
    FloatingLegData floatingLegData_;
    FixedLegData fixedLegData_;
    ScheduleData schedule_;
    string dayCounter_;
    vector<double> notionals_;
    vector<string> notionalDates_;
    string paymentConvention_;
    bool notionalInitialExchange_;
    bool notionalFinalExchange_;
    bool notionalAmortizingExchange_;
    bool isNotResetXCCY_;
    string foreignCurrency_;
    double foreignAmount_;
    string fxIndex_;
    int fixingDays_;
};

//! \name Utilities for building QuantLib Legs
//@{
Leg makeFixedLeg(LegData& data);
Leg makeIborLeg(LegData& data, boost::shared_ptr<IborIndex> index,
                const boost::shared_ptr<EngineFactory>& engineFactory);
Leg makeOISLeg(LegData& data, boost::shared_ptr<OvernightIndex> index);
Leg makeSimpleLeg(LegData& data);
Leg makeNotionalLeg(const Leg& refLeg, bool initNomFlow, bool finalNomFlow, bool amortNomFlow = true);

//@}

//! Build a full vector of values from the given node.
//  For use with Notionals, Rates, Spreads, Gearing, Caps and Floor rates.
//  In all cases we can expand the vector to take the given schedule into account
vector<double> buildScheduledVector(const vector<double>& values, const vector<string>& dates,
                                    const Schedule& schedule);
} // namespace data
} // namespace ore
