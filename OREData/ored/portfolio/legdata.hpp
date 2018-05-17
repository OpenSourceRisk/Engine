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
    \ingroup tradedata
*/

#pragma once

#include <boost/make_shared.hpp>
#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/cashflow.hpp>
#include <ql/experimental/coupons/swapspreadindex.hpp>
#include <ql/indexes/iborindex.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>

#include <vector>

using namespace QuantLib;
using std::string;

namespace ore {
namespace data {

//! Serializable Additional Leg Data
/*!
\ingroup tradedata
*/

// Really bad name....
class LegAdditionalData : public XMLSerializable {
public:
    LegAdditionalData(const string& legType, const string& legNodeName)
        : legType_(legType), legNodeName_(legNodeName) {}
    LegAdditionalData(const string& legType) : legType_(legType), legNodeName_(legType + "LegData") {}

    const string& legType() const { return legType_; }
    const string& legNodeName() const { return legNodeName_; }

private:
    string legType_;
    string legNodeName_; // the XML node name
};

//! Serializable Cashflow Leg Data
/*!
  \ingroup tradedata
*/

class CashflowData : public LegAdditionalData {
public:
    //! Default constructor
    CashflowData() : LegAdditionalData("Cashflow", "CashflowData") {}
    //! Constructor
    CashflowData(const vector<double>& amounts, const vector<string>& dates)
        : LegAdditionalData("Cashflow", "CashflowData"), amounts_(amounts), dates_(dates) {}

    //! \name Inspectors
    //@{
    const vector<double>& amounts() const { return amounts_; }
    const vector<string>& dates() const { return dates_; }
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    vector<double> amounts_;
    vector<string> dates_;
};

//! Serializable Fixed Leg Data
/*!
  \ingroup tradedata
*/
class FixedLegData : public LegAdditionalData {
public:
    //! Default constructor
    FixedLegData() : LegAdditionalData("Fixed") {}
    //! Constructor
    FixedLegData(const vector<double>& rates, const vector<string>& rateDates = vector<string>())
        : LegAdditionalData("Fixed"), rates_(rates), rateDates_(rateDates) {}

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
class FloatingLegData : public LegAdditionalData {
public:
    //! Default constructor
    FloatingLegData() : LegAdditionalData("Floating"), fixingDays_(0), isInArrears_(true), nakedOption_(false) {}
    //! Constructor
    FloatingLegData(const string& index, QuantLib::Natural fixingDays, bool isInArrears, const vector<double>& spreads,
                    const vector<string>& spreadDates = vector<string>(), const vector<double>& caps = vector<double>(),
                    const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
                    const vector<string>& floorDates = vector<string>(),
                    const vector<double>& gearings = vector<double>(),
                    const vector<string>& gearingDates = vector<string>(), bool isAveraged = false,
                    bool nakedOption = false)
        : LegAdditionalData("Floating"), index_(index), fixingDays_(fixingDays), isInArrears_(isInArrears),
          isAveraged_(isAveraged), spreads_(spreads), spreadDates_(spreadDates), caps_(caps), capDates_(capDates),
          floors_(floors), floorDates_(floorDates), gearings_(gearings), gearingDates_(gearingDates),
          nakedOption_(nakedOption) {}

