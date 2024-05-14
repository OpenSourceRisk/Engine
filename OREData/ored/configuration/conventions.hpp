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
#include <ored/portfolio/schedule.hpp>
#include <ql/experimental/fx/deltavolquote.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/instruments/overnightindexfuture.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/option.hpp>
#include <qle/cashflows/subperiodscoupon.hpp> // SubPeriodsCouponType
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/indexes/commodityindex.hpp>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using std::map;
using std::string;
using QuantExt::SubPeriodsCoupon1;
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
        ZeroInflationIndex,
        InflationSwap,
        SecuritySpread,
        CMSSpreadOption,
        CommodityForward,
        CommodityFuture,
        FxOption,
	BondYield
    };

    //! Default destructor
    virtual ~Convention() {}

    //! \name Inspectors
    //@{
    const string& id() const { return id_; }
    Type type() const { return type_; }
    //@}

    //! \name convention interface definition
    //@{
    virtual void build() = 0;
    //@}

protected:
    Convention() {}
    Convention(const string& id, Type type);
    Type type_;
    string id_;
};

std::ostream& operator<<(std::ostream& out, Convention::Type type);

//! Repository for currency dependent market conventions
/*!
  \ingroup market
*/
class Conventions : public XMLSerializable {
public:
    //! Default constructor
    Conventions() {}

    /*! Returns the convention if found and throws if not */
    QuantLib::ext::shared_ptr<Convention> get(const string& id) const;

    /*! Get a convention with the given \p id and \p type. If no convention of the given \p type with the given \p id
        is found, the first element of the returned pair is \c false and the second element is a \c nullptr. If a
        convention is found, the first element of the returned pair is \c true and the second element holds the
        convention.
    */
    std::pair<bool, QuantLib::ext::shared_ptr<Convention>> get(const std::string& id, const Convention::Type& type) const;

    /*! Get all conventions of a given type */
    std::set<QuantLib::ext::shared_ptr<Convention>> get(const Convention::Type& type) const;
    
    /*! Find a convention for an FX pair */
    QuantLib::ext::shared_ptr<Convention> getFxConvention(const string& ccy1, const string& ccy2) const;

    //! Checks if we have a convention with the given \p id
    bool has(const std::string& id) const;

    //! Checks if we have a convention with the given \p id and \p type
    bool has(const std::string& id, const Convention::Type& type) const;

    /*! Clear all conventions */
    void clear() const;

    /*! Add a convention. This will overwrite an existing convention
        with the same id */
    void add(const QuantLib::ext::shared_ptr<Convention>& convention) const;

    //! \name Serialisation
    //@{0
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    mutable map<string, QuantLib::ext::shared_ptr<Convention>> data_;
    mutable map<string, std::pair<string, string>> unparsed_;
    mutable std::set<string> used_;
    mutable boost::shared_mutex mutex_;
};

//! Singleton to hold conventions
//
class InstrumentConventions : public QuantLib::Singleton<InstrumentConventions, std::integral_constant<bool, true>> {
    friend class QuantLib::Singleton<InstrumentConventions, std::integral_constant<bool, true>>;

private:
    InstrumentConventions() { conventions_[Date()] = QuantLib::ext::make_shared<ore::data::Conventions>(); }

    mutable std::map<QuantLib::Date, QuantLib::ext::shared_ptr<ore::data::Conventions>> conventions_;
    mutable boost::shared_mutex mutex_;
    mutable std::size_t numberOfEmittedWarnings_ = 0;

public:
    const QuantLib::ext::shared_ptr<ore::data::Conventions>& conventions(QuantLib::Date d = QuantLib::Date()) const;
    void setConventions(const QuantLib::ext::shared_ptr<ore::data::Conventions>& conventions,
                        QuantLib::Date d = QuantLib::Date());
    void clear() { conventions_.clear(); }
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
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
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
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
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
    enum class DateGenerationRule { IMM, FirstDayOfMonth };
    //! \name Constructors
    //@{
    //! Default constructor
    FutureConvention() {}
    //! Index based constructor
    FutureConvention(const string& id, const string& index);
    //! Index based constructor taking in addition a netting type for ON indices and a date generation rule
    FutureConvention(const string& id, const string& index,
                     const QuantLib::RateAveraging::Type overnightIndexFutureNettingType,
                     const DateGenerationRule dateGeneration);
    //@}
    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<IborIndex> index() const;
    QuantLib::RateAveraging::Type overnightIndexFutureNettingType() const { return overnightIndexFutureNettingType_; }
    DateGenerationRule dateGenerationRule() const { return dateGenerationRule_; }
    //@}

    //! Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override {}
    //@}

private:
    string strIndex_;
    QuantLib::RateAveraging::Type overnightIndexFutureNettingType_;
    DateGenerationRule dateGenerationRule_;
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
    QuantLib::ext::shared_ptr<IborIndex> index() const;
    const string& indexName() const { return strIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override {}
    //@}

private:
    string strIndex_;
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
                  const string& fixedCalendar, const string& paymentLag = "", const string& eom = "",
                  const string& fixedFrequency = "", const string& fixedConvention = "",
                  const string& fixedPaymentConvention = "", const string& rule = "",
                  const std::string& paymentCalendar = "");
    //@}

    //! \name Inspectors
    //@{
    Natural spotLag() const { return spotLag_; }
    const string& indexName() const { return strIndex_; }
    QuantLib::ext::shared_ptr<OvernightIndex> index() const;
    const DayCounter& fixedDayCounter() const { return fixedDayCounter_; }
    // might be empty to retain bwd compatibility
    const Calendar& fixedCalendar() const { return fixedCalendar_; }
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
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}

