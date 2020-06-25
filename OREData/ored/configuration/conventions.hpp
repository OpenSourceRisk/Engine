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
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <qle/cashflows/subperiodscoupon.hpp> // SubPeriodsCouponType
#include <qle/indexes/bmaindexwrapper.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using std::map;
using std::string;
using namespace QuantLib;

//! Abstract base class for convention objects
/*!
  \ingroup configuration
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
        BMABasisSwap,
        FX,
        CrossCcyBasis,
        CrossCcyFixFloat,
        CDS,
        IborIndex,
        OvernightIndex,
        SwapIndex,
        InflationSwap,
        SecuritySpread,
        CMSSpreadOption,
        CommodityForward,
        CommodityFuture,
        FxOption
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

    //! Checks if we have a convention with the given \p id
    bool has(const std::string& id) const;

    //! Checks if we have a convention with the given \p id and \p type
    bool has(const std::string& id, const Convention::Type& type) const;

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
    FutureConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit FutureConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Index based constructor
    FutureConvention(const string& id, const string& index, const Conventions* conventions = nullptr);
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
    const Conventions* conventions_;
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
    FraConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit FraConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Index based constructor
    FraConvention(const string& id, const string& index, const Conventions* conventions = nullptr);
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
    const Conventions* conventions_;
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
    OisConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit OisConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Detailed constructor
    OisConvention(const string& id, const string& spotLag, const string& index, const string& fixedDayCounter,
                  const string& paymentLag = "", const string& eom = "", const string& fixedFrequency = "",
                  const string& fixedConvention = "", const string& fixedPaymentConvention = "",
                  const string& rule = "", const std::string& paymentCalendar = "",
                  const Conventions* conventions = nullptr);
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
    QuantLib::Calendar paymentCalendar() const { return paymentCal_; }
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
    QuantLib::Calendar paymentCal_;

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
    std::string strPaymentCal_;

    const Conventions* conventions_;
};

//! Container for storing Ibor Index conventions
/*!
  \ingroup marketdata
 */
class IborIndexConvention : public Convention {
public:
    IborIndexConvention() {}
    IborIndexConvention(const string& id, const string& fixingCalendar, const string& dayCounter,
                        const Size settlementDays, const string& businessDayConvention, const bool endOfMonth);

    const string& id() const { return id_; }
    const string& fixingCalendar() const { return strFixingCalendar_; }
    const string& dayCounter() const { return strDayCounter_; }
    const Size settlementDays() const { return settlementDays_; }
    const string& businessDayConvention() const { return strBusinessDayConvention_; }
    const bool endOfMonth() const { return endOfMonth_; }

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();

private:
    string strFixingCalendar_;
    string strDayCounter_;
    Size settlementDays_;
    string strBusinessDayConvention_;
    bool endOfMonth_;
};

//! Container for storing Overnight Index conventions
/*!
  \ingroup marketdata
 */
class OvernightIndexConvention : public Convention {
public:
    OvernightIndexConvention() {}
    OvernightIndexConvention(const string& id, const string& fixingCalendar, const string& dayCounter,
                             const Size settlementDays);

    const string& id() const { return id_; }
    const string& fixingCalendar() const { return strFixingCalendar_; }
    const string& dayCounter() const { return strDayCounter_; }
    const Size settlementDays() const { return settlementDays_; }

    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();

private:
    string strFixingCalendar_;
    string strDayCounter_;
    Size settlementDays_;
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
    IRSwapConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit IRSwapConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Detailed constructor
    IRSwapConvention(const string& id, const string& fixedCalendar, const string& fixedFrequency,
                     const string& fixedConvention, const string& fixedDayCounter, const string& index,
                     bool hasSubPeriod = false, const string& floatFrequency = "",
                     const string& subPeriodsCouponType = "", const Conventions* conventions = nullptr);
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

    const Conventions* conventions_;
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
    AverageOisConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit AverageOisConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Detailed constructor
    AverageOisConvention(const string& id, const string& spotLag, const string& fixedTenor,
                         const string& fixedDayCounter, const string& fixedCalendar, const string& fixedConvention,
                         const string& fixedPaymentConvention, const string& index, const string& onTenor,
                         const string& rateCutoff, const Conventions* conventions = nullptr);
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

    const Conventions* conventions_;
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
    TenorBasisSwapConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit TenorBasisSwapConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Detailed constructor
    TenorBasisSwapConvention(const string& id, const string& longIndex, const string& shortIndex,
                             const string& shortPayTenor = "", const string& spreadOnShort = "",
                             const string& includeSpread = "", const string& subPeriodsCouponType = "",
                             const Conventions* conventions = nullptr);
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