    //! \name Inspectors
    //@{
    const string& index() const { return index_; }
    QuantLib::Natural fixingDays() const { return fixingDays_; }
    bool isInArrears() const { return isInArrears_; }
    bool isAveraged() const { return isAveraged_; }
    const vector<double>& spreads() const { return spreads_; }
    const vector<string>& spreadDates() const { return spreadDates_; }
    const vector<double>& caps() const { return caps_; }
    const vector<string>& capDates() const { return capDates_; }
    const vector<double>& floors() const { return floors_; }
    const vector<string>& floorDates() const { return floorDates_; }
    const vector<double>& gearings() const { return gearings_; }
    const vector<string>& gearingDates() const { return gearingDates_; }
    bool nakedOption() const { return nakedOption_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    string index_;
    QuantLib::Natural fixingDays_;
    bool isInArrears_;
    bool isAveraged_;
    vector<double> spreads_;
    vector<string> spreadDates_;
    vector<double> caps_;
    vector<string> capDates_;
    vector<double> floors_;
    vector<string> floorDates_;
    vector<double> gearings_;
    vector<string> gearingDates_;
    bool nakedOption_;
};

//! Serializable CPI Leg Data
/*!
\ingroup tradedata
*/

class CPILegData : public LegAdditionalData {
public:
    //! Default constructor
    CPILegData() : LegAdditionalData("CPI") {}
    //! Constructor
    CPILegData(string index, double baseCPI, string observationLag, bool interpolated, const vector<double>& rates,
               const vector<string>& rateDates = std::vector<string>(), bool subtractInflationNominal = true)
        : LegAdditionalData("CPI"), index_(index), baseCPI_(baseCPI), observationLag_(observationLag),
          interpolated_(interpolated), rates_(rates), rateDates_(rateDates),
          subtractInflationNominal_(subtractInflationNominal) {}

    //! \name Inspectors
    //@{
    const string index() const { return index_; }
    double baseCPI() const { return baseCPI_; }
    const string observationLag() const { return observationLag_; }
    bool interpolated() const { return interpolated_; }
    const std::vector<double>& rates() const { return rates_; }
    const std::vector<string>& rateDates() const { return rateDates_; }
    bool subtractInflationNominal() const { return subtractInflationNominal_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    string index_;
    double baseCPI_;
    string observationLag_;
    bool interpolated_;
    vector<double> rates_;
    vector<string> rateDates_;
    bool subtractInflationNominal_;
};

//! Serializable YoY Leg Data
/*!
\ingroup tradedata
*/
class YoYLegData : public LegAdditionalData {
public:
    //! Default constructor
    YoYLegData() : LegAdditionalData("YY") {}
    //! Constructor
    YoYLegData(string index, string observationLag, bool interpolated, Size fixingDays,
               const vector<double>& gearings = std::vector<double>(),
               const vector<string>& gearingDates = std::vector<string>(),
               const vector<double>& spreads = std::vector<double>(),
               const vector<string>& spreadDates = std::vector<string>())
        : LegAdditionalData("YY"), index_(index), observationLag_(observationLag), interpolated_(interpolated),
          fixingDays_(fixingDays), gearings_(gearings), gearingDates_(gearingDates), spreads_(spreads),
          spreadDates_(spreadDates) {}

    //! \name Inspectors
    //@{
    const string index() const { return index_; }
    const string observationLag() const { return observationLag_; }
    bool interpolated() const { return interpolated_; }
    Size fixingDays() const { return fixingDays_; }
    const std::vector<double>& gearings() const { return gearings_; }
    const std::vector<string>& gearingDates() const { return gearingDates_; }
    const std::vector<double>& spreads() const { return spreads_; }
    const std::vector<string>& spreadDates() const { return spreadDates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

private:
    string index_;
    string observationLag_;
    bool interpolated_;
    Size fixingDays_;
    vector<double> gearings_;
    vector<string> gearingDates_;
    vector<double> spreads_;
    vector<string> spreadDates_;
};

//! Serializable CMS Leg Data
/*!
\ingroup tradedata
*/
class CMSLegData : public LegAdditionalData {
public:
    //! Default constructor
    CMSLegData() : LegAdditionalData("CMS"), fixingDays_(0), isInArrears_(true), nakedOption_(false) {}
    //! Constructor
    CMSLegData(const string& swapIndex, int fixingDays, bool isInArrears, const vector<double>& spreads,
               const vector<string>& spreadDates = vector<string>(), const vector<double>& caps = vector<double>(),
               const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
               const vector<string>& floorDates = vector<string>(), const vector<double>& gearings = vector<double>(),
               const vector<string>& gearingDates = vector<string>(), bool nakedOption = false)
        : LegAdditionalData("CMS"), swapIndex_(swapIndex), fixingDays_(fixingDays), isInArrears_(isInArrears),
          spreads_(spreads), spreadDates_(spreadDates), caps_(caps), capDates_(capDates), floors_(floors),
          floorDates_(floorDates), gearings_(gearings), gearingDates_(gearingDates), nakedOption_(nakedOption) {}