private:
    Natural spotLag_;
    DayCounter fixedDayCounter_;
    Calendar fixedCalendar_;
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
    string strFixedCalendar_;
    string strPaymentLag_;
    string strEom_;
    string strFixedFrequency_;
    string strFixedConvention_;
    string strFixedPaymentConvention_;
    string strRule_;
    std::string strPaymentCal_;
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

    const string& fixingCalendar() const { return strFixingCalendar_; }
    const string& dayCounter() const { return strDayCounter_; }
    const Size settlementDays() const { return settlementDays_; }
    const string& businessDayConvention() const { return strBusinessDayConvention_; }
    const bool endOfMonth() const { return endOfMonth_; }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;

private:
    string localId_;
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

    const string& fixingCalendar() const { return strFixingCalendar_; }
    const string& dayCounter() const { return strDayCounter_; }
    const Size settlementDays() const { return settlementDays_; }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;

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
    SwapIndexConvention(const string& id, const string& conventions, const string& fixingCalendar = "");

    const string& conventions() const { return strConventions_; }
    const string& fixingCalendar() const { return fixingCalendar_; }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override {};

private:
    string strConventions_;
    string fixingCalendar_;
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
    QuantLib::ext::shared_ptr<IborIndex> index() const;
    // For sub period
    bool hasSubPeriod() const { return hasSubPeriod_; }
    Frequency floatFrequency() const { return floatFrequency_; } // returns NoFrequency for normal swaps
    SubPeriodsCoupon1::Type subPeriodsCouponType() const { return subPeriodsCouponType_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}

private:
    Calendar fixedCalendar_;
    Frequency fixedFrequency_;
    BusinessDayConvention fixedConvention_;
    DayCounter fixedDayCounter_;
    bool hasSubPeriod_;
    Frequency floatFrequency_;
    SubPeriodsCoupon1::Type subPeriodsCouponType_;

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
                         const string& fixedPaymentConvention, const string& fixedFrequency, const string& index,
                         const string& onTenor, const string& rateCutoff);
    //@}

    //! \name Inspectors
    //@{
    Natural spotLag() const { return spotLag_; }
    const Period& fixedTenor() const { return fixedTenor_; }
    const DayCounter& fixedDayCounter() const { return fixedDayCounter_; }
    const Calendar& fixedCalendar() const { return fixedCalendar_; }
    BusinessDayConvention fixedConvention() const { return fixedConvention_; }
    BusinessDayConvention fixedPaymentConvention() const { return fixedPaymentConvention_; }
    Frequency fixedFrequency() const { return fixedFrequency_; }
    const string& indexName() const { return strIndex_; }
    QuantLib::ext::shared_ptr<OvernightIndex> index() const;
    const Period& onTenor() const { return onTenor_; }
    Natural rateCutoff() const { return rateCutoff_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}
private:
    Natural spotLag_;
    Period fixedTenor_;
    DayCounter fixedDayCounter_;
    Calendar fixedCalendar_;
    BusinessDayConvention fixedConvention_;
    BusinessDayConvention fixedPaymentConvention_;
    Frequency fixedFrequency_;
    Period onTenor_;
    Natural rateCutoff_;

    // Strings to store the inputs
    string strSpotLag_;
    string strFixedTenor_;
    string strFixedDayCounter_;
    string strFixedCalendar_;
    string strFixedConvention_;
    string strFixedPaymentConvention_;
    string strFixedFrequency_;
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
    TenorBasisSwapConvention(const string& id, const string& payIndex, const string& receiveIndex,
                             const string& receiveFrequency = "", const string& payFrequency = "",
                             const string& spreadOnRec = "", const string& includeSpread = "", 
                             const string& subPeriodsCouponType = "");
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<IborIndex> payIndex() const;
    QuantLib::ext::shared_ptr<IborIndex> receiveIndex() const;
    const string& payIndexName() const { return strPayIndex_; }
    const string& receiveIndexName() const { return strReceiveIndex_; }
    const Period& receiveFrequency() const { return receiveFrequency_; }
    const Period& payFrequency() const { return payFrequency_; }
    bool spreadOnRec() const { return spreadOnRec_; }
    bool includeSpread() const { return includeSpread_; }
    SubPeriodsCoupon1::Type subPeriodsCouponType() const { return subPeriodsCouponType_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}

private:
    Period receiveFrequency_;
    Period payFrequency_;
    bool spreadOnRec_;
    bool includeSpread_;
    SubPeriodsCoupon1::Type subPeriodsCouponType_;

    // Strings to store the inputs
    string strPayIndex_;
    string strReceiveIndex_;
    string strReceiveFrequency_;
    string strPayFrequency_;
    string strSpreadOnRec_;
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
    QuantLib::ext::shared_ptr<IborIndex> longIndex() const;
    Frequency shortFixedFrequency() const { return shortFixedFrequency_; }
    BusinessDayConvention shortFixedConvention() const { return shortFixedConvention_; }
    const DayCounter& shortFixedDayCounter() const { return shortFixedDayCounter_; }
    QuantLib::ext::shared_ptr<IborIndex> shortIndex() const;
    bool longMinusShort() const { return longMinusShort_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}