    const Conventions* conventions_;
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
    TenorBasisTwoSwapConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit TenorBasisTwoSwapConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Detailed constructor
    TenorBasisTwoSwapConvention(const string& id, const string& calendar, const string& longFixedFrequency,
                                const string& longFixedConvention, const string& longFixedDayCounter,
                                const string& longIndex, const string& shortFixedFrequency,
                                const string& shortFixedConvention, const string& shortFixedDayCounter,
                                const string& shortIndex, const string& longMinusShort = "",
                                const Conventions* conventions = nullptr);
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

    const Conventions* conventions_;
};

//! Container for storing Libor-BMA Basis Swap conventions
/*!
\ingroup marketdata
*/
class BMABasisSwapConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    BMABasisSwapConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit BMABasisSwapConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Detailed constructor
    BMABasisSwapConvention(const string& id, const string& liborIndex, const string& bmaIndex,
                           const Conventions* conventions = nullptr);
    //@}

    //! \name Inspectors
    //@{
    const boost::shared_ptr<IborIndex>& liborIndex() const { return liborIndex_; }
    const boost::shared_ptr<QuantExt::BMAIndexWrapper>& bmaIndex() const { return bmaIndex_; }
    const string& liborIndexName() const { return strLiborIndex_; }
    const string& bmaIndexName() const { return strBmaIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    boost::shared_ptr<IborIndex> liborIndex_;
    boost::shared_ptr<QuantExt::BMAIndexWrapper> bmaIndex_;

    // Strings to store the inputs
    string strLiborIndex_;
    string strBmaIndex_;

    const Conventions* conventions_;
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
    CrossCcyBasisSwapConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit CrossCcyBasisSwapConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Detailed constructor
    CrossCcyBasisSwapConvention(const string& id, const string& strSettlementDays, const string& strSettlementCalendar,
                                const string& strRollConvention, const string& flatIndex, const string& spreadIndex,
                                const string& strEom = "", const string& strIsResettable = "",
                                const string& strFlatIndexIsResettable = "", const std::string& strFlatTenor = "",
                                const std::string& strSpreadTenor = "", const Conventions* conventions = nullptr);
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
    bool isResettable() const { return isResettable_; }
    bool flatIndexIsResettable() const { return flatIndexIsResettable_; }
    const QuantLib::Period& flatTenor() const { return flatTenor_; }
    const QuantLib::Period& spreadTenor() const { return spreadTenor_; }
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
    bool isResettable_;
    bool flatIndexIsResettable_;
    QuantLib::Period flatTenor_;
    QuantLib::Period spreadTenor_;

    // Strings to store the inputs
    string strSettlementDays_;
    string strSettlementCalendar_;
    string strRollConvention_;
    string strFlatIndex_;
    string strSpreadIndex_;
    string strEom_;
    string strIsResettable_;
    string strFlatIndexIsResettable_;
    std::string strFlatTenor_;
    std::string strSpreadTenor_;

    const Conventions* conventions_;
};

/*! Container for storing Cross Currency Fix vs Float Swap quote conventions
    \ingroup marketdata
 */
class CrossCcyFixFloatSwapConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CrossCcyFixFloatSwapConvention() : conventions_(nullptr) {}
    //! Constructor taking conventions only
    explicit CrossCcyFixFloatSwapConvention(const Conventions* conventions) : conventions_(conventions) {}
    //! Detailed constructor
    CrossCcyFixFloatSwapConvention(const std::string& id, const std::string& settlementDays,
                                   const std::string& settlementCalendar, const std::string& settlementConvention,
                                   const std::string& fixedCurrency, const std::string& fixedFrequency,
                                   const std::string& fixedConvention, const std::string& fixedDayCounter,
                                   const std::string& index, const std::string& eom = "",
                                   const Conventions* conventions = nullptr);
    //@}

    //! \name Inspectors
    //@{
    QuantLib::Natural settlementDays() const { return settlementDays_; }
    const QuantLib::Calendar& settlementCalendar() const { return settlementCalendar_; }
    QuantLib::BusinessDayConvention settlementConvention() const { return settlementConvention_; }
    const QuantLib::Currency& fixedCurrency() const { return fixedCurrency_; }
    QuantLib::Frequency fixedFrequency() const { return fixedFrequency_; }
    QuantLib::BusinessDayConvention fixedConvention() const { return fixedConvention_; }
    const QuantLib::DayCounter& fixedDayCounter() const { return fixedDayCounter_; }
    const boost::shared_ptr<QuantLib::IborIndex>& index() const { return index_; }
    bool eom() const { return eom_; }
    //@}

    //! \name Serialisation interface
    //@{
    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Convention interface
    //@{
    void build();
    //@}