    //! \name Inspectors
    //@{
    const string& swapIndex() const { return swapIndex_; }
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
    bool nakedOption() const { return nakedOption_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    string swapIndex_;
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
    bool nakedOption_;
};

//! Serializable CMS Spread Leg Data
/*!
\ingroup tradedata
*/
class CMSSpreadLegData : public LegAdditionalData {
public:
    //! Default constructor
    CMSSpreadLegData() : LegAdditionalData("CMSSpread"), fixingDays_(0), isInArrears_(true), nakedOption_(false) {}
    //! Constructor
    CMSSpreadLegData(const string& swapIndex1, const string& swapIndex2, int fixingDays, bool isInArrears,
                     const vector<double>& spreads, const vector<string>& spreadDates = vector<string>(),
                     const vector<double>& caps = vector<double>(), const vector<string>& capDates = vector<string>(),
                     const vector<double>& floors = vector<double>(),
                     const vector<string>& floorDates = vector<string>(),
                     const vector<double>& gearings = vector<double>(),
                     const vector<string>& gearingDates = vector<string>(), bool nakedOption = false)
        : LegAdditionalData("CMSSpread"), swapIndex1_(swapIndex1), swapIndex2_(swapIndex2), fixingDays_(fixingDays),
          isInArrears_(isInArrears), spreads_(spreads), spreadDates_(spreadDates), caps_(caps), capDates_(capDates),
          floors_(floors), floorDates_(floorDates), gearings_(gearings), gearingDates_(gearingDates),
          nakedOption_(nakedOption) {}

    //! \name Inspectors
    //@{
    const string& swapIndex1() const { return swapIndex1_; }
    const string& swapIndex2() const { return swapIndex2_; }
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
    bool nakedOption() const { return nakedOption_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    string swapIndex1_;
    string swapIndex2_;
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
    bool nakedOption_;
};

//! Serializable object holding amortization rules
class AmortizationData : public XMLSerializable {
public:
    AmortizationData() : initialized_(false) {}

    AmortizationData(string type, double value, string startDate, string endDate, string frequency, bool underflow)
        : type_(type), value_(value), startDate_(startDate), endDate_(endDate), frequency_(frequency),
          underflow_(underflow), initialized_(true) {}

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);

    //! FixedAmount, RelativeToInitialNotional, RelativeToPreviousNotional, Annuity
    const string& type() const { return type_; }
    //! Interpretation depending on type()
    double value() const { return value_; }
    //! Amortization start date
    const string& startDate() const { return startDate_; }
    //! Amortization end date
    const string& endDate() const { return endDate_; }
    //! Amortization frequency
    const string& frequency() const { return frequency_; }
    //! Allow amortization below zero notional if true
    bool underflow() const { return underflow_; }
    bool initialized() const { return initialized_; }

private:
    string type_;
    double value_;
    string startDate_;
    string endDate_;
    string frequency_;
    bool underflow_;
    bool initialized_;
};

//! Serializable object holding leg data
class LegData : public XMLSerializable {
public:
    //! Default constructor
    LegData()
        : isPayer_(true), notionalInitialExchange_(false), notionalFinalExchange_(false),
          notionalAmortizingExchange_(false), isNotResetXCCY_(true), foreignAmount_(0.0), fixingDays_(0) {}