private:
    Calendar calendar_;
    Frequency longFixedFrequency_;
    BusinessDayConvention longFixedConvention_;
    DayCounter longFixedDayCounter_;
    Frequency shortFixedFrequency_;
    BusinessDayConvention shortFixedConvention_;
    DayCounter shortFixedDayCounter_;
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

//! Container for storing Libor-BMA Basis Swap conventions
/*!
\ingroup marketdata
*/
class BMABasisSwapConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    BMABasisSwapConvention() {}
    //! Detailed constructor
    BMABasisSwapConvention(const string& id, const string& liborIndex, const string& bmaIndex);
    //@}

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<IborIndex> liborIndex() const;
    QuantLib::ext::shared_ptr<QuantExt::BMAIndexWrapper> bmaIndex() const;
    const string& liborIndexName() const { return strLiborIndex_; }
    const string& bmaIndexName() const { return strBmaIndex_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}

private:
    // Strings to store the inputs
    string strLiborIndex_;
    string strBmaIndex_;
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
                 const string& pointsFactor, const string& advanceCalendar = "", const string& spotRelative = "",
                 const string& endOfMonth = "", const string& convention = "");
    //@}

    //! \name Inspectors
    //@{
    Natural spotDays() const { return spotDays_; }
    const Currency& sourceCurrency() const { return sourceCurrency_; }
    const Currency& targetCurrency() const { return targetCurrency_; }
    Real pointsFactor() const { return pointsFactor_; }
    const Calendar& advanceCalendar() const { return advanceCalendar_; }
    bool spotRelative() const { return spotRelative_; }
    bool endOfMonth() const { return endOfMonth_; }
    BusinessDayConvention convention() const { return convention_; }
    //@}

    //! \name Serialisation
    //
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}

private:
    Natural spotDays_;
    Currency sourceCurrency_;
    Currency targetCurrency_;
    Real pointsFactor_;
    Calendar advanceCalendar_;
    bool spotRelative_;
    bool endOfMonth_;
    BusinessDayConvention convention_;

    // Strings to store the inputs
    string strSpotDays_;
    string strSourceCurrency_;
    string strTargetCurrency_;
    string strPointsFactor_;
    string strAdvanceCalendar_;
    string strSpotRelative_;
    string strEndOfMonth_;
    string strConvention_;
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
                                const string& strEom = "", const string& strIsResettable = "",
                                const string& strFlatIndexIsResettable = "", const std::string& strFlatTenor = "",
                                const std::string& strSpreadTenor = "", const string& strPaymentLag = "",
                                const string& strFlatPaymentLag = "", const string& strIncludeSpread = "",
                                const string& strLookback = "", const string& strFixingDays = "",
                                const string& strRateCutoff = "", const string& strIsAveraged = "",
                                const string& strFlatIncludeSpread = "", const string& strFlatLookback = "",
                                const string& strFlatFixingDays = "", const string& strFlatRateCutoff = "",
                                const string& strFlatIsAveraged = "", const Conventions* conventions = nullptr);
    //@}

    //! \name Inspectors
    //@{
    Natural settlementDays() const { return settlementDays_; }
    const Calendar& settlementCalendar() const { return settlementCalendar_; }
    BusinessDayConvention rollConvention() const { return rollConvention_; }
    QuantLib::ext::shared_ptr<IborIndex> flatIndex() const;
    QuantLib::ext::shared_ptr<IborIndex> spreadIndex() const;
    const string& flatIndexName() const { return strFlatIndex_; }
    const string& spreadIndexName() const { return strSpreadIndex_; }

    bool eom() const { return eom_; }
    bool isResettable() const { return isResettable_; }
    bool flatIndexIsResettable() const { return flatIndexIsResettable_; }
    const QuantLib::Period& flatTenor() const { return flatTenor_; }
    const QuantLib::Period& spreadTenor() const { return spreadTenor_; }

    Size paymentLag() const { return paymentLag_; }
    Size flatPaymentLag() const { return flatPaymentLag_; }

    // only OIS
    boost::optional<bool> includeSpread() const { return includeSpread_; }
    boost::optional<QuantLib::Period> lookback() const { return lookback_; }
    boost::optional<QuantLib::Size> fixingDays() const { return fixingDays_; }
    boost::optional<Size> rateCutoff() const { return rateCutoff_; }
    boost::optional<bool> isAveraged() const { return isAveraged_; }
    boost::optional<bool> flatIncludeSpread() const { return flatIncludeSpread_; }
    boost::optional<QuantLib::Period> flatLookback() const { return flatLookback_; }
    boost::optional<QuantLib::Size> flatFixingDays() const { return flatFixingDays_; }
    boost::optional<Size> flatRateCutoff() const { return flatRateCutoff_; }
    boost::optional<bool> flatIsAveraged() const { return flatIsAveraged_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}
