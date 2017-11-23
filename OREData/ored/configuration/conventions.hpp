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

/*! \file ored/configuration/conventions.hpp
    \brief Currency and instrument specific conventions/defaults
    \ingroup marketdata
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <qle/cashflows/subperiodscoupon.hpp> // SubPeriodsCouponType

using std::string;
using std::map;
using ore::data::XMLSerializable;
using ore::data::XMLNode;

namespace ore {
namespace data {

//! Abstract base class for convention objects
/*!
  \ingroup marketdata
 */
class Convention : public XMLSerializable {
public:
    //! Supported convention types
    enum class Type {
        Zero,
        Deposit,
        Future,
        FRA,
        OIS,
        Swap,
        AverageOIS,
        TenorBasisSwap,
        TenorBasisTwoSwap,
        FX,
        CrossCcyBasis,
        CDS,
        SwapIndex,
        InflationSwap,
        SecuritySpread
    };

    //! Default destructor
    virtual ~Convention() {}

    //! \name Inspectors
    //@{
    const string& id() const { return id_; }
    Type type() const { return type_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) = 0;
    virtual XMLNode* toXML(XMLDocument& doc) = 0;
    virtual void build() = 0;
    //@}

protected:
    Convention() {}
    Convention(const string& id, Type type);
    Type type_;
    string id_;
};

//! Container for storing Zero Rate conventions
/*!
  \ingroup marketdata
 */
class ZeroRateConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    ZeroRateConvention() {}
    ZeroRateConvention(const string& id, const string& dayCounter, const string& compounding,
                       const string& compoundingFrequency);
    ZeroRateConvention(const string& id, const string& dayCounter, const string& tenorCalendar,
                       const string& compounding = "Continuous", const string& compoundingFrequency = "Annual",
                       const string& spotLag = "", const string& spotCalendar = "", const string& rollConvention = "",
                       const string& eom = "");
    //@}

    //! \name Inspectors
    //@{
    //! Zero rate day counter
    const DayCounter& dayCounter() const { return dayCounter_; }
    //! Return the calendar used for converting tenor points into dates
    const Calendar& tenorCalendar() const { return tenorCalendar_; }
    //! Zero rate compounding
    Compounding compounding() const { return compounding_; }
    //! Zero rate compounding frequency
    Frequency compoundingFrequency() const { return compoundingFrequency_; }
    //! Zero rate spot lag
    Natural spotLag() const { return spotLag_; }
    //! Calendar used for spot date adjustment
    const Calendar& spotCalendar() const { return spotCalendar_; }
    //! Business day convention used in converting tenor points into dates
    BusinessDayConvention rollConvention() const { return rollConvention_; }
    //! End of month adjustment
    bool eom() { return eom_; }
    //! Flag to indicate whether the Zero Rate convention is based on a tenor input
    bool tenorBased() { return tenorBased_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    DayCounter dayCounter_;
    Calendar tenorCalendar_;
    Compounding compounding_;
    Frequency compoundingFrequency_;
    Natural spotLag_;
    Calendar spotCalendar_;
    BusinessDayConvention rollConvention_;
    bool eom_;
    bool tenorBased_;

    // Strings to store the inputs
    string strDayCounter_;
    string strTenorCalendar_;
    string strCompounding_;
    string strCompoundingFrequency_;
    string strSpotLag_;
    string strSpotCalendar_;
    string strRollConvention_;
    string strEom_;
};

//! Container for storing Deposit conventions
/*!
  \ingroup marketdata
 */
class DepositConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    DepositConvention() {}
    //! Index based constructor
    DepositConvention(const string& id, const string& index);
    //! Detailed constructor
    DepositConvention(const string& id, const string& calendar, const string& convention, const string& eom,
                      const string& dayCounter, const string& settlementDays);
    //@}

    //! \name Inspectors
    //@{
    const string& index() const { return index_; }
    const Calendar& calendar() const { return calendar_; }
    BusinessDayConvention convention() const { return convention_; }
    bool eom() { return eom_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Size settlementDays() const { return settlementDays_; }
    bool indexBased() { return indexBased_; }
    // @}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    string index_;
    Calendar calendar_;
    BusinessDayConvention convention_;
    bool eom_;
    DayCounter dayCounter_;
    Size settlementDays_;
    bool indexBased_;

    // Strings to store the inputs
    string strCalendar_;
    string strConvention_;
    string strEom_;
    string strDayCounter_;
    string strSettlementDays_;
};

//! Container for storing Money Market Futures conventions
/*!
  \ingroup marketdata
 */
class FutureConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    FutureConvention() {}
    //! Index based constructor
    FutureConvention(const string& id, const string& index);
    //@}
    //! \name Inspectors
    //@{
    const boost::shared_ptr<IborIndex>& index() const { return index_; }
    //@}

    //! Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build() {}
    //@}

private:
    string strIndex_;
    boost::shared_ptr<IborIndex> index_;
};

//! Container for storing Forward rate Agreement conventions
/*!
  \ingroup marketdata
 */
class FraConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    FraConvention() {}
    //! Index based constructor
    FraConvention(const string& id, const string& index);
    //@}

    //! \name Inspectors
    //@{
    const boost::shared_ptr<IborIndex>& index() const { return index_; }
    const string& indexName() const { return strIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build() {}
    //@}

private:
    string strIndex_;
    boost::shared_ptr<IborIndex> index_;
};

//! Container for storing Overnight Index Swap conventions
/*!
  \ingroup marketdata
 */
class OisConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    OisConvention() {}
    //! Detailed constructor
    OisConvention(const string& id, const string& spotLag, const string& index, const string& fixedDayCounter,
                  const string& paymentLag = "", const string& eom = "", const string& fixedFrequency = "",
                  const string& fixedConvention = "", const string& fixedPaymentConvention = "",
                  const string& rule = "");
    //@}

    //! \name Inspectors
    //@{
    Natural spotLag() const { return spotLag_; }
    const string& indexName() const { return strIndex_; }
    const boost::shared_ptr<OvernightIndex>& index() const { return index_; }
    const DayCounter& fixedDayCounter() const { return fixedDayCounter_; }
    Natural paymentLag() const { return paymentLag_; }
    bool eom() { return eom_; }
    Frequency fixedFrequency() const { return fixedFrequency_; }
    BusinessDayConvention fixedConvention() const { return fixedConvention_; }
    BusinessDayConvention fixedPaymentConvention() const { return fixedPaymentConvention_; }
    DateGeneration::Rule rule() const { return rule_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    Natural spotLag_;
    boost::shared_ptr<OvernightIndex> index_;
    DayCounter fixedDayCounter_;
    Natural paymentLag_;
    bool eom_;
    Frequency fixedFrequency_;
    BusinessDayConvention fixedConvention_;
    BusinessDayConvention fixedPaymentConvention_;
    DateGeneration::Rule rule_;

    // Strings to store the inputs
    string strSpotLag_;
    string strIndex_;
    string strFixedDayCounter_;
    string strPaymentLag_;
    string strEom_;
    string strFixedFrequency_;
    string strFixedConvention_;
    string strFixedPaymentConvention_;
    string strRule_;
};

//! Container for storing Swap Index conventions
/*!
  \ingroup marketdata
 */
class SwapIndexConvention : public Convention {
public:
    SwapIndexConvention() {}
    SwapIndexConvention(const string& id, const string& conventions);

    const string& conventions() const { return strConventions_; }

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build(){};

private:
    string strConventions_;
};

//! Container for storing Interest Rate Swap conventions
/*!
  \ingroup marketdata
 */
class IRSwapConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    IRSwapConvention() {}
    //! Detailed constructor
    IRSwapConvention(const string& id, const string& fixedCalendar, const string& fixedFrequency,
                     const string& fixedConvention, const string& fixedDayCounter, const string& index,
                     bool hasSubPeriod = false, const string& floatFrequency = "",
                     const string& subPeriodsCouponType = "");
    //@}

    //! \name Inspectors
    //@{
    const Calendar& fixedCalendar() const { return fixedCalendar_; }
    Frequency fixedFrequency() const { return fixedFrequency_; }
    BusinessDayConvention fixedConvention() const { return fixedConvention_; }
    const DayCounter& fixedDayCounter() const { return fixedDayCounter_; }
    const string& indexName() const { return strIndex_; }
    const boost::shared_ptr<IborIndex>& index() const { return index_; }
    // For sub period
    bool hasSubPeriod() const { return hasSubPeriod_; }
    Frequency floatFrequency() const { return floatFrequency_; } // returns NoFrequency for normal swaps
    QuantExt::SubPeriodsCoupon::Type subPeriodsCouponType() const { return subPeriodsCouponType_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    Calendar fixedCalendar_;
    Frequency fixedFrequency_;
    BusinessDayConvention fixedConvention_;
    DayCounter fixedDayCounter_;
    boost::shared_ptr<IborIndex> index_;
    bool hasSubPeriod_;
    Frequency floatFrequency_;
    QuantExt::SubPeriodsCoupon::Type subPeriodsCouponType_;

    // Strings to store the inputs
    string strFixedCalendar_;
    string strFixedFrequency_;
    string strFixedConvention_;
    string strFixedDayCounter_;
    string strIndex_;
    string strFloatFrequency_;
    string strSubPeriodsCouponType_;
};

//! Container for storing Average OIS conventions
/*!
  \ingroup marketdata
 */
class AverageOisConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    AverageOisConvention() {}
    //! Detailed constructor
    AverageOisConvention(const string& id, const string& spotLag, const string& fixedTenor,
                         const string& fixedDayCounter, const string& fixedCalendar, const string& fixedConvention,
                         const string& fixedPaymentConvention, const string& index, const string& onTenor,
                         const string& rateCutoff);
    //@}

    //! \name Inspectors
    //@{
    Natural spotLag() const { return spotLag_; }
    const Period& fixedTenor() const { return fixedTenor_; }
    const DayCounter& fixedDayCounter() const { return fixedDayCounter_; }
    const Calendar& fixedCalendar() const { return fixedCalendar_; }
    BusinessDayConvention fixedConvention() const { return fixedConvention_; }
    BusinessDayConvention fixedPaymentConvention() const { return fixedPaymentConvention_; }
    const string& indexName() const { return strIndex_; }
    const boost::shared_ptr<OvernightIndex>& index() const { return index_; }
    const Period& onTenor() const { return onTenor_; }
    Natural rateCutoff() const { return rateCutoff_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}
private:
    Natural spotLag_;
    Period fixedTenor_;
    DayCounter fixedDayCounter_;
    Calendar fixedCalendar_;
    BusinessDayConvention fixedConvention_;
    BusinessDayConvention fixedPaymentConvention_;
    boost::shared_ptr<OvernightIndex> index_;
    Period onTenor_;
    Natural rateCutoff_;

    // Strings to store the inputs
    string strSpotLag_;
    string strFixedTenor_;
    string strFixedDayCounter_;
    string strFixedCalendar_;
    string strFixedConvention_;
    string strFixedPaymentConvention_;
    string strIndex_;
    string strOnTenor_;
    string strRateCutoff_;
};

//! Container for storing Tenor Basis Swap conventions
/*!
  \ingroup marketdata
 */
class TenorBasisSwapConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    TenorBasisSwapConvention() {}
    //! Detailed constructor
    TenorBasisSwapConvention(const string& id, const string& longIndex, const string& shortIndex,
                             const string& shortPayTenor = "", const string& spreadOnShort = "",
                             const string& includeSpread = "", const string& subPeriodsCouponType = "");
    //@}

    //! \name Inspectors
    //@{
    const boost::shared_ptr<IborIndex>& longIndex() const { return longIndex_; }
    const boost::shared_ptr<IborIndex>& shortIndex() const { return shortIndex_; }
    const string& longIndexName() const { return strLongIndex_; }
    const string& shortIndexName() const { return strShortIndex_; }
    const Period& shortPayTenor() const { return shortPayTenor_; }
    bool spreadOnShort() const { return spreadOnShort_; }
    bool includeSpread() const { return includeSpread_; }
    QuantExt::SubPeriodsCoupon::Type subPeriodsCouponType() const { return subPeriodsCouponType_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    boost::shared_ptr<IborIndex> longIndex_;
    boost::shared_ptr<IborIndex> shortIndex_;
    Period shortPayTenor_;
    bool spreadOnShort_;
    bool includeSpread_;
    QuantExt::SubPeriodsCoupon::Type subPeriodsCouponType_;

    // Strings to store the inputs
    string strLongIndex_;
    string strShortIndex_;
    string strShortPayTenor_;
    string strSpreadOnShort_;
    string strIncludeSpread_;
    string strSubPeriodsCouponType_;
};

//! Container for storing conventions for Tenor Basis Swaps quoted as a spread of two interest rate swaps
/*!
  \ingroup marketdata
 */
class TenorBasisTwoSwapConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    TenorBasisTwoSwapConvention() {}
    //! Detailed constructor
    TenorBasisTwoSwapConvention(const string& id, const string& calendar, const string& longFixedFrequency,
                                const string& longFixedConvention, const string& longFixedDayCounter,
                                const string& longIndex, const string& shortFixedFrequency,
                                const string& shortFixedConvention, const string& shortFixedDayCounter,
                                const string& shortIndex, const string& longMinusShort = "");
    //@}

    //! \name Inspectors
    //@{
    const Calendar& calendar() const { return calendar_; }
    Frequency longFixedFrequency() const { return longFixedFrequency_; }
    BusinessDayConvention longFixedConvention() const { return longFixedConvention_; }
    const DayCounter& longFixedDayCounter() const { return longFixedDayCounter_; }
    const boost::shared_ptr<IborIndex>& longIndex() const { return longIndex_; }
    Frequency shortFixedFrequency() const { return shortFixedFrequency_; }
    BusinessDayConvention shortFixedConvention() const { return shortFixedConvention_; }
    const DayCounter& shortFixedDayCounter() const { return shortFixedDayCounter_; }
    const boost::shared_ptr<IborIndex>& shortIndex() const { return shortIndex_; }
    bool longMinusShort() const { return longMinusShort_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    Calendar calendar_;
    Frequency longFixedFrequency_;
    BusinessDayConvention longFixedConvention_;
    DayCounter longFixedDayCounter_;
    boost::shared_ptr<IborIndex> longIndex_;
    Frequency shortFixedFrequency_;
    BusinessDayConvention shortFixedConvention_;
    DayCounter shortFixedDayCounter_;
    boost::shared_ptr<IborIndex> shortIndex_;
    bool longMinusShort_;

    // Strings to store the inputs
    string strCalendar_;
    string strLongFixedFrequency_;
    string strLongFixedConvention_;
    string strLongFixedDayCounter_;
    string strLongIndex_;
    string strShortFixedFrequency_;
    string strShortFixedConvention_;
    string strShortFixedDayCounter_;
    string strShortIndex_;
    string strLongMinusShort_;
};

//! Container for storing FX Spot quote conventions
/*!
  \ingroup marketdata
 */
class FXConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    FXConvention() {}
    //! Detailed constructor
    FXConvention(const string& id, const string& spotDays, const string& sourceCurrency, const string& targetCurrency,
                 const string& pointsFactor, const string& advanceCalendar = "", const string& spotRelative = "");
    //@}

    //! \name Inspectors
    //@{
    Natural spotDays() const { return spotDays_; }
    const Currency& sourceCurrency() const { return sourceCurrency_; }
    const Currency& targetCurrency() const { return targetCurrency_; }
    Real pointsFactor() const { return pointsFactor_; }
    const Calendar& advanceCalendar() const { return advanceCalendar_; }
    bool spotRelative() const { return spotRelative_; }
    //@}

    //! \name Serialisation
    //
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    Natural spotDays_;
    Currency sourceCurrency_;
    Currency targetCurrency_;
    Real pointsFactor_;
    Calendar advanceCalendar_;
    bool spotRelative_;

    // Strings to store the inputs
    string strSpotDays_;
    string strSourceCurrency_;
    string strTargetCurrency_;
    string strPointsFactor_;
    string strAdvanceCalendar_;
    string strSpotRelative_;
};

//! Container for storing Cross Currency Basis Swap quote conventions
/*!
  \ingroup marketdata
 */
class CrossCcyBasisSwapConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CrossCcyBasisSwapConvention() {}
    //! Detailed constructor
    CrossCcyBasisSwapConvention(const string& id, const string& strSettlementDays, const string& strSettlementCalendar,
                                const string& strRollConvention, const string& flatIndex, const string& spreadIndex,
                                const string& strEom = "");
    //@}

    //! \name Inspectors
    //@{
    Natural settlementDays() const { return settlementDays_; }
    const Calendar& settlementCalendar() const { return settlementCalendar_; }
    BusinessDayConvention rollConvention() const { return rollConvention_; }
    const boost::shared_ptr<IborIndex>& flatIndex() const { return flatIndex_; }
    const boost::shared_ptr<IborIndex>& spreadIndex() const { return spreadIndex_; }
    const string& flatIndexName() const { return strFlatIndex_; }
    const string& spreadIndexName() const { return strSpreadIndex_; }

    bool eom() const { return eom_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}
private:
    Natural settlementDays_;
    Calendar settlementCalendar_;
    BusinessDayConvention rollConvention_;
    boost::shared_ptr<IborIndex> flatIndex_;
    boost::shared_ptr<IborIndex> spreadIndex_;
    bool eom_;

    // Strings to store the inputs
    string strSettlementDays_;
    string strSettlementCalendar_;
    string strRollConvention_;
    string strFlatIndex_;
    string strSpreadIndex_;
    string strEom_;
};

//! Container for storing Credit Default Swap quote conventions
/*!
  \ingroup marketdata
 */
class CdsConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CdsConvention() {}
    //! Detailed constructor
    CdsConvention(const string& id, const string& strSettlementDays, const string& strCalendar,
                  const string& strFrequency, const string& strPaymentConvention, const string& strRule,
                  const string& dayCounter, const string& settlesAccrual, const string& paysAtDefaultTime);
    //@}

    //! \name Inspectors
    //@{
    Natural settlementDays() const { return settlementDays_; }
    const Calendar& calendar() const { return calendar_; }
    Frequency frequency() const { return frequency_; }
    BusinessDayConvention paymentConvention() const { return paymentConvention_; }
    DateGeneration::Rule rule() const { return rule_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    bool settlesAccrual() const { return settlesAccrual_; }
    bool paysAtDefaultTime() const { return paysAtDefaultTime_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}
private:
    Natural settlementDays_;
    Calendar calendar_;
    Frequency frequency_;
    BusinessDayConvention paymentConvention_;
    DateGeneration::Rule rule_;
    DayCounter dayCounter_;
    bool settlesAccrual_;
    bool paysAtDefaultTime_;

    // Strings to store the inputs
    string strSettlementDays_;
    string strCalendar_;
    string strFrequency_;
    string strPaymentConvention_;
    string strRule_;
    string strDayCounter_;
    string strSettlesAccrual_;
    string strPaysAtDefaultTime_;
};

class InflationSwapConvention : public Convention {
public:
    InflationSwapConvention() {}
    InflationSwapConvention(const string& id, const string& strFixCalendar, const string& strFixConvention,
                            const string& strDayCounter, const string& strIndex, const string& strInterpolated,
                            const string& strObservationLag, const string& strAdjustInfObsDates,
                            const string& strInfCalendar, const string& strInfConvention);