private:
    QuantLib::Natural settlementDays_;
    QuantLib::Calendar settlementCalendar_;
    QuantLib::BusinessDayConvention settlementConvention_;
    QuantLib::Currency fixedCurrency_;
    QuantLib::Frequency fixedFrequency_;
    QuantLib::BusinessDayConvention fixedConvention_;
    QuantLib::DayCounter fixedDayCounter_;
    boost::shared_ptr<QuantLib::IborIndex> index_;
    bool eom_;

    // Strings to store the inputs
    std::string strSettlementDays_;
    std::string strSettlementCalendar_;
    std::string strSettlementConvention_;
    std::string strFixedCurrency_;
    std::string strFixedFrequency_;
    std::string strFixedConvention_;
    std::string strFixedDayCounter_;
    std::string strIndex_;
    std::string strEom_;

    const Conventions* conventions_;
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

//! Container for storing CMS Spread Option conventions
/*!
  \ingroup marketdata
 */
class CmsSpreadOptionConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CmsSpreadOptionConvention() {}
    //! Detailed constructor
    CmsSpreadOptionConvention(const string& id, const string& strForwardStart, const string& strSpotDays,
                              const string& strSwapTenor, const string& strFixingDays, const string& strCalendar,
                              const string& strDayCounter, const string& strConvention);
    //@}

    //! \name Inspectors
    //@{
    const Period& forwardStart() const { return forwardStart_; }
    const Period spotDays() const { return spotDays_; }
    const Period& swapTenor() { return swapTenor_; }
    Natural fixingDays() const { return fixingDays_; }
    const Calendar& calendar() const { return calendar_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    BusinessDayConvention rollConvention() const { return rollConvention_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}
private:
    Period forwardStart_;
    Period spotDays_;
    Period swapTenor_;
    Natural fixingDays_;
    Calendar calendar_;
    DayCounter dayCounter_;
    BusinessDayConvention rollConvention_;

    // Strings to store the inputs
    string strForwardStart_;
    string strSpotDays_;
    string strSwapTenor_;
    string strFixingDays_;
    string strCalendar_;
    string strDayCounter_;
    string strRollConvention_;
};

/*! Container for storing Commodity forward quote conventions
    \ingroup marketdata
 */
class CommodityForwardConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CommodityForwardConvention() {}

    //! Detailed constructor
    CommodityForwardConvention(const string& id, const string& spotDays = "", const string& pointsFactor = "",
                               const string& advanceCalendar = "", const string& spotRelative = "",
                               BusinessDayConvention bdc = Following, bool outright = true);
    //@}

    //! \name Inspectors
    //@{
    Natural spotDays() const { return spotDays_; }
    Real pointsFactor() const { return pointsFactor_; }
    const Calendar& advanceCalendar() const { return advanceCalendar_; }
    bool spotRelative() const { return spotRelative_; }
    BusinessDayConvention bdc() const { return bdc_; }
    bool outright() const { return outright_; }
    //@}

    //! \name Serialisation
    //
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}

private:
    Natural spotDays_;
    Real pointsFactor_;
    Calendar advanceCalendar_;
    bool spotRelative_;
    BusinessDayConvention bdc_;
    bool outright_;

    // Strings to store the inputs
    string strSpotDays_;
    string strPointsFactor_;
    string strAdvanceCalendar_;
    string strSpotRelative_;
};

/*! Container for storing commodity future conventions
    \ingroup marketdata
 */
class CommodityFutureConvention : public Convention {
public:
    /*! The anchor day type of commodity future convention
     */
    enum class AnchorType { DayOfMonth, NthWeekday, CalendarDaysBefore };

    //! Classes to differentiate constructors below
    //@{
    struct DayOfMonth {
        DayOfMonth(const std::string& dayOfMonth) : dayOfMonth_(dayOfMonth) {}
        std::string dayOfMonth_;
    };

    struct CalendarDaysBefore {
        CalendarDaysBefore(const std::string& calendarDaysBefore) : calendarDaysBefore_(calendarDaysBefore) {}
        std::string calendarDaysBefore_;
    };
    //@}

    //! \name Constructors
    //@{
    //! Default constructor
    CommodityFutureConvention() {}

    //! Day of month based constructor
    CommodityFutureConvention(const std::string& id, const DayOfMonth& dayOfMonth, const std::string& contractFrequency,
                              const std::string& calendar, const std::string& expiryCalendar = "",
                              QuantLib::Natural expiryMonthLag = 0, const std::string& oneContractMonth = "",
                              const std::string& offsetDays = "", const std::string& bdc = "",
                              bool adjustBeforeOffset = true, bool isAveraging = false,
                              const std::string& optionExpiryOffset = "",
                              const std::vector<std::string>& prohibitedExpiries = {});