private:
    Natural settlementDays_;
    Calendar settlementCalendar_;
    BusinessDayConvention rollConvention_;
    bool eom_;
    bool isResettable_;
    bool flatIndexIsResettable_;
    QuantLib::Period flatTenor_;
    QuantLib::Period spreadTenor_;
    QuantLib::Size paymentLag_;
    QuantLib::Size flatPaymentLag_;
    // OIS only
    boost::optional<bool> includeSpread_;
    boost::optional<QuantLib::Period> lookback_;
    boost::optional<QuantLib::Size> fixingDays_;
    boost::optional<Size> rateCutoff_;
    boost::optional<bool> isAveraged_;
    boost::optional<bool> flatIncludeSpread_;
    boost::optional<QuantLib::Period> flatLookback_;
    boost::optional<QuantLib::Size> flatFixingDays_;
    boost::optional<Size> flatRateCutoff_;
    boost::optional<bool> flatIsAveraged_;

    // Strings to store the inputs
    string strSettlementDays_;
    string strSettlementCalendar_;
    string strRollConvention_;
    string strFlatIndex_;
    string strSpreadIndex_;
    string strEom_;
    string strIsResettable_;
    string strFlatIndexIsResettable_;
    string strFlatTenor_;
    string strSpreadTenor_;
    string strPaymentLag_;
    string strFlatPaymentLag_;
    // OIS only
    string strIncludeSpread_;
    string strLookback_;
    string strFixingDays_;
    string strRateCutoff_;
    string strIsAveraged_;
    string strFlatIncludeSpread_;
    string strFlatLookback_;
    string strFlatFixingDays_;
    string strFlatRateCutoff_;
    string strFlatIsAveraged_;
};

/*! Container for storing Cross Currency Fix vs Float Swap quote conventions
    \ingroup marketdata
 */
class CrossCcyFixFloatSwapConvention : public Convention {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CrossCcyFixFloatSwapConvention() {}
    //! Detailed constructor
    CrossCcyFixFloatSwapConvention(const std::string& id, const std::string& settlementDays,
                                   const std::string& settlementCalendar, const std::string& settlementConvention,
                                   const std::string& fixedCurrency, const std::string& fixedFrequency,
                                   const std::string& fixedConvention, const std::string& fixedDayCounter,
                                   const std::string& index, const std::string& eom = "",
                                   const std::string& strIsResettable = "",
                                   const std::string& strFloatIndexIsResettable = "");
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
    QuantLib::ext::shared_ptr<QuantLib::IborIndex> index() const;
    bool eom() const { return eom_; }
    bool isResettable() const { return isResettable_; }
    bool floatIndexIsResettable() const { return floatIndexIsResettable_; }
    //@}

    //! \name Serialisation interface
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Convention interface
    //@{
    void build() override;
    //@}

private:
    QuantLib::Natural settlementDays_;
    QuantLib::Calendar settlementCalendar_;
    QuantLib::BusinessDayConvention settlementConvention_;
    QuantLib::Currency fixedCurrency_;
    QuantLib::Frequency fixedFrequency_;
    QuantLib::BusinessDayConvention fixedConvention_;
    QuantLib::DayCounter fixedDayCounter_;
    bool eom_;
    bool isResettable_;
    bool floatIndexIsResettable_;

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

    std::string strIsResettable_;
    std::string strFloatIndexIsResettable_;
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
    CdsConvention();

    //! Detailed constructor
    CdsConvention(const string& id, const string& strSettlementDays, const string& strCalendar,
                  const string& strFrequency, const string& strPaymentConvention, const string& strRule,
                  const string& dayCounter, const string& settlesAccrual, const string& paysAtDefaultTime,
                  const string& strUpfrontSettlementDays = "", const string& lastPeriodDayCounter = "");
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
    Natural upfrontSettlementDays() const { return upfrontSettlementDays_; }
    const DayCounter& lastPeriodDayCounter() const { return lastPeriodDayCounter_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node0) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
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
    Natural upfrontSettlementDays_;
    DayCounter lastPeriodDayCounter_;

    // Strings to store the inputs
    string strSettlementDays_;
    string strCalendar_;
    string strFrequency_;
    string strPaymentConvention_;
    string strRule_;
    string strDayCounter_;
    string strSettlesAccrual_;
    string strPaysAtDefaultTime_;
    string strUpfrontSettlementDays_;
    string strLastPeriodDayCounter_;
};

class InflationSwapConvention : public Convention {
public:
    //! Rule for determining when inflation swaps roll to observing latest inflation index release.
    enum class PublicationRoll {
        None,
        OnPublicationDate,
        AfterPublicationDate
    };

    InflationSwapConvention();

    InflationSwapConvention(const string& id, const string& strFixCalendar, const string& strFixConvention,
                            const string& strDayCounter, const string& strIndex, const string& strInterpolated,
                            const string& strObservationLag, const string& strAdjustInfObsDates,
                            const string& strInfCalendar, const string& strInfConvention,
                            PublicationRoll publicationRoll = PublicationRoll::None,
                            const QuantLib::ext::shared_ptr<ScheduleData>& publicationScheduleData = nullptr);