    const Calendar& fixCalendar() const { return fixCalendar_; }
    BusinessDayConvention fixConvention() const { return fixConvention_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const boost::shared_ptr<ZeroInflationIndex> index() const { return index_; }
    const string& indexName() const { return strIndex_; }
    bool interpolated() const { return interpolated_; }
    Period observationLag() const { return observationLag_; }
    bool adjustInfObsDates() const { return adjustInfObsDates_; }
    const Calendar& infCalendar() const { return infCalendar_; }
    BusinessDayConvention infConvention() const { return infConvention_; }

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();

private:
    Calendar fixCalendar_;
    BusinessDayConvention fixConvention_;
    DayCounter dayCounter_;
    boost::shared_ptr<ZeroInflationIndex> index_;
    bool interpolated_;
    Period observationLag_;
    bool adjustInfObsDates_;
    Calendar infCalendar_;
    BusinessDayConvention infConvention_;

    // Strings to store the inputs
    string strFixCalendar_;
    string strFixConvention_;
    string strDayCounter_;
    string strIndex_;
    string strInterpolated_;
    string strObservationLag_;
    string strAdjustInfObsDates_;
    string strInfCalendar_;
    string strInfConvention_;
};

//! Repository for currency dependent market conventions
/*!
  \ingroup market
*/
class Conventions : public XMLSerializable {
public:
    //! Default constructor
    Conventions() {}

    /*! Returns the convention if found and throws if not */
    boost::shared_ptr<Convention> get(const string& id) const;
    /*! Clear all conventions */
    void clear();

    /*! Add a convention. This will overwrite an existing convention
        with the same id */
    void add(const boost::shared_ptr<Convention>& convention);

    //! \name Serilaisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

private:
    map<string, boost::shared_ptr<Convention>> data_;
};

//! Container for storing Bond Spread Rate conventions
/*!
\ingroup marketdata
*/
class SecuritySpreadConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    SecuritySpreadConvention() {}
    SecuritySpreadConvention(const string& id, const string& dayCounter, const string& compounding,
                             const string& compoundingFrequency);
    SecuritySpreadConvention(const string& id, const string& dayCounter, const string& tenorCalendar,
                             const string& compounding = "Continuous", const string& compoundingFrequency = "Annual",
                             const string& spotLag = "", const string& spotCalendar = "",
                             const string& rollConvention = "", const string& eom = "");
    //@}

    //! \name Inspectors
    //@{
    //! Zero rate day counter
    const DayCounter& dayCounter() const { return dayCounter_; }
    //! Return the calendar used for converting tenor points into dates
    const Calendar& tenorCalendar() const { return tenorCalendar_; }
    //! Zero rate compounding
    Compounding compounding() const { return compounding_; }
    //! Zero rate compounding frequency
    Frequency compoundingFrequency() const { return compoundingFrequency_; }
    //! Zero rate spot lag
    Natural spotLag() const { return spotLag_; }
    //! Calendar used for spot date adjustment
    const Calendar& spotCalendar() const { return spotCalendar_; }
    //! Business day convention used in converting tenor points into dates
    BusinessDayConvention rollConvention() const { return rollConvention_; }
    //! End of month adjustment
    bool eom() { return eom_; }
    //! Flag to indicate whether the Zero Rate convention is based on a tenor input
    bool tenorBased() { return tenorBased_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    DayCounter dayCounter_;
    Calendar tenorCalendar_;
    Compounding compounding_;
    Frequency compoundingFrequency_;
    Natural spotLag_;
    Calendar spotCalendar_;
    BusinessDayConvention rollConvention_;
    bool eom_;
    bool tenorBased_;

    // Strings to store the inputs
    string strDayCounter_;
    string strTenorCalendar_;
    string strCompounding_;
    string strCompoundingFrequency_;
    string strSpotLag_;
    string strSpotCalendar_;
    string strRollConvention_;
    string strEom_;
};
} // namespace data
} // namespace ore