    //! N-th weekday based constructor
    CommodityFutureConvention(const std::string& id, const std::string& nth, const std::string& weekday,
                              const std::string& contractFrequency, const std::string& calendar,
                              const std::string& expiryCalendar = "", QuantLib::Natural expiryMonthLag = 0,
                              const std::string& oneContractMonth = "", const std::string& offsetDays = "",
                              const std::string& bdc = "", bool adjustBeforeOffset = true, bool isAveraging = false,
                              const std::string& optionExpiryOffset = "",
                              const std::vector<std::string>& prohibitedExpiries = {});

    //! Calendar days before based constructor
    CommodityFutureConvention(const std::string& id, const CalendarDaysBefore& calendarDaysBefore,
                              const std::string& contractFrequency, const std::string& calendar,
                              const std::string& expiryCalendar = "", QuantLib::Natural expiryMonthLag = 0,
                              const std::string& oneContractMonth = "", const std::string& offsetDays = "",
                              const std::string& bdc = "", bool adjustBeforeOffset = true, bool isAveraging = false,
                              const std::string& optionExpiryOffset = "",
                              const std::vector<std::string>& prohibitedExpiries = {});
    //@}

    //! \name Inspectors
    //@{
    AnchorType anchorType() const { return anchorType_; }
    QuantLib::Natural dayOfMonth() const { return dayOfMonth_; }
    QuantLib::Natural nth() const { return nth_; }
    QuantLib::Weekday weekday() const { return weekday_; }
    QuantLib::Natural calendarDaysBefore() const { return calendarDaysBefore_; }
    QuantLib::Frequency contractFrequency() const { return contractFrequency_; }
    const QuantLib::Calendar& calendar() const { return calendar_; }
    const QuantLib::Calendar& expiryCalendar() const { return expiryCalendar_; }
    QuantLib::Natural expiryMonthLag() const { return expiryMonthLag_; }
    QuantLib::Month oneContractMonth() const { return oneContractMonth_; }
    QuantLib::Integer offsetDays() const { return offsetDays_; }
    QuantLib::BusinessDayConvention businessDayConvention() const { return bdc_; }
    bool adjustBeforeOffset() const { return adjustBeforeOffset_; }
    bool isAveraging() const { return isAveraging_; }
    QuantLib::Natural optionExpiryOffset() const { return optionExpiryOffset_; }
    const std::set<QuantLib::Date>& prohibitedExpiries() const { return prohibitedExpiries_; }
    //@}

    //! Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! Implementation
    void build() override;

private:
    AnchorType anchorType_;
    QuantLib::Natural dayOfMonth_;
    QuantLib::Natural nth_;
    QuantLib::Weekday weekday_;
    QuantLib::Natural calendarDaysBefore_;
    QuantLib::Frequency contractFrequency_;
    QuantLib::Calendar calendar_;
    QuantLib::Calendar expiryCalendar_;
    QuantLib::Month oneContractMonth_;
    QuantLib::Integer offsetDays_;
    QuantLib::BusinessDayConvention bdc_;
    QuantLib::Natural optionExpiryOffset_;
    std::set<QuantLib::Date> prohibitedExpiries_;

    std::string strDayOfMonth_;
    std::string strNth_;
    std::string strWeekday_;
    std::string strCalendarDaysBefore_;
    std::string strContractFrequency_;
    std::string strCalendar_;
    std::string strExpiryCalendar_;
    QuantLib::Natural expiryMonthLag_;
    std::string strOneContractMonth_;
    std::string strOffsetDays_;
    std::string strBdc_;
    bool adjustBeforeOffset_;
    bool isAveraging_;
    std::string strOptionExpiryOffset_;
    std::vector<std::string> strProhibitedExpiries_;
};

//! Container for storing FX Option conventions
/*!
\ingroup marketdata
*/
class FxOptionConvention : public Convention {
public:
    //! \name Constructors
    //@{
    FxOptionConvention() {}
    FxOptionConvention(const string& id, const string& atmType, const string& deltaType);
    //@}

    //! \name Inspectors
    //@{
    const DeltaVolQuote::AtmType& atmType() const { return atmType_; }
    const DeltaVolQuote::DeltaType& deltaType() const { return deltaType_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    virtual void build();
    //@}
private:
    DeltaVolQuote::AtmType atmType_;
    DeltaVolQuote::DeltaType deltaType_;

    // Strings to store the inputs
    string strAtmType_;
    string strDeltaType_;
};

} // namespace data
} // namespace ore