    const Calendar& fixCalendar() const { return fixCalendar_; }
    BusinessDayConvention fixConvention() const { return fixConvention_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    QuantLib::ext::shared_ptr<ZeroInflationIndex> index() const;
    const string& indexName() const { return strIndex_; }
    bool interpolated() const { return interpolated_; }
    Period observationLag() const { return observationLag_; }
    bool adjustInfObsDates() const { return adjustInfObsDates_; }
    const Calendar& infCalendar() const { return infCalendar_; }
    BusinessDayConvention infConvention() const { return infConvention_; }
    PublicationRoll publicationRoll() const { return publicationRoll_; }
    const Schedule& publicationSchedule() const { return publicationSchedule_; }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;

private:
    Calendar fixCalendar_;
    BusinessDayConvention fixConvention_;
    DayCounter dayCounter_;
    QuantLib::ext::shared_ptr<ZeroInflationIndex> index_;
    bool interpolated_;
    Period observationLag_;
    bool adjustInfObsDates_;
    Calendar infCalendar_;
    BusinessDayConvention infConvention_;
    Schedule publicationSchedule_;

    // Store the inputs
    string strFixCalendar_;
    string strFixConvention_;
    string strDayCounter_;
    string strIndex_;
    string strInterpolated_;
    string strObservationLag_;
    string strAdjustInfObsDates_;
    string strInfCalendar_;
    string strInfConvention_;
    PublicationRoll publicationRoll_;
    QuantLib::ext::shared_ptr<ScheduleData> publicationScheduleData_;
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
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
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
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
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
    string strAdvanceCalendar() const { return strAdvanceCalendar_; }
    bool spotRelative() const { return spotRelative_; }
    BusinessDayConvention bdc() const { return bdc_; }
    bool outright() const { return outright_; }
    //@}

    //! \name Serialisation
    //
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
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
    enum class AnchorType { DayOfMonth, NthWeekday, CalendarDaysBefore, LastWeekday, BusinessDaysAfter, WeeklyDayOfTheWeek };
    enum class OptionAnchorType { DayOfMonth, NthWeekday, BusinessDaysBefore, LastWeekday, WeeklyDayOfTheWeek };

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
    
    struct BusinessDaysAfter {
        BusinessDaysAfter(const std::string& businessDaysAfter) : businessDaysAfter_(businessDaysAfter) {}
        std::string businessDaysAfter_;
    };

    struct WeeklyWeekday {
        WeeklyWeekday(const std::string& weekday) : weekday_(weekday) {}
        std::string weekday_;
    };


    struct OptionExpiryAnchorDateRule {
        OptionExpiryAnchorDateRule()
            : type_(OptionAnchorType::BusinessDaysBefore), daysBefore_("0"), expiryDay_(""), nth_(""), weekday_("") {}
        OptionExpiryAnchorDateRule(const DayOfMonth& expiryDay)
            : type_(OptionAnchorType::DayOfMonth), daysBefore_(""), expiryDay_(expiryDay.dayOfMonth_), nth_(""),
              weekday_("") {}
        OptionExpiryAnchorDateRule(const CalendarDaysBefore& businessDaysBefore)
            : type_(OptionAnchorType::BusinessDaysBefore), daysBefore_(businessDaysBefore.calendarDaysBefore_),
              expiryDay_(""), nth_(""), weekday_("") {}
        OptionExpiryAnchorDateRule(const std::string& nth, const std::string& weekday)
            : type_(OptionAnchorType::NthWeekday), daysBefore_(""), expiryDay_(""), nth_(nth), weekday_(weekday) {}
        OptionExpiryAnchorDateRule(const std::string& lastWeekday)
            : type_(OptionAnchorType::LastWeekday), daysBefore_(""), expiryDay_(""), nth_(""), weekday_(lastWeekday) {}
        OptionExpiryAnchorDateRule(const WeeklyWeekday& weekday)
            : type_(OptionAnchorType::WeeklyDayOfTheWeek), daysBefore_(""), expiryDay_(""), nth_(""),
              weekday_(weekday.weekday_) {}

        OptionAnchorType type_;
        std::string daysBefore_;
        std::string expiryDay_;
        std::string nth_;
        std::string weekday_;
    };
    //@}

    /*! Class to hold averaging information when \c isAveraging_ is \c true. It is generally needed 
        in the CommodityFutureConvention when referenced in piecewise price curve construction.
    */
    class AveragingData : public XMLSerializable {
    public:
        //! Indicate location of calculation period relative to the future expiry date.
        enum class CalculationPeriod { PreviousMonth, ExpiryToExpiry };

        //! \name Constructors
        //@{
        //! Default constructor
        AveragingData();
        //! Detailed constructor
        AveragingData(const std::string& commodityName, const std::string& period, const std::string& pricingCalendar,
            bool useBusinessDays, const std::string& conventionsId = "", QuantLib::Natural deliveryRollDays = 0,
            QuantLib::Natural futureMonthOffset = 0, QuantLib::Natural dailyExpiryOffset =
            QuantLib::Null<QuantLib::Natural>());
        //@}

        //! \name Inspectors
        //@{
        const std::string& commodityName() const;
        CalculationPeriod period() const;
        const QuantLib::Calendar& pricingCalendar() const;
        bool useBusinessDays() const;
        const std::string& conventionsId() const;
        QuantLib::Natural deliveryRollDays() const;
        QuantLib::Natural futureMonthOffset() const;
        QuantLib::Natural dailyExpiryOffset() const;
        //@}

        //! Returns \c true if the data has not been populated.
        bool empty() const;

        //! Serialisation
        //@{
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        //@}

    private:
        std::string commodityName_;
        std::string strPeriod_;
        std::string strPricingCalendar_;
        bool useBusinessDays_;
        std::string conventionsId_;
        QuantLib::Natural deliveryRollDays_;
        QuantLib::Natural futureMonthOffset_;
        QuantLib::Natural dailyExpiryOffset_;

        CalculationPeriod period_;
        QuantLib::Calendar pricingCalendar_;

        //! Populate members
        void build();
    };

    //! Class to store conventions for creating an off peak power index 
    class OffPeakPowerIndexData : public XMLSerializable {
    public:
        //! Constructor.
        OffPeakPowerIndexData();

        //! Detailed constructor.
        OffPeakPowerIndexData(
            const std::string& offPeakIndex,
            const std::string& peakIndex,
            const std::string& offPeakHours,
            const std::string& peakCalendar);

        const std::string& offPeakIndex() const { return offPeakIndex_; }
        const std::string& peakIndex() const { return peakIndex_; }
        QuantLib::Real offPeakHours() const { return offPeakHours_; }
        const QuantLib::Calendar& peakCalendar() const { return peakCalendar_; }

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        void build();

    private:
        std::string offPeakIndex_;
        std::string peakIndex_;
        std::string strOffPeakHours_;
        std::string strPeakCalendar_;

        QuantLib::Real offPeakHours_;
        QuantLib::Calendar peakCalendar_;
    };

    //! Class to hold prohibited expiry information.
    class ProhibitedExpiry : public XMLSerializable {
    public:
        ProhibitedExpiry();

        ProhibitedExpiry(
            const QuantLib::Date& expiry,
            bool forFuture = true,
            QuantLib::BusinessDayConvention futureBdc = QuantLib::Preceding,
            bool forOption = true,
            QuantLib::BusinessDayConvention optionBdc = QuantLib::Preceding);

        const QuantLib::Date& expiry() const { return expiry_; }
        bool forFuture() const { return forFuture_; }
        QuantLib::BusinessDayConvention futureBdc() const { return futureBdc_; }
        bool forOption() const { return forOption_; }
        QuantLib::BusinessDayConvention optionBdc() const { return optionBdc_; }

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;

    private:
        QuantLib::Date expiry_;
        bool forFuture_;
        QuantLib::BusinessDayConvention futureBdc_;
        bool forOption_;
        QuantLib::BusinessDayConvention optionBdc_;
    };

    //! \name Constructors
    //@{
    //! Default constructor
    CommodityFutureConvention();

    //! Day of month based constructor
    CommodityFutureConvention(const std::string& id, const DayOfMonth& dayOfMonth, const std::string& contractFrequency,
                              const std::string& calendar, const std::string& expiryCalendar = "",
                              QuantLib::Size expiryMonthLag = 0, const std::string& oneContractMonth = "",
                              const std::string& offsetDays = "", const std::string& bdc = "",
                              bool adjustBeforeOffset = true, bool isAveraging = false,
                              const OptionExpiryAnchorDateRule& optionExpiryDateRule = OptionExpiryAnchorDateRule(),
                              const std::set<ProhibitedExpiry>& prohibitedExpiries = {},
                              QuantLib::Size optionExpiryMonthLag = 0,
                              const std::string& optionBdc = "",
                              const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings = {},
                              const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings = {},
                              const AveragingData& averagingData = AveragingData(),
                              QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                              const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData = boost::none,
                              const std::string& indexName = "", const std::string& optionFrequency = "");

    //! N-th weekday based constructor
    CommodityFutureConvention(const std::string& id, const std::string& nth, const std::string& weekday,
                              const std::string& contractFrequency, const std::string& calendar,
                              const std::string& expiryCalendar = "", QuantLib::Size expiryMonthLag = 0,
                              const std::string& oneContractMonth = "", const std::string& offsetDays = "",
                              const std::string& bdc = "", bool adjustBeforeOffset = true, bool isAveraging = false,
                              const OptionExpiryAnchorDateRule& optionExpiryDateRule = OptionExpiryAnchorDateRule(),
                              const std::set<ProhibitedExpiry>& prohibitedExpiries = {},
                              QuantLib::Size optionExpiryMonthLag = 0,
                              const std::string& optionBdc = "",
                              const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings = {},
                              const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings = {},
                              const AveragingData& averagingData = AveragingData(),
                              QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                              const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData = boost::none,
                              const std::string& indexName = "", const std::string& optionFrequency = "");

    //! Calendar days before based constructor
    CommodityFutureConvention(const std::string& id, const CalendarDaysBefore& calendarDaysBefore,
                              const std::string& contractFrequency, const std::string& calendar,
                              const std::string& expiryCalendar = "", QuantLib::Size expiryMonthLag = 0,
                              const std::string& oneContractMonth = "", const std::string& offsetDays = "",
                              const std::string& bdc = "", bool adjustBeforeOffset = true, bool isAveraging = false,
                              const OptionExpiryAnchorDateRule& optionExpiryDateRule = OptionExpiryAnchorDateRule(),
                              const std::set<ProhibitedExpiry>& prohibitedExpiries = {},
                              QuantLib::Size optionExpiryMonthLag = 0,
                              const std::string& optionBdc = "",
                              const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings = {},
                              const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings = {},
                              const AveragingData& averagingData = AveragingData(),
                              QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                              const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData = boost::none,
                              const std::string& indexName = "", const std::string& optionFrequency = "");
    
    //! Business days before based constructor
    CommodityFutureConvention(const std::string& id, const BusinessDaysAfter& businessDaysAfter,
                              const std::string& contractFrequency, const std::string& calendar,
                              const std::string& expiryCalendar = "", QuantLib::Size expiryMonthLag = 0,
                              const std::string& oneContractMonth = "", const std::string& offsetDays = "",
                              const std::string& bdc = "", bool adjustBeforeOffset = true, bool isAveraging = false,
                              const OptionExpiryAnchorDateRule& optionExpiryDateRule = OptionExpiryAnchorDateRule(),
                              const std::set<ProhibitedExpiry>& prohibitedExpiries = {},
                              QuantLib::Size optionExpiryMonthLag = 0,
                              const std::string& optionBdc = "",
                              const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings = {},
                              const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings = {},
                              const AveragingData& averagingData = AveragingData(),
                              QuantLib::Natural hoursPerDay = QuantLib::Null<QuantLib::Natural>(),
                              const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData = boost::none,
                              const std::string& indexName = "", const std::string& optionFrequency = "");

    //! \name Inspectors
    //@{
    AnchorType anchorType() const { return anchorType_; }
    QuantLib::Natural dayOfMonth() const { return dayOfMonth_; }
    QuantLib::Natural nth() const { return nth_; }
    QuantLib::Weekday weekday() const { return weekday_; }
    QuantLib::Natural calendarDaysBefore() const { return calendarDaysBefore_; }
    QuantLib::Integer businessDaysAfter() const { return businessDaysAfter_; }
    QuantLib::Frequency contractFrequency() const { return contractFrequency_; }
    const QuantLib::Calendar& calendar() const { return calendar_; }
    const QuantLib::Calendar& expiryCalendar() const { return expiryCalendar_; }
    QuantLib::Size expiryMonthLag() const { return expiryMonthLag_; }
    QuantLib::Month oneContractMonth() const { return oneContractMonth_; }
    QuantLib::Integer offsetDays() const { return offsetDays_; }
    QuantLib::BusinessDayConvention businessDayConvention() const { return bdc_; }
    bool adjustBeforeOffset() const { return adjustBeforeOffset_; }
    bool isAveraging() const { return isAveraging_; }
    QuantLib::Natural optionExpiryOffset() const { return optionExpiryOffset_; }
    const std::set<ProhibitedExpiry>& prohibitedExpiries() const {
        return prohibitedExpiries_;
    }
    QuantLib::Size optionExpiryMonthLag() const { return optionExpiryMonthLag_; }
    QuantLib::Natural optionExpiryDay() const { return optionExpiryDay_; }
    QuantLib::BusinessDayConvention optionBusinessDayConvention() const { return optionBdc_; }
    const std::map<QuantLib::Natural, QuantLib::Natural>& futureContinuationMappings() const {
        return futureContinuationMappings_;
    }
    const std::map<QuantLib::Natural, QuantLib::Natural>& optionContinuationMappings() const {
        return optionContinuationMappings_;
    }
    const AveragingData& averagingData() const { return averagingData_; }
    QuantLib::Natural hoursPerDay() const { return hoursPerDay_; }
    const boost::optional<OffPeakPowerIndexData>& offPeakPowerIndexData() const { return offPeakPowerIndexData_; }
    const std::string& indexName() const { return indexName_; }
    QuantLib::Frequency optionContractFrequency() const { return optionContractFrequency_; }
    OptionAnchorType optionAnchorType() const { return optionAnchorType_; }
    QuantLib::Natural optionNth() const { return optionNth_; }
    QuantLib::Weekday optionWeekday() const { return optionWeekday_; }
    const std::string& savingsTime() const { return savingsTime_; }
    const std::set<QuantLib::Month>& validContractMonths() const { return validContractMonths_; }
    bool balanceOfTheMonth() const { return balanceOfTheMonth_; }
    Calendar balanceOfTheMonthPricingCalendar() const { return balanceOfTheMonthPricingCalendar_; }
    const std::string& optionUnderlyingFutureConvention() const { return optionUnderlyingFutureConvention_; }
    //@}

    //! Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! Implementation
    void build() override;

private:
    AnchorType anchorType_;
    QuantLib::Natural dayOfMonth_;
    QuantLib::Natural nth_;
    QuantLib::Weekday weekday_;
    QuantLib::Natural calendarDaysBefore_;
    QuantLib::Integer businessDaysAfter_;
    QuantLib::Frequency contractFrequency_;
    QuantLib::Calendar calendar_;
    QuantLib::Calendar expiryCalendar_;
    QuantLib::Month oneContractMonth_;
    QuantLib::Integer offsetDays_;
    QuantLib::BusinessDayConvention bdc_;
    

    std::string strDayOfMonth_;
    std::string strNth_;
    std::string strWeekday_;
    std::string strCalendarDaysBefore_;
    std::string strBusinessDaysAfter_;
    std::string strContractFrequency_;
    std::string strCalendar_;
    std::string strExpiryCalendar_;
    QuantLib::Size expiryMonthLag_;
    std::string strOneContractMonth_;
    std::string strOffsetDays_;
    std::string strBdc_;
    bool adjustBeforeOffset_;
    bool isAveraging_;
    std::set<ProhibitedExpiry> prohibitedExpiries_;
    QuantLib::Size optionExpiryMonthLag_;
    QuantLib::BusinessDayConvention optionBdc_;
    std::string strOptionBdc_;
    std::map<QuantLib::Natural, QuantLib::Natural> futureContinuationMappings_;
    std::map<QuantLib::Natural, QuantLib::Natural> optionContinuationMappings_;
    AveragingData averagingData_;
    QuantLib::Natural hoursPerDay_;
    boost::optional<OffPeakPowerIndexData> offPeakPowerIndexData_;
    std::string indexName_;
    
    std::string strOptionContractFrequency_;
    
    OptionAnchorType optionAnchorType_;
    std::string strOptionExpiryOffset_;
    std::string strOptionExpiryDay_;
    std::string strOptionNth_;
    std::string strOptionWeekday_;
    
    
    QuantLib::Frequency optionContractFrequency_;
    QuantLib::Natural optionExpiryOffset_;
    QuantLib::Natural optionNth_;
    QuantLib::Weekday optionWeekday_;
    QuantLib::Natural optionExpiryDay_;

    std::set<QuantLib::Month> validContractMonths_;
    std::string savingsTime_;
    // If its averaging Future but the front month is spot averaged and 
    // balance of the month price is the average price of the remaining 
    // future days in contract
    bool balanceOfTheMonth_;
    std::string balanceOfTheMonthPricingCalendarStr_;
    Calendar balanceOfTheMonthPricingCalendar_;
    //! Option Underlying Future convention
    std::string optionUnderlyingFutureConvention_;
    //! Populate and check frequency.
    Frequency parseAndValidateFrequency(const std::string& strFrequency);

    //! Validate the business day conventions in the ProhibitedExpiry
    bool validateBdc(const ProhibitedExpiry& pe) const;
};

//! Compare two prohibited expiries.
bool operator<(const CommodityFutureConvention::ProhibitedExpiry& lhs,
    const CommodityFutureConvention::ProhibitedExpiry& rhs);

//! Container for storing FX Option conventions
/*! Defining a switchTenor is optional. It is set to 0 * Days if no switch tenor is defined. In this case
    longTermAtmType and longTermDeltaType are set to atmType and deltaType respectively.
\ingroup marketdata
*/
class FxOptionConvention : public Convention {
public:
    //! \name Constructors
    //@{
    FxOptionConvention() {}
    FxOptionConvention(const string& id, const string& fxConventionId, const string& atmType, const string& deltaType,
                       const string& switchTenor = "", const string& longTermAtmType = "",
                       const string& longTermDeltaType = "", const string& riskReversalInFavorOf = "Call",
                       const string& butterflyStyle = "Broker");
    //@}

