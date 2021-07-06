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
#include <ored/portfolio/indexing.hpp>
#include <ored/portfolio/legdatafactory.hpp>
#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/underlying.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/cashflow.hpp>
#include <ql/experimental/coupons/swapspreadindex.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/position.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/indexes/equityindex.hpp>

#include <vector>

namespace ore {
namespace data {
using namespace QuantLib;
using std::string;

class EngineFactory;
class Market;
class RequiredFixings;

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
    const std::set<std::string>& indices() const { return indices_; }

protected:
    /*! Store the set of ORE index names that appear on this leg.
        Should be populated by derived classes.
    */
    std::set<std::string> indices_;

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
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    vector<double> amounts_;
    vector<string> dates_;

    static LegDataRegister<CashflowData> reg_;
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
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    vector<double> rates_;
    vector<string> rateDates_;

    static LegDataRegister<FixedLegData> reg_;
};

//! Serializable Fixed Leg Data
/*!
  \ingroup tradedata
*/
class ZeroCouponFixedLegData : public LegAdditionalData {
public:
    //! Default constructor
    ZeroCouponFixedLegData() : LegAdditionalData("ZeroCouponFixed"), subtractNotional_(true) {}
    //! Constructor
    ZeroCouponFixedLegData(const vector<double>& rates, const vector<string>& rateDates = vector<string>(),
                           const string& compounding = "Compounded", const bool subtractNotional = true)
        : LegAdditionalData("ZeroCouponFixed"), rates_(rates), rateDates_(rateDates), compounding_(compounding),
          subtractNotional_(subtractNotional) {}

    //! \name Inspectors
    //@{
    const vector<double>& rates() const { return rates_; }
    const vector<string>& rateDates() const { return rateDates_; }
    const string& compounding() const { return compounding_; }
    const bool& subtractNotional() const { return subtractNotional_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    vector<double> rates_;
    vector<string> rateDates_;
    string compounding_;
    bool subtractNotional_;

    static LegDataRegister<ZeroCouponFixedLegData> reg_;
};

//! Serializable Floating Leg Data
/*!
  \ingroup tradedata
*/
class FloatingLegData : public LegAdditionalData {
public:
    //! Default constructor
    FloatingLegData()
        : LegAdditionalData("Floating"), fixingDays_(Null<Size>()), lookback_(0 * Days), rateCutoff_(Null<Size>()),
          isAveraged_(false), hasSubPeriods_(false), includeSpread_(false), nakedOption_(false) {}
    //! Constructor
    FloatingLegData(const string& index, QuantLib::Size fixingDays, bool isInArrears, const vector<double>& spreads,
                    const vector<string>& spreadDates = vector<string>(), const vector<double>& caps = vector<double>(),
                    const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
                    const vector<string>& floorDates = vector<string>(),
                    const vector<double>& gearings = vector<double>(),
                    const vector<string>& gearingDates = vector<string>(), bool isAveraged = false,
                    bool nakedOption = false, bool hasSubPeriods = false, bool includeSpread = false,
                    QuantLib::Period lookback = 0 * Days, const Size rateCutoff = Null<Size>(),
                    bool localCapFloor = false, const boost::optional<Period>& lastRecentPeriod = boost::none,
                    const std::string& lastRecentPeriodCalendar = std::string())
        : LegAdditionalData("Floating"), index_(ore::data::internalIndexName(index)), fixingDays_(fixingDays),
          lookback_(lookback), rateCutoff_(rateCutoff), isInArrears_(isInArrears), isAveraged_(isAveraged),
          hasSubPeriods_(hasSubPeriods), includeSpread_(includeSpread), spreads_(spreads), spreadDates_(spreadDates),
          caps_(caps), capDates_(capDates), floors_(floors), floorDates_(floorDates), gearings_(gearings),
          gearingDates_(gearingDates), nakedOption_(nakedOption), localCapFloor_(localCapFloor),
          lastRecentPeriod_(lastRecentPeriod), lastRecentPeriodCalendar_(lastRecentPeriodCalendar) {
        indices_.insert(index_);
    }