    //! Constructor with concrete leg data
    LegData(const boost::shared_ptr<LegAdditionalData>& innerLegData, bool isPayer, const string& currency,
            const ScheduleData& scheduleData = ScheduleData(), const string& dayCounter = "",
            const std::vector<double>& notionals = std::vector<double>(),
            const std::vector<string>& notionalDates = std::vector<string>(), const string& paymentConvention = "F",
            const bool notionalInitialExchange = false, const bool notionalFinalExchange = false,
            const bool notionalAmortizingExchange = false, const bool isNotResetXCCY = true,
            const string& foreignCurrency = "", const double foreignAmount = 0, const string& fxIndex = "",
            int fixingDays = 0, const string& fixingCalendar = "",
            const std::vector<AmortizationData>& amortizationData = std::vector<AmortizationData>());

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
    bool isNotResetXCCY() const { return isNotResetXCCY_; }
    const string& foreignCurrency() const { return foreignCurrency_; }
    double foreignAmount() const { return foreignAmount_; }
    const string& fxIndex() const { return fxIndex_; }
    int fixingDays() const { return fixingDays_; }
    const string& fixingCalendar() const { return fixingCalendar_; }
    const std::vector<AmortizationData>& amortizationData() const { return amortizationData_; }
    //
    const string& legType() const { return concreteLegData_->legType(); }
    boost::shared_ptr<LegAdditionalData> concreteLegData() const { return concreteLegData_; }
    //@}

protected:
    virtual boost::shared_ptr<LegAdditionalData> initialiseConcreteLegData(const string&);

private:
    boost::shared_ptr<LegAdditionalData> concreteLegData_;
    bool isPayer_;
    string currency_;
    string legType_;
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
    string fixingCalendar_;
    std::vector<AmortizationData> amortizationData_;
};

//! \name Utilities for building QuantLib Legs
//@{
Leg makeFixedLeg(const LegData& data);
Leg makeIborLeg(const LegData& data, const boost::shared_ptr<IborIndex>& index,
                const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer = true);
Leg makeOISLeg(const LegData& data, const boost::shared_ptr<OvernightIndex>& index);
Leg makeBMALeg(const LegData& data, const boost::shared_ptr<QuantExt::BMAIndexWrapper>& indexWrapper);
Leg makeSimpleLeg(const LegData& data);
Leg makeNotionalLeg(const Leg& refLeg, const bool initNomFlow, const bool finalNomFlow, const bool amortNomFlow = true);
Leg makeCPILeg(const LegData& data, const boost::shared_ptr<ZeroInflationIndex>& index);
Leg makeYoYLeg(const LegData& data, const boost::shared_ptr<YoYInflationIndex>& index);
Leg makeCMSLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapIndex>& swapindex,
               const boost::shared_ptr<EngineFactory>& engineFactory, const vector<double>& caps = vector<double>(),
               const vector<double>& floors = vector<double>(), const bool attachPricer = true);
Leg makeCMSSpreadLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapSpreadIndex>& swapSpreadIndex,
                     const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer = true);
Real currentNotional(const Leg& leg);

//@}

//! Build a full vector of values from the given node.
//  For use with Notionals, Rates, Spreads, Gearing, Caps and Floor rates.
//  In all cases we can expand the vector to take the given schedule into account
vector<double> buildScheduledVector(const vector<double>& values, const vector<string>& dates,
                                    const Schedule& schedule);

// extend values to schedule size (if values is empty, the default value is used)
vector<double> normaliseToSchedule(const vector<double>& values, const Schedule& schedule,
                                   const Real defaultValue = Null<Real>());

// normaliseToSchedule concat buildScheduledVector
vector<double> buildScheduledVectorNormalised(const vector<double>& values, const vector<string>& dates,
                                              const Schedule& schedule, const Real defaultValue = Null<Real>());

// notional vector derived from a fixed amortisation amount
vector<double> buildAmortizationScheduleFixedAmount(const vector<double>& notionals, const Schedule& schedule,
                                                    const AmortizationData& data);

// notional vector with amortizations expressed as a percentage of initial notional
vector<double> buildAmortizationScheduleRelativeToInitialNotional(const vector<double>& notionals,
                                                                  const Schedule& schedule,
                                                                  const AmortizationData& data);

// notional vector with amortizations expressed as a percentage of the respective previous notional
vector<double> buildAmortizationScheduleRelativeToPreviousNotional(const vector<double>& notionals,
                                                                   const Schedule& schedule,
                                                                   const AmortizationData& data);

// notional vector derived from a fixed annuity amount
vector<double> buildAmortizationScheduleFixedAnnuity(const vector<double>& notionals, const vector<double>& rates,
                                                     const Schedule& schedule, const AmortizationData& data,
                                                     const DayCounter& dc);
} // namespace data
} // namespace ore