    //! \name Inspectors
    //@{
    const string& fxConventionID() const { return fxConventionID_; }
    const DeltaVolQuote::AtmType& atmType() const { return atmType_; }
    const DeltaVolQuote::DeltaType& deltaType() const { return deltaType_; }
    const Period& switchTenor() const { return switchTenor_; }
    const DeltaVolQuote::AtmType& longTermAtmType() const { return longTermAtmType_; }
    const DeltaVolQuote::DeltaType& longTermDeltaType() const { return longTermDeltaType_; }
    const QuantLib::Option::Type& riskReversalInFavorOf() const { return riskReversalInFavorOf_; }
    const bool butterflyIsBrokerStyle() const { return butterflyIsBrokerStyle_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    virtual void build() override;
    //@}
private:
    string fxConventionID_;
    DeltaVolQuote::AtmType atmType_, longTermAtmType_;
    DeltaVolQuote::DeltaType deltaType_, longTermDeltaType_;
    Period switchTenor_;
    QuantLib::Option::Type riskReversalInFavorOf_;
    bool butterflyIsBrokerStyle_;

    // Strings to store the inputs
    string strAtmType_;
    string strDeltaType_;
    string strSwitchTenor_;
    string strLongTermAtmType_;
    string strLongTermDeltaType_;
    string strRiskReversalInFavorOf_;
    string strButterflyStyle_;
};

/*! Container for storing zero inflation index conventions
   \ingroup marketdata
*/
class ZeroInflationIndexConvention : public Convention {
public:
    //! Constructor.
    ZeroInflationIndexConvention();