    //! \name Inspectors
    //@{
    const string& index() const { return index_; }
    QuantLib::Size fixingDays() const { return fixingDays_; }
    QuantLib::Period lookback() const { return lookback_; }
    QuantLib::Size rateCutoff() const { return rateCutoff_; }
    boost::optional<bool> isInArrears() const { return isInArrears_; }
    bool isAveraged() const { return isAveraged_; }
    bool hasSubPeriods() const { return hasSubPeriods_; }
    bool includeSpread() const { return includeSpread_; }
    const vector<double>& spreads() const { return spreads_; }
    const vector<string>& spreadDates() const { return spreadDates_; }
    const vector<double>& caps() const { return caps_; }
    const vector<string>& capDates() const { return capDates_; }
    const vector<double>& floors() const { return floors_; }
    const vector<string>& floorDates() const { return floorDates_; }
    const vector<double>& gearings() const { return gearings_; }
    const vector<string>& gearingDates() const { return gearingDates_; }
    bool nakedOption() const { return nakedOption_; }
    bool localCapFloor() const { return localCapFloor_; }
    const boost::optional<Period>& lastRecentPeriod() const { return lastRecentPeriod_; }
    const std::string& lastRecentPeriodCalendar() const { return lastRecentPeriodCalendar_; }
    //@}

    //! \name Modifiers
    //@{
    vector<double>& caps() { return caps_; }
    vector<string>& capDates() { return capDates_; }
    vector<double>& floors() { return floors_; }
    vector<string>& floorDates() { return floorDates_; }
    bool& nakedOption() { return nakedOption_; }
    bool& localCapFloor() { return localCapFloor_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    string index_;
    QuantLib::Size fixingDays_;
    QuantLib::Period lookback_;
    QuantLib::Size rateCutoff_;
    boost::optional<bool> isInArrears_;
    bool isAveraged_;
    bool hasSubPeriods_;
    bool includeSpread_;
    vector<double> spreads_;
    vector<string> spreadDates_;
    vector<double> caps_;
    vector<string> capDates_;
    vector<double> floors_;
    vector<string> floorDates_;
    vector<double> gearings_;
    vector<string> gearingDates_;
    bool nakedOption_;
    bool localCapFloor_;
    boost::optional<Period> lastRecentPeriod_;
    std::string lastRecentPeriodCalendar_;

    static LegDataRegister<FloatingLegData> reg_;
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
    CPILegData(string index, string startDate, double baseCPI, string observationLag, string interpolation,
               const vector<double>& rates, const vector<string>& rateDates = std::vector<string>(),
               bool subtractInflationNominal = true, const vector<double>& caps = vector<double>(),
               const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
               const vector<string>& floorDates = vector<string>(), double finalFlowCap = Null<Real>(),
               double finalFlowFloor = Null<Real>(), bool nakedOption = false,
               bool subtractInflationNominalCoupons = false)
        : LegAdditionalData("CPI"), index_(index), startDate_(startDate), baseCPI_(baseCPI),
          observationLag_(observationLag), interpolation_(interpolation), rates_(rates), rateDates_(rateDates),
          subtractInflationNominal_(subtractInflationNominal), caps_(caps), capDates_(capDates), floors_(floors),
          floorDates_(floorDates), finalFlowCap_(finalFlowCap), finalFlowFloor_(finalFlowFloor),
          nakedOption_(nakedOption), subtractInflationNominalCoupons_(subtractInflationNominalCoupons) {
        indices_.insert(index_);
    }

    //! \name Inspectors
    //@{
    const string& index() const { return index_; }
    const string& startDate() const { return startDate_; }
    double baseCPI() const { return baseCPI_; }
    const string& observationLag() const { return observationLag_; }
    const string& interpolation() const { return interpolation_; }
    const std::vector<double>& rates() const { return rates_; }
    const std::vector<string>& rateDates() const { return rateDates_; }
    bool subtractInflationNominal() const { return subtractInflationNominal_; }
    const vector<double>& caps() const { return caps_; }
    const vector<string>& capDates() const { return capDates_; }
    const vector<double>& floors() const { return floors_; }
    const vector<string>& floorDates() const { return floorDates_; }
    double finalFlowCap() const { return finalFlowCap_; }
    double finalFlowFloor() const { return finalFlowFloor_; }
    bool nakedOption() const { return nakedOption_; }
    bool subtractInflationNominalCoupons() const { return subtractInflationNominalCoupons_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    string index_;
    string startDate_;
    double baseCPI_;
    string observationLag_;
    string interpolation_;
    vector<double> rates_;
    vector<string> rateDates_;
    bool subtractInflationNominal_;
    vector<double> caps_;
    vector<string> capDates_;
    vector<double> floors_;
    vector<string> floorDates_;
    double finalFlowCap_;
    double finalFlowFloor_;
    bool nakedOption_;
    bool subtractInflationNominalCoupons_;

    static LegDataRegister<CPILegData> reg_;
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
    YoYLegData(string index, string observationLag, Size fixingDays,
               const vector<double>& gearings = std::vector<double>(),
               const vector<string>& gearingDates = std::vector<string>(),
               const vector<double>& spreads = std::vector<double>(),
               const vector<string>& spreadDates = std::vector<string>(), const vector<double>& caps = vector<double>(),
               const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
               const vector<string>& floorDates = vector<string>(), bool nakedOption = false,
               bool addInflationNotional = false, bool irregularYoY = false)
        : LegAdditionalData("YY"), index_(index), observationLag_(observationLag), fixingDays_(fixingDays),
          gearings_(gearings), gearingDates_(gearingDates), spreads_(spreads), spreadDates_(spreadDates), caps_(caps),
          capDates_(capDates), floors_(floors), floorDates_(floorDates), nakedOption_(nakedOption),
          addInflationNotional_(addInflationNotional), irregularYoY_(irregularYoY) {
        indices_.insert(index_);
    }

    //! \name Inspectors
    //@{
    const string index() const { return index_; }
    const string observationLag() const { return observationLag_; }
    Size fixingDays() const { return fixingDays_; }
    const std::vector<double>& gearings() const { return gearings_; }
    const std::vector<string>& gearingDates() const { return gearingDates_; }
    const std::vector<double>& spreads() const { return spreads_; }
    const std::vector<string>& spreadDates() const { return spreadDates_; }
    const vector<double>& caps() const { return caps_; }
    const vector<string>& capDates() const { return capDates_; }
    const vector<double>& floors() const { return floors_; }
    const vector<string>& floorDates() const { return floorDates_; }
    bool nakedOption() const { return nakedOption_; }
    bool addInflationNotional() const { return addInflationNotional_; };
    bool irregularYoY() const { return irregularYoY_; };
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

private:
    string index_;
    string observationLag_;
    Size fixingDays_;
    vector<double> gearings_;
    vector<string> gearingDates_;
    vector<double> spreads_;
    vector<string> spreadDates_;
    vector<double> caps_;
    vector<string> capDates_;
    vector<double> floors_;
    vector<string> floorDates_;
    bool nakedOption_;
    bool addInflationNotional_;
    bool irregularYoY_;

    static LegDataRegister<YoYLegData> reg_;
};

//! Serializable CMS Leg Data
/*!
\ingroup tradedata
*/
class CMSLegData : public LegAdditionalData {
public:
    //! Default constructor
    CMSLegData() : LegAdditionalData("CMS"), fixingDays_(Null<Size>()), isInArrears_(true), nakedOption_(false) {}
    //! Constructor
    CMSLegData(const string& swapIndex, Size fixingDays, bool isInArrears, const vector<double>& spreads,
               const vector<string>& spreadDates = vector<string>(), const vector<double>& caps = vector<double>(),
               const vector<string>& capDates = vector<string>(), const vector<double>& floors = vector<double>(),
               const vector<string>& floorDates = vector<string>(), const vector<double>& gearings = vector<double>(),
               const vector<string>& gearingDates = vector<string>(), bool nakedOption = false)
        : LegAdditionalData("CMS"), swapIndex_(swapIndex), fixingDays_(fixingDays), isInArrears_(isInArrears),
          spreads_(spreads), spreadDates_(spreadDates), caps_(caps), capDates_(capDates), floors_(floors),
          floorDates_(floorDates), gearings_(gearings), gearingDates_(gearingDates), nakedOption_(nakedOption) {
        indices_.insert(swapIndex_);
    }

    //! \name Inspectors
    //@{
    const string& swapIndex() const { return swapIndex_; }
    Size fixingDays() const { return fixingDays_; }
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
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    string swapIndex_;
    Size fixingDays_;
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

    static LegDataRegister<CMSLegData> reg_;
};

//! Serializable Digital CMS Leg Data
/*!
\ingroup tradedata
*/
class DigitalCMSLegData : public LegAdditionalData {
public:
    //! Default constructor
    DigitalCMSLegData() : LegAdditionalData("DigitalCMS") {}
    //! Constructor
    DigitalCMSLegData(const boost::shared_ptr<CMSLegData>& underlying, Position::Type callPosition = Position::Long,
                      bool isCallATMIncluded = false, const vector<double> callStrikes = vector<double>(),
                      const vector<string> callStrikeDates = vector<string>(),
                      const vector<double> callPayoffs = vector<double>(),
                      const vector<string> callPayoffDates = vector<string>(),
                      Position::Type putPosition = Position::Long, bool isPutATMIncluded = false,
                      const vector<double> putStrikes = vector<double>(),
                      const vector<string> putStrikeDates = vector<string>(),
                      const vector<double> putPayoffs = vector<double>(),
                      const vector<string> putPayoffDates = vector<string>())
        : LegAdditionalData("DigitalCMS"), underlying_(underlying), callPosition_(callPosition),
          isCallATMIncluded_(isCallATMIncluded), callStrikes_(callStrikes), callStrikeDates_(callStrikeDates),
          callPayoffs_(callPayoffs), callPayoffDates_(callPayoffDates), putPosition_(putPosition),
          isPutATMIncluded_(isPutATMIncluded), putStrikes_(putStrikes), putStrikeDates_(putStrikeDates),
          putPayoffs_(putPayoffs), putPayoffDates_(putPayoffDates) {
        indices_ = underlying_->indices();
    }

    //! \name Inspectors
    //@{
    const boost::shared_ptr<CMSLegData>& underlying() const { return underlying_; }

    const Position::Type callPosition() const { return callPosition_; }
    const bool isCallATMIncluded() const { return isCallATMIncluded_; }
    const vector<double> callStrikes() const { return callStrikes_; }
    const vector<double> callPayoffs() const { return callPayoffs_; }
    const vector<string> callStrikeDates() const { return callStrikeDates_; }
    const vector<string> callPayoffDates() const { return callPayoffDates_; }

    const Position::Type putPosition() const { return putPosition_; }
    const bool isPutATMIncluded() const { return isPutATMIncluded_; }
    const vector<double> putStrikes() const { return putStrikes_; }
    const vector<double> putPayoffs() const { return putPayoffs_; }
    const vector<string> putStrikeDates() const { return putStrikeDates_; }
    const vector<string> putPayoffDates() const { return putPayoffDates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    boost::shared_ptr<CMSLegData> underlying_;

    Position::Type callPosition_;
    bool isCallATMIncluded_;
    vector<double> callStrikes_;
    vector<string> callStrikeDates_;
    vector<double> callPayoffs_;
    vector<string> callPayoffDates_;

    Position::Type putPosition_;
    bool isPutATMIncluded_;
    vector<double> putStrikes_;
    vector<string> putStrikeDates_;
    vector<double> putPayoffs_;
    vector<string> putPayoffDates_;

    static LegDataRegister<DigitalCMSLegData> reg_;
};

//! Serializable CMS Spread Leg Data
/*!
\ingroup tradedata
*/
class CMSSpreadLegData : public LegAdditionalData {
public:
    //! Default constructor
    CMSSpreadLegData()
        : LegAdditionalData("CMSSpread"), fixingDays_(Null<Size>()), isInArrears_(false), nakedOption_(false) {}
    //! Constructor
    CMSSpreadLegData(const string& swapIndex1, const string& swapIndex2, Size fixingDays, bool isInArrears,
                     const vector<double>& spreads, const vector<string>& spreadDates = vector<string>(),
                     const vector<double>& caps = vector<double>(), const vector<string>& capDates = vector<string>(),
                     const vector<double>& floors = vector<double>(),
                     const vector<string>& floorDates = vector<string>(),
                     const vector<double>& gearings = vector<double>(),
                     const vector<string>& gearingDates = vector<string>(), bool nakedOption = false)
        : LegAdditionalData("CMSSpread"), swapIndex1_(swapIndex1), swapIndex2_(swapIndex2), fixingDays_(fixingDays),
          isInArrears_(isInArrears), spreads_(spreads), spreadDates_(spreadDates), caps_(caps), capDates_(capDates),
          floors_(floors), floorDates_(floorDates), gearings_(gearings), gearingDates_(gearingDates),
          nakedOption_(nakedOption) {
        indices_.insert(swapIndex1_);
        indices_.insert(swapIndex2_);
    }

    //! \name Inspectors
    //@{
    const string& swapIndex1() const { return swapIndex1_; }
    const string& swapIndex2() const { return swapIndex2_; }
    Size fixingDays() const { return fixingDays_; }
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
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    string swapIndex1_;
    string swapIndex2_;
    Size fixingDays_;
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

    static LegDataRegister<CMSSpreadLegData> reg_;
};

//! Serializable Digital CMS Spread Leg Data
/*!
\ingroup tradedata
*/
class DigitalCMSSpreadLegData : public LegAdditionalData {
public:
    //! Default constructor
    DigitalCMSSpreadLegData() : LegAdditionalData("DigitalCMSSpread") {}
    //! Constructor
    DigitalCMSSpreadLegData(
        const boost::shared_ptr<CMSSpreadLegData>& underlying, Position::Type callPosition = Position::Long,
        bool isCallATMIncluded = false, const vector<double> callStrikes = vector<double>(),
        const vector<string> callStrikeDates = vector<string>(), const vector<double> callPayoffs = vector<double>(),
        const vector<string> callPayoffDates = vector<string>(), Position::Type putPosition = Position::Long,
        bool isPutATMIncluded = false, const vector<double> putStrikes = vector<double>(),
        const vector<string> putStrikeDates = vector<string>(), const vector<double> putPayoffs = vector<double>(),
        const vector<string> putPayoffDates = vector<string>())
        : LegAdditionalData("DigitalCMSSpread"), underlying_(underlying), callPosition_(callPosition),
          isCallATMIncluded_(isCallATMIncluded), callStrikes_(callStrikes), callStrikeDates_(callStrikeDates),
          callPayoffs_(callPayoffs), callPayoffDates_(callPayoffDates), putPosition_(putPosition),
          isPutATMIncluded_(isPutATMIncluded), putStrikes_(putStrikes), putStrikeDates_(putStrikeDates),
          putPayoffs_(putPayoffs), putPayoffDates_(putPayoffDates) {
        indices_ = underlying_->indices();
    }

    //! \name Inspectors
    //@{
    const boost::shared_ptr<CMSSpreadLegData>& underlying() const { return underlying_; }

    const Position::Type callPosition() const { return callPosition_; }
    const bool isCallATMIncluded() const { return isCallATMIncluded_; }
    const vector<double> callStrikes() const { return callStrikes_; }
    const vector<double> callPayoffs() const { return callPayoffs_; }
    const vector<string> callStrikeDates() const { return callStrikeDates_; }
    const vector<string> callPayoffDates() const { return callPayoffDates_; }

    const Position::Type putPosition() const { return putPosition_; }
    const bool isPutATMIncluded() const { return isPutATMIncluded_; }
    const vector<double> putStrikes() const { return putStrikes_; }
    const vector<double> putPayoffs() const { return putPayoffs_; }
    const vector<string> putStrikeDates() const { return putStrikeDates_; }
    const vector<string> putPayoffDates() const { return putPayoffDates_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    boost::shared_ptr<CMSSpreadLegData> underlying_;

    Position::Type callPosition_;
    bool isCallATMIncluded_;
    vector<double> callStrikes_;
    vector<string> callStrikeDates_;
    vector<double> callPayoffs_;
    vector<string> callPayoffDates_;

    Position::Type putPosition_;
    bool isPutATMIncluded_;
    vector<double> putStrikes_;
    vector<string> putStrikeDates_;
    vector<double> putPayoffs_;
    vector<string> putPayoffDates_;

    static LegDataRegister<DigitalCMSSpreadLegData> reg_;
};

//! Serializable Fixed Leg Data
/*!
\ingroup tradedata
*/
class EquityLegData : public LegAdditionalData {
public:
    //! Default constructor
    EquityLegData() : LegAdditionalData("Equity"), quantity_(Null<Real>()) {}
    //! Constructor
    EquityLegData(string returnType, Real dividendFactor, EquityUnderlying equityUnderlying, Real initialPrice,
                  bool notionalReset, Natural fixingDays = 0, const ScheduleData& valuationSchedule = ScheduleData(),
                  string eqCurrency = "", string fxIndex = "", Natural fxIndexFixingDays = 2,
                  string fxIndexCalendar = "", Real quantity = Null<Real>(), string initialPriceCurrency = "")
        : LegAdditionalData("Equity"), returnType_(returnType), dividendFactor_(dividendFactor),
          equityUnderlying_(equityUnderlying), initialPrice_(initialPrice), notionalReset_(notionalReset),
          fixingDays_(fixingDays), valuationSchedule_(valuationSchedule), eqCurrency_(eqCurrency), fxIndex_(fxIndex),
          fxIndexFixingDays_(fxIndexFixingDays), fxIndexCalendar_(fxIndexCalendar), quantity_(quantity),
          initialPriceCurrency_(initialPriceCurrency) {
        indices_.insert("EQ-" + eqName());
    }

    //! \name Inspectors
    //@{
    const string& returnType() const { return returnType_; }
    string eqName() { return equityUnderlying_.name(); }
    Real dividendFactor() const { return dividendFactor_; }
    EquityUnderlying equityIdentifier() const { return equityUnderlying_; }
    Real initialPrice() const { return initialPrice_; }
    Natural fixingDays() const { return fixingDays_; }
    ScheduleData valuationSchedule() const { return valuationSchedule_; }
    const string& eqCurrency() const { return eqCurrency_; }
    const string& fxIndex() const { return fxIndex_; }
    Natural fxIndexFixingDays() const { return fxIndexFixingDays_; }
    const string& fxIndexCalendar() const { return fxIndexCalendar_; }
    bool notionalReset() const { return notionalReset_; }
    Real quantity() const { return quantity_; }                                  // might be null
    const string& initialPriceCurrency() const { return initialPriceCurrency_; } // might be empty
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}
private:
    string returnType_;
    Real dividendFactor_ = 1.0;
    EquityUnderlying equityUnderlying_;
    Real initialPrice_;
    bool notionalReset_ = false;
    Natural fixingDays_ = 0;
    ScheduleData valuationSchedule_;
    string eqCurrency_ = "";
    string fxIndex_ = "";
    Natural fxIndexFixingDays_ = 2;
    string fxIndexCalendar_ = "";
    Real quantity_;
    string initialPriceCurrency_;

    static LegDataRegister<EquityLegData> reg_;
};

//! Serializable object holding amortization rules
class AmortizationData : public XMLSerializable {
public:
    AmortizationData() : initialized_(false) {}

    AmortizationData(string type, double value, string startDate, string endDate, string frequency, bool underflow)
        : type_(type), value_(value), startDate_(startDate), endDate_(endDate), frequency_(frequency),
          underflow_(underflow), initialized_(true) {}

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;

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
          notionalAmortizingExchange_(false), isNotResetXCCY_(true), foreignAmount_(0.0), fixingDays_(0),
          indexingFromAssetLeg_(false) {}

    //! Constructor with concrete leg data
    LegData(const boost::shared_ptr<LegAdditionalData>& innerLegData, bool isPayer, const string& currency,
            const ScheduleData& scheduleData = ScheduleData(), const string& dayCounter = "",
            const std::vector<double>& notionals = std::vector<double>(),
            const std::vector<string>& notionalDates = std::vector<string>(), const string& paymentConvention = "F",
            const bool notionalInitialExchange = false, const bool notionalFinalExchange = false,
            const bool notionalAmortizingExchange = false, const bool isNotResetXCCY = true,
            const string& foreignCurrency = "", const double foreignAmount = 0, const string& fxIndex = "",
            int fixingDays = 0, const string& fixingCalendar = "",
            const std::vector<AmortizationData>& amortizationData = std::vector<AmortizationData>(),
            const int paymentLag = 0, const std::string& paymentCalendar = "",
            const std::vector<std::string>& paymentDates = std::vector<std::string>(),
            const std::vector<Indexing>& indexing = {}, const bool indexingFromAssetLeg = false,
            const string& lastPeriodDayCounter = "");

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
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
    const int paymentLag() const { return paymentLag_; }
    const std::vector<AmortizationData>& amortizationData() const { return amortizationData_; }
    const std::string& paymentCalendar() const { return paymentCalendar_; }
    const string& legType() const { return concreteLegData_->legType(); }
    boost::shared_ptr<LegAdditionalData> concreteLegData() const { return concreteLegData_; }
    const std::set<std::string>& indices() const { return indices_; }
    const std::vector<std::string>& paymentDates() const { return paymentDates_; }
    const std::vector<Indexing>& indexing() const { return indexing_; }
    const bool indexingFromAssetLeg() const { return indexingFromAssetLeg_; }
    const string& lastPeriodDayCounter() const { return lastPeriodDayCounter_; }
    //@}

    //! \name modifiers
    //@{
    vector<double>& notionals() { return notionals_; }
    vector<string>& notionalDates() { return notionalDates_; }
    bool& isPayer() { return isPayer_; }
    boost::shared_ptr<LegAdditionalData>& concreteLegData() { return concreteLegData_; }
    std::vector<Indexing>& indexing() { return indexing_; }
    bool& indexingFromAssetLeg() { return indexingFromAssetLeg_; }
    //@}

protected:
    virtual boost::shared_ptr<LegAdditionalData> initialiseConcreteLegData(const string&);

    /*! Store the set of ORE index names that appear on this leg.

        Take the set appearing in LegAdditionalData::indices() and add on any appearing here. Currently, the only
        possible extra index appearing at LegData level is \c fxIndex.
    */
    std::set<std::string> indices_;

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
    int paymentLag_;
    std::string paymentCalendar_;
    std::vector<std::string> paymentDates_;
    std::vector<Indexing> indexing_;
    bool indexingFromAssetLeg_;
    string lastPeriodDayCounter_;
};

//! \name Utilities for building QuantLib Legs
//@{
Leg makeFixedLeg(const LegData& data, const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeZCFixedLeg(const LegData& data, const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeIborLeg(const LegData& data, const boost::shared_ptr<IborIndex>& index,
                const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer = true,
                const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeOISLeg(const LegData& data, const boost::shared_ptr<OvernightIndex>& index,
               const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer = true,
               const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeBMALeg(const LegData& data, const boost::shared_ptr<QuantExt::BMAIndexWrapper>& indexWrapper,
               const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeSimpleLeg(const LegData& data);
Leg makeNotionalLeg(const Leg& refLeg, const bool initNomFlow, const bool finalNomFlow, const bool amortNomFlow,
                    const BusinessDayConvention paymentConvention, const Calendar paymentCalendar);
Leg makeCPILeg(const LegData& data, const boost::shared_ptr<ZeroInflationIndex>& index,
               const boost::shared_ptr<EngineFactory>& engineFactory,
               const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeYoYLeg(const LegData& data, const boost::shared_ptr<InflationIndex>& index,
               const boost::shared_ptr<EngineFactory>& engineFactory,
               const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeCMSLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapIndex>& swapindex,
               const boost::shared_ptr<EngineFactory>& engineFactory, const vector<double>& caps = vector<double>(),
               const vector<double>& floors = vector<double>(), const bool attachPricer = true,
               const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeDigitalCMSLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapIndex>& swapIndex,
                      const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer = true,
                      const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeCMSSpreadLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapSpreadIndex>& swapSpreadIndex,
                     const boost::shared_ptr<EngineFactory>& engineFactory, const bool attachPricer = true,
                     const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeDigitalCMSSpreadLeg(const LegData& data, const boost::shared_ptr<QuantLib::SwapSpreadIndex>& swapSpreadIndex,
                            const boost::shared_ptr<EngineFactory>& engineFactory,
                            const QuantLib::Date& openEndDateReplacement = Null<Date>());
Leg makeEquityLeg(const LegData& data, const boost::shared_ptr<QuantExt::EquityIndex>& equityCurve,
                  const boost::shared_ptr<QuantExt::FxIndex>& fxIndex = nullptr,
                  const QuantLib::Date& openEndDateReplacement = Null<Date>());
Real currentNotional(const Leg& leg);
Real originalNotional(const Leg& leg);

//@}

//! Build a full vector of values from the given node.
//  For use with Notionals, Rates, Spreads, Gearing, Caps and Floor rates.
//  In all cases we can expand the vector to take the given schedule into account
//  If checkAllValuesAppearInResult is true, we require that all input values are appearing in the result (in order)
template <typename T>
vector<T> buildScheduledVector(const vector<T>& values, const vector<string>& dates, const Schedule& schedule,
                               const bool checkAllValuesAppearInResult = false);

// extend values to schedule size (if values is empty, the default value is used)
template <typename T>
vector<T> normaliseToSchedule(const vector<T>& values, const Schedule& schedule, const T& defaultValue);

// normaliseToSchedule concat buildScheduledVector
template <typename T>
vector<T> buildScheduledVectorNormalised(const vector<T>& values, const vector<string>& dates, const Schedule& schedule,
                                         const T& defaultValue, const bool checkAllValuesAppearInResult = false);

/* returns an iterator to the first input value that is not appearing (in order) in the scheduled vector, or an iterator
   pointing to the end of the input value vector if no such element exists */
template <typename T>
typename vector<T>::const_iterator checkAllValuesAppearInScheduledVector(const vector<T>& scheduledVecotr,
                                                                         const vector<T>& inputValues);

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

// apply amortisation to given notionals
void applyAmortization(std::vector<Real>& notionals, const LegData& data, const Schedule& schedule,
                       const bool annuityAllowed = false, const std::vector<Real>& rates = std::vector<Real>());

// apply indexing (if given in LegData) to existing leg
void applyIndexing(Leg& leg, const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                   std::map<std::string, std::string>& qlToOREIndexNames, RequiredFixings& requiredFixings,
                   const QuantLib::Date& openEndDateReplacement = Null<Date>());

// template implementations

template <typename T>
vector<T> buildScheduledVector(const vector<T>& values, const vector<string>& dates, const Schedule& schedule,
                               const bool checkAllValuesAppearInResult) {
    if (values.size() < 2 || dates.size() == 0)
        return values;

    QL_REQUIRE(values.size() == dates.size(), "Value / Date size mismatch in buildScheduledVector."
                                                  << "Value:" << values.size() << ", Dates:" << dates.size());

    // Need to use schedule logic
    // Length of data will be 1 less than schedule
    //
    // Notional 100
    // Notional {startDate 2015-01-01} 200
    // Notional {startDate 2016-01-01} 300
    //
    // Given schedule June, Dec from 2014 to 2016 (6 dates, 5 coupons)
    // we return 100, 100, 200, 200, 300

    // The first node must not have a date.
    // If the second one has a date, all the rest must have, and we process
    // If the second one does not have a date, none of them must have one
    // and we return the vector uneffected.
    QL_REQUIRE(dates[0] == "", "Invalid date " << dates[0] << " for first node");
    if (dates[1] == "") {
        // They must all be empty and then we return values
        for (Size i = 2; i < dates.size(); i++) {
            QL_REQUIRE(dates[i] == "", "Invalid date " << dates[i] << " for node " << i
                                                       << ". Cannot mix dates and non-dates attributes");
        }
        return values;
    }

    // We have nodes with date attributes now
    Size len = schedule.size() - 1;
    vector<T> data(len);
    Size j = 0, max_j = dates.size() - 1; // j is the index of date/value vector. 0 to max_j
    Date d = parseDate(dates[j + 1]);     // The first start date
    for (Size i = 0; i < len; i++) {      // loop over data vector and populate it.
        // If j == max_j we just fall through and take the final value
        while (schedule[i] >= d && j < max_j) {
            j++;
            if (j < max_j) {
                QL_REQUIRE(dates[j + 1] != "", "Cannot have empty date attribute for node " << j + 1);
                d = parseDate(dates[j + 1]);
            }
        }
        data[i] = values[j];
    }

    if (checkAllValuesAppearInResult) {
        auto it = checkAllValuesAppearInScheduledVector<T>(data, values);
        QL_REQUIRE(it == values.end(),
                   "buildScheduledVector: input value '" << *it << " does not appear in the result vector");
    }

    return data;
}

template <typename T>
vector<T> normaliseToSchedule(const vector<T>& values, const Schedule& schedule, const T& defaultValue) {
    vector<T> res = values;
    if (res.size() < schedule.size() - 1)
        res.resize(schedule.size() - 1, res.size() == 0 ? defaultValue : res.back());
    return res;
}

template <typename T>
vector<T> buildScheduledVectorNormalised(const vector<T>& values, const vector<string>& dates, const Schedule& schedule,
                                         const T& defaultValue, const bool checkAllValuesAppearInResult) {
    return normaliseToSchedule(buildScheduledVector(values, dates, schedule, checkAllValuesAppearInResult), schedule,
                               defaultValue);
}

template <typename T>
typename vector<T>::const_iterator checkAllValuesAppearInScheduledVector(const vector<T>& scheduledVector,
                                                                         const vector<T>& inputValues) {
    auto s = scheduledVector.begin();
    auto i = inputValues.begin();
    while (i != inputValues.end()) {
        if (s == scheduledVector.end())
            return i;
        if (*i == *s) {
            ++i;
        } else {
            ++s;
        }
    }
    return i;
}

// build an FX Index needed by legbuilders / makeLeg methods
boost::shared_ptr<QuantExt::FxIndex> buildFxIndex(const string& fxIndex, const string& domestic, const string& foreign,
                                                  const boost::shared_ptr<Market>& market, const string& configuration,
                                                  const string& calendar, Size fixingDays = 0,
                                                  bool useXbsCurves = false);

// build a Bond Index needed by legbuilders (populates bond data from bond reference data if required)
class BondData;
boost::shared_ptr<QuantExt::BondIndex> buildBondIndex(const BondData& securityData, const bool dirty,
                                                      const bool relative, const Calendar& fixingCalendar,
                                                      const bool conditionalOnSurvival,
                                                      const boost::shared_ptr<EngineFactory>& engineFactory,
                                                      RequiredFixings& requiredFixings);

// join a vector of legs to a single leg, check if the legs have adjacent periods
Leg joinLegs(const std::vector<Leg>& legs);

// build a notional leg for a given coupon leg, returns an empty Leg if not applicable
Leg buildNotionalLeg(const LegData& data, const Leg& leg, RequiredFixings& requiredFixings,
                     const boost::shared_ptr<Market>& market, const std::string& configuration);

} // namespace data
} // namespace ore