    //! Detailed constructor.
    ZeroInflationIndexConvention(const std::string& id,
        const std::string& regionName,
        const std::string& regionCode,
        bool revised,
        const std::string& frequency,
        const std::string& availabilityLag,
        const std::string& currency);

    QuantLib::Region region() const;
    bool revised() const { return revised_; }
    QuantLib::Frequency frequency() const { return frequency_; }
    const QuantLib::Period& availabilityLag() const { return availabilityLag_; }
    const QuantLib::Currency& currency() const { return currency_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void build() override;

private:
    std::string regionName_;
    std::string regionCode_;
    bool revised_;
    std::string strFrequency_;
    std::string strAvailabilityLag_;
    std::string strCurrency_;

    QuantLib::Frequency frequency_;
    QuantLib::Period availabilityLag_;
    QuantLib::Currency currency_;
};

/*! Container for storing bond yield calculation conventions
   \ingroup marketdata
*/
class BondYieldConvention : public Convention {
public:
    //! Constructor.
    BondYieldConvention();

    //! Detailed constructor.
    BondYieldConvention(const std::string& id,
			const std::string& compoundingName,
			const std::string& frequencyName,
			const std::string& priceTypeName,
			Real accuracy,
			Size maxEvaluations,
			Real guess);

    std::string compoundingName() const { return compoundingName_; }
    Compounding compounding() const { return compounding_; }
    std::string frequencyName() const { return frequencyName_; }
    Frequency frequency() const { return frequency_; }
    QuantLib::Bond::Price::Type priceType() const { return priceType_; }
    std::string priceTypeName() const { return priceTypeName_; }
    QuantLib::Real accuracy() const { return accuracy_; }
    QuantLib::Size maxEvaluations() const { return maxEvaluations_; }
    QuantLib::Real guess() const { return guess_; }

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    void build() override;

private:
    Compounding compounding_;
    std::string compoundingName_;
    Frequency frequency_;
    std::string frequencyName_;
    QuantLib::Bond::Price::Type priceType_;
    std::string priceTypeName_;
    QuantLib::Real accuracy_;
    QuantLib::Size maxEvaluations_;
    QuantLib::Real guess_;
};

} // namespace data
} // namespace ore
