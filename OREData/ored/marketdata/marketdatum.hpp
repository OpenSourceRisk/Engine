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

/*! \file ored/marketdata/marketdatum.hpp
    \brief Market data representation
    \ingroup marketdata
*/

#pragma once

#include <ored/marketdata/expiry.hpp>
#include <ored/marketdata/strike.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/serializationdate.hpp>
#include <ored/utilities/serializationdaycounter.hpp>
#include <ored/utilities/serializationperiod.hpp>
#include <ored/utilities/strike.hpp>

#include <ql/currency.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/types.hpp>
#include <string>

#include <boost/make_shared.hpp>
#include <boost/optional.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/variant.hpp>

namespace ore {
namespace data {
using QuantLib::Date;
using QuantLib::DayCounter;
using QuantLib::Handle;
using QuantLib::Month;
using QuantLib::Months;
using QuantLib::Natural;
using QuantLib::Period;
using QuantLib::Quote;
using QuantLib::Real;
using QuantLib::SimpleQuote;
using QuantLib::Size;
using std::string;

//! Base market data class
/*!
  This class holds a single market point, a SimpleQuote pointer and generic
  additional information.

  The market point is classified by an instrument type, a quote type and
  a name string. The name's structure depends on the market point's type
  with tokens separated by "/".

  Specific market data classes are derived from this base class and hold
  additional specific data that are represented by the market point's name.

  \ingroup marketdata
*/
class MarketDatum {
public:
    MarketDatum() {}
    //! Supported market instrument types
    enum class InstrumentType {
        ZERO,
        DISCOUNT,
        MM,
        MM_FUTURE,
        OI_FUTURE,
        FRA,
        IMM_FRA,
        IR_SWAP,
        BASIS_SWAP,
        BMA_SWAP,
        CC_BASIS_SWAP,
        CC_FIX_FLOAT_SWAP,
        CDS,
        CDS_INDEX,
        FX_SPOT,
        FX_FWD,
        HAZARD_RATE,
        RECOVERY_RATE,
        SWAPTION,
        CAPFLOOR,
        FX_OPTION,
        ZC_INFLATIONSWAP,
        ZC_INFLATIONCAPFLOOR,
        YY_INFLATIONSWAP,
        YY_INFLATIONCAPFLOOR,
        SEASONALITY,
        EQUITY_SPOT,
        EQUITY_FWD,
        EQUITY_DIVIDEND,
        EQUITY_OPTION,
        BOND,
        BOND_OPTION,
        INDEX_CDS_OPTION,
        COMMODITY_SPOT,
        COMMODITY_FWD,
        CORRELATION,
        COMMODITY_OPTION,
        CPR,
        RATING,
        NONE
    };

    //! Supported market quote types
    enum class QuoteType {
        BASIS_SPREAD,
        CREDIT_SPREAD,
        CONV_CREDIT_SPREAD,
        YIELD_SPREAD,
        HAZARD_RATE,
        RATE,
        RATIO,
        PRICE,
        RATE_LNVOL,
        RATE_NVOL,
        RATE_SLNVOL,
        BASE_CORRELATION,
        SHIFT,
        TRANSITION_PROBABILITY,
        NONE
    };

    //! Constructor
    MarketDatum(Real value, Date asofDate, const string& name, QuoteType quoteType, InstrumentType instrumentType)
        : quote_(QuantLib::ext::make_shared<SimpleQuote>(value)), asofDate_(asofDate), name_(name),
          instrumentType_(instrumentType), quoteType_(quoteType) {}

    //! Default destructor
    virtual ~MarketDatum() {}

    //! Make a copy of the market datum
    virtual QuantLib::ext::shared_ptr<MarketDatum> clone() {
        return QuantLib::ext::make_shared<MarketDatum>(quote_->value(), asofDate_, name_, quoteType_, instrumentType_);
    }

    //! \name Inspectors
    //@{
    const string& name() const { return name_; }
    const Handle<Quote>& quote() const { return quote_; }
    Date asofDate() const { return asofDate_; }
    InstrumentType instrumentType() const { return instrumentType_; }
    QuoteType quoteType() const { return quoteType_; }
    //@}
protected:
    Handle<Quote> quote_;
    Date asofDate_;
    string name_;
    InstrumentType instrumentType_;
    QuoteType quoteType_;

private:
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

bool operator<(const MarketDatum& a, const MarketDatum& b);

struct SharedPtrMarketDatumComparator {
    bool operator()(const QuantLib::ext::shared_ptr<MarketDatum>& a, const QuantLib::ext::shared_ptr<MarketDatum>& b) const {
        return *a < *b;
    }
};

std::ostream& operator<<(std::ostream& out, const MarketDatum::QuoteType& type);
std::ostream& operator<<(std::ostream& out, const MarketDatum::InstrumentType& type);

//! Money market data class
/*!
  This class holds single market points of type
  - MM

  Specific data comprise currency, fwdStart, term

  \ingroup marketdata
*/
class MoneyMarketQuote : public MarketDatum {
public:
    MoneyMarketQuote() {}
    //! Constructor
    MoneyMarketQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period fwdStart,
                     Period term, const std::string& indexName = "")
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::MM), ccy_(ccy), fwdStart_(fwdStart),
          term_(term), indexName_(indexName) {}
    
    //! Make a copy of the datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<MoneyMarketQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, fwdStart_, term_, indexName_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& fwdStart() const { return fwdStart_; }
    const Period& term() const { return term_; }
    //! Empty if the index name is not provided.
    const std::string& indexName() const { return indexName_; }
    //@}
private:
    string ccy_;
    Period fwdStart_;
    Period term_;
    std::string indexName_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! FRA market data class
/*!
  This class holds single market points of type
  - FRA

  Specific data comprise currency, fwdStart, term

  \ingroup marketdata
*/
class FRAQuote : public MarketDatum {
public:
    FRAQuote() {}
    //! Constructor
    FRAQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period fwdStart,
             Period term)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::FRA), ccy_(ccy), fwdStart_(fwdStart),
          term_(term) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<FRAQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, fwdStart_, term_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& fwdStart() const { return fwdStart_; }
    const Period& term() const { return term_; }
    //@}
private:
    string ccy_;
    Period fwdStart_;
    Period term_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! IMM FRA market data class
/*!
    This class holds single market points of type
    - IMM FRA

    Specific data comprise currency, IMM 1 and IMM 2

    IMM 1 & 2 are strings representing the IMM dates - 1 is the next date,
    up to 9, and then A, B, C, D

\ingroup marketdata
*/
class ImmFraQuote : public MarketDatum {
public:
    ImmFraQuote() {}
    //! Constructor
    ImmFraQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Size imm1, Size imm2)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::IMM_FRA), ccy_(ccy), imm1_(imm1), imm2_(imm2) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<ImmFraQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, imm1_, imm2_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Size& imm1() const { return imm1_; }
    const Size& imm2() const { return imm2_; }
    //@}
private:
    string ccy_;
    Size imm1_;
    Size imm2_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Swap market data class
/*!
  This class holds single market points of type
  - IR_SWAP

  Specific data comprise currency, fwdStart, tenor, term, startDate, maturityDate
  The constructor accepts either fwdStart/term or startDate/maturityDate

  \ingroup marketdata
*/
class SwapQuote : public MarketDatum {
public:
    SwapQuote() {}
    //! Constructor if fwdStart / tenor is given
    SwapQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period fwdStart,
              Period term, Period tenor, const std::string& indexName = "")
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::IR_SWAP), ccy_(ccy), fwdStart_(fwdStart),
          term_(term), tenor_(tenor), indexName_(indexName), startDate_(Null<Date>()), maturityDate_(Null<Date>()) {}
    //! Constructor if startDate, maturityDate is given
    SwapQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Date startDate,
              Date maturityDate, Period tenor, const std::string& indexName = "")
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::IR_SWAP), ccy_(ccy), fwdStart_(Period()),
          term_(Period()), tenor_(tenor), indexName_(indexName), startDate_(startDate), maturityDate_(maturityDate) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        if (startDate_ == Null<Date>() && maturityDate_ == Null<Date>())
            return QuantLib::ext::make_shared<SwapQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, fwdStart_, term_,
                                                 tenor_);
        else
            return QuantLib::ext::make_shared<SwapQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, startDate_,
                                                 maturityDate_, tenor_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& fwdStart() const { return fwdStart_; }
    const Period& term() const { return term_; }
    const Period& tenor() const { return tenor_; }
    const std::string& indeName() const { return indexName_; }
    const Date& startDate() const { return startDate_; }
    const Date& maturityDate() const { return maturityDate_; }
    //@}
private:
    string ccy_;
    Period fwdStart_;
    Period term_;
    Period tenor_;
    std::string indexName_;
    Date startDate_;
    Date maturityDate_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

class ZeroQuote : public MarketDatum {
public:
    ZeroQuote() {}
    //! Constructor
    ZeroQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, const string& ccy, Date date,
              DayCounter dayCounter, Period tenor = Period())
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::ZERO), ccy_(ccy), date_(date),
          dayCounter_(dayCounter), tenor_(tenor) {
        // Minimal adjustment in the absence of a calendar
        QL_REQUIRE(date_ != Date() || tenor != Period(), "ZeroQuote: either date or period is required");
        tenorBased_ = (date_ == Date());
    }
    
    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<ZeroQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, date_, dayCounter_, tenor_);
    }

    //! Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    Date date() const { return date_; }
    DayCounter dayCounter() const { return dayCounter_; }
    const Period& tenor() const { return tenor_; }
    bool tenorBased() const { return tenorBased_; }
    //@}
private:
    string ccy_;
    Date date_;
    DayCounter dayCounter_;
    Period tenor_;
    bool tenorBased_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Discount market data class
/*!
  This class holds single market points of type
  - DISCOUNT.
  Specific data comprise currency, date.

  \ingroup marketdata
*/
class DiscountQuote : public MarketDatum {
public:
    DiscountQuote() {}
    //! Constructor
    DiscountQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Date date, Period tenor)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::DISCOUNT), ccy_(ccy), date_(date), tenor_(tenor) {}
    
    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<DiscountQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, date_, tenor_);
    }
    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    Date date() const { return date_; }
    Period tenor() const { return tenor_; }
    //@}
private:
    string ccy_;
    Date date_;
    Period tenor_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Money Market Future data class
/*! This class holds single market points of type - MM_FUTURE.
    Specific data comprise currency, expiry, contract and future tenor.

    \warning expiry parameter is expected in the format YYYY-MM e.g.
             2013-06 for Jun 2013, 1998-05 for May 1998, etc.

    \ingroup marketdata
*/
class MMFutureQuote : public MarketDatum {
public:
    MMFutureQuote() {}
    //! Constructor
    MMFutureQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, string expiry,
                  string contract = "", Period tenor = 3 * Months)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::MM_FUTURE), ccy_(ccy), expiry_(expiry),
          contract_(contract), tenor_(tenor) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<MMFutureQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, expiry_, contract_, tenor_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const string& expiry() const { return expiry_; }
    Natural expiryYear() const;
    Month expiryMonth() const;
    const string& contract() const { return contract_; }
    const Period& tenor() const { return tenor_; }
    //@}

private:
    string ccy_;
    string expiry_;
    string contract_;
    Period tenor_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Overnight index future data class
/*! This class holds single market points of type - OI_FUTURE.
    Specific data comprise currency, expiry, contract and future tenor.

    \warning expiry parameter is expected in the format YYYY-MM e.g.
             2013-06 for Jun 2013, 1998-05 for May 1998, etc.

    \ingroup marketdata
*/
class OIFutureQuote : public MarketDatum {
public:
    OIFutureQuote() {}
    //! Constructor
    OIFutureQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, string expiry,
                  string contract = "", Period tenor = 3 * Months)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::OI_FUTURE), ccy_(ccy), expiry_(expiry),
          contract_(contract), tenor_(tenor) {}
    
    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<OIFutureQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, expiry_, contract_, tenor_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const string& expiry() const { return expiry_; }
    Natural expiryYear() const;
    Month expiryMonth() const;
    const string& contract() const { return contract_; }
    const Period& tenor() const { return tenor_; }
    //@}

private:
    string ccy_;
    string expiry_;
    string contract_;
    Period tenor_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Basis Swap data class
/*!
  This class holds single market points of type
  - BASIS_SWAP SPREAD
  Specific data comprise
  - flat term
  - term

  The quote (in Basis Points) is then interpreted as follows:

  A fair Swap pays the reference index with "flat term" with spread zero
  and receives the reference index with "term" plus the quoted spread.

  \ingroup marketdata
*/
class BasisSwapQuote : public MarketDatum {
public:
    BasisSwapQuote() {}
    //! Constructor
    BasisSwapQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, Period flatTerm, Period term,
                   string ccy = "USD", Period maturity = 3 * Months)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::BASIS_SWAP), flatTerm_(flatTerm), term_(term),
          ccy_(ccy), maturity_(maturity) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<BasisSwapQuote>(quote_->value(), asofDate_, name_, quoteType_, flatTerm_, term_, ccy_, maturity_);
    }

    //! \name Inspectors
    //@{
    const Period& flatTerm() const { return flatTerm_; }
    const Period& term() const { return term_; }
    const string& ccy() const { return ccy_; }
    const Period& maturity() const { return maturity_; }
    //@}
private:
    Period flatTerm_;
    Period term_;
    string ccy_;
    Period maturity_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! BMA Swap data class
/*!
This class holds single market points of type
- BMA_SWAP
Specific data comprise
- term
- currency
- maturity

The quote (in Basis Points) is then interpreted as follows:

A fair Swap pays the libor index with gearing equal to the quote
and receives the bma index.

\ingroup marketdata
*/
class BMASwapQuote : public MarketDatum {
public:
    BMASwapQuote() {}
    //! Constructor
    BMASwapQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, Period term, string ccy = "USD",
                 Period maturity = 3 * Months)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::BMA_SWAP), term_(term), ccy_(ccy),
          maturity_(maturity) {}
    
    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<BMASwapQuote>(quote_->value(), asofDate_, name_, quoteType_, term_, ccy_, maturity_);
    }

    //! \name Inspectors
    //@{
    const Period& term() const { return term_; }
    const string& ccy() const { return ccy_; }
    const Period& maturity() const { return maturity_; }
    //@}
private:
    Period term_;
    string ccy_;
    Period maturity_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Cross Currency Basis Swap data class
/*!
  This class holds single market points of type
  - CC_BASIS_SWAP BASIS_SPREAD
  Specific data comprise
  - flat currency
  - currency

  The quote in Basis Points is then interpreted as follows:

  A fair Swap pays the reference index of "flat currency" in "flat currency"
  with spread zero and receives the reference index of "currency" in
  "currency" plus the quoted spread.

  \ingroup marketdata
*/
class CrossCcyBasisSwapQuote : public MarketDatum {
public:
    CrossCcyBasisSwapQuote() {}
    //! Constructor
    CrossCcyBasisSwapQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string flatCcy,
                           Period flatTerm, string ccy, Period term, Period maturity = 3 * Months)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::CC_BASIS_SWAP), flatCcy_(flatCcy),
          flatTerm_(flatTerm), ccy_(ccy), term_(term), maturity_(maturity) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CrossCcyBasisSwapQuote>(quote_->value(), asofDate_, name_, quoteType_, flatCcy_, flatTerm_, ccy_, term_, maturity_);
    }

    //! \name Inspectors
    //@{
    const string& flatCcy() const { return flatCcy_; }
    const Period& flatTerm() const { return flatTerm_; }
    const string& ccy() const { return ccy_; }
    const Period& term() const { return term_; }
    const Period& maturity() const { return maturity_; }
    //@}

private:
    string flatCcy_;
    Period flatTerm_;
    string ccy_;
    Period term_;
    Period maturity_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Cross Currency Fix Float Swap quote holder
/*! Holds the quote for the fair fixed rate on a fixed against float
    cross currency swap.

    \ingroup marketdata
*/
class CrossCcyFixFloatSwapQuote : public MarketDatum {
public:
    CrossCcyFixFloatSwapQuote() {}
    //! Constructor
    CrossCcyFixFloatSwapQuote(QuantLib::Real value, const QuantLib::Date& asof, const std::string& name,
                              QuoteType quoteType, const string& floatCurrency, const QuantLib::Period& floatTenor,
                              const string& fixedCurrency, const QuantLib::Period& fixedTenor,
                              const QuantLib::Period& maturity)
        : MarketDatum(value, asof, name, quoteType, InstrumentType::CC_FIX_FLOAT_SWAP), floatCurrency_(floatCurrency),
          floatTenor_(floatTenor), fixedCurrency_(fixedCurrency), fixedTenor_(fixedTenor), maturity_(maturity) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CrossCcyFixFloatSwapQuote>(quote_->value(), asofDate_, name_, quoteType_, floatCurrency_, floatTenor_, fixedCurrency_, fixedTenor_, maturity_);
    }

    //! \name Inspectors
    //@{
    const string& floatCurrency() const { return floatCurrency_; }
    const QuantLib::Period& floatTenor() const { return floatTenor_; }
    const string& fixedCurrency() const { return fixedCurrency_; }
    const QuantLib::Period& fixedTenor() const { return fixedTenor_; }
    const QuantLib::Period& maturity() const { return maturity_; }
    //@}

private:
    string floatCurrency_;
    QuantLib::Period floatTenor_;
    string fixedCurrency_;
    QuantLib::Period fixedTenor_;
    QuantLib::Period maturity_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

/*! CDS Spread data class
    This class holds single market points of type
    - CREDIT_SPREAD
    - CONV_CREDIT_SPREAD
    - PRICE
    \ingroup marketdata
*/
class CdsQuote : public MarketDatum {
public:
    CdsQuote() : runningSpread_(Null<Real>()) {}

    //! Constructor
    CdsQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, const string& underlyingName,
             const string& seniority, const string& ccy, Period term, const string& docClause = "",
             Real runningSpread = Null<Real>())
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::CDS), underlyingName_(underlyingName),
          seniority_(seniority), ccy_(ccy), term_(term), docClause_(docClause), runningSpread_(runningSpread) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CdsQuote>(quote_->value(), asofDate_, name_, quoteType_, underlyingName_,
            seniority_, ccy_, term_, docClause_, runningSpread_);
    }

    //! \name Inspectors
    //@{
    const Period& term() const { return term_; }
    const string& seniority() const { return seniority_; }
    const string& ccy() const { return ccy_; }
    const string& underlyingName() const { return underlyingName_; }
    const string& docClause() const { return docClause_; }
    Real runningSpread() const { return runningSpread_; }
    //@}

private:
    string underlyingName_;
    string seniority_;
    string ccy_;
    Period term_;
    string docClause_;
    Real runningSpread_;

    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Hazard rate data class
/*!
  This class holds single market points of type
  - HAZARD_RATE

  \ingroup marketdata
*/
class HazardRateQuote : public MarketDatum {
public:
    HazardRateQuote() {}
    //! Constructor
    HazardRateQuote(Real value, Date asofDate, const string& name, const string& underlyingName,
                    const string& seniority, const string& ccy, Period term, const string& docClause = "")
        : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::HAZARD_RATE),
          underlyingName_(underlyingName), seniority_(seniority), ccy_(ccy), term_(term), docClause_(docClause) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<HazardRateQuote>(quote_->value(), asofDate_, name_, underlyingName_, seniority_, ccy_, term_, docClause_);
    }

    //! \name Inspectors
    //@{
    const Period& term() const { return term_; }
    const string& seniority() const { return seniority_; }
    const string& ccy() const { return ccy_; }
    const string& underlyingName() const { return underlyingName_; }
    const string& docClause() const { return docClause_; }
    //@}
private:
    string underlyingName_;
    string seniority_;
    string ccy_;
    Period term_;
    string docClause_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Recovery rate data class
/*!
  This class holds single market points of type
  - RECOVERY_RATE
  \ingroup marketdata
*/
class RecoveryRateQuote : public MarketDatum {
public:
    RecoveryRateQuote() {}
    //! Constructor
    RecoveryRateQuote(Real value, Date asofDate, const string& name, const string& underlyingName,
                      const string& seniority, const string& ccy, const string& docClause = "")
        : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::RECOVERY_RATE),
          underlyingName_(underlyingName), seniority_(seniority), ccy_(ccy), docClause_(docClause) {}
    
    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<RecoveryRateQuote>(quote_->value(), asofDate_, name_, underlyingName_, seniority_, ccy_, docClause_);
    }

    //! \name Inspectors
    //@{
    const string& seniority() const { return seniority_; }
    const string& ccy() const { return ccy_; }
    const string& underlyingName() const { return underlyingName_; }
    const string& docClause() const { return docClause_; }
    //@}
private:
    string underlyingName_;
    string seniority_;
    string ccy_;
    string docClause_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Swaption data class
/*!
  This class holds single market points of type
  - SWAPTION
  Specific data comprise
  - currency
  - expiry
  - term
  - at-the-money flag (is an at-the-money swaption quote?)
  - strike

  \ingroup marketdata
*/
class SwaptionQuote : public MarketDatum {
public:
    SwaptionQuote() {}
    //! Constructor
    SwaptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period expiry,
                  Period term, string dimension, Real strike = 0.0, const std::string& quoteTag = std::string(),
                  bool isPayer = true)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::SWAPTION), ccy_(ccy), expiry_(expiry),
          term_(term), dimension_(dimension), strike_(strike), quoteTag_(quoteTag), isPayer_(isPayer) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<SwaptionQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, expiry_, term_,
                                                 dimension_, strike_, quoteTag_, isPayer_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& expiry() const { return expiry_; }
    const Period& term() const { return term_; }
    const string& dimension() const { return dimension_; }
    Real strike() { return strike_; }
    const string& quoteTag() const { return quoteTag_; }
    bool isPayer() const { return isPayer_; }
    //@}
private:
    string ccy_;
    Period expiry_;
    Period term_;
    string dimension_;
    Real strike_;
    string quoteTag_;
    bool isPayer_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Shift data class (for SLN swaption volatilities)
/*!
  This class holds single market points of type
  - SHIFT
  Specific data comprise
  - currency
  - term

  \ingroup marketdata
*/
class SwaptionShiftQuote : public MarketDatum {
public:
    SwaptionShiftQuote() {}
    //! Constructor
    SwaptionShiftQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period term,
                       const std::string& quoteTag = std::string())
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::SWAPTION), ccy_(ccy), term_(term),
          quoteTag_(quoteTag) {
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::SHIFT, "quote type must be SHIFT for shift data");
    }

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<SwaptionShiftQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, term_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& term() const { return term_; }
    const string& quoteTag() const { return quoteTag_; }
    //@}
private:
    string ccy_;
    Period term_;
    string quoteTag_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Bond option data class
/*!
This class holds single market points of type
- BOND_OPTION
Specific data comprise
- qualifier
- expiry
- term

\ingroup marketdata
*/

class BondOptionQuote : public MarketDatum {
public:
    BondOptionQuote() {}
    //! Constructor
    BondOptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string qualifier, Period expiry,
                    Period term)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::BOND_OPTION), qualifier_(qualifier),
          expiry_(expiry), term_(term) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<BondOptionQuote>(quote_->value(), asofDate_, name_, quoteType_, qualifier_, expiry_, term_);
    }

    //! \name Inspectors
    //@{
    const string& qualifier() const { return qualifier_; }
    const Period& expiry() const { return expiry_; }
    const Period& term() const { return term_; }
    //@}
private:
    string qualifier_;
    Period expiry_;
    Period term_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Shift data class (for SLN bond option volatilities)
/*!
This class holds single market points of type
- SHIFT
Specific data comprise
- qualifier
- term

\ingroup marketdata
*/

class BondOptionShiftQuote : public MarketDatum {
public:
    BondOptionShiftQuote() {}
    //! Constructor
    BondOptionShiftQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string qualifier,
                         Period term)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::BOND_OPTION), qualifier_(qualifier),
          term_(term) {
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::SHIFT, "quote type must be SHIFT for shift data");
    }

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<BondOptionShiftQuote>(quote_->value(), asofDate_, name_, quoteType_, qualifier_, term_);
    }

    //! \name Inspectors
    //@{
    const string& qualifier() const { return qualifier_; }
    const Period& term() const { return term_; }
    //@}
private:
    string qualifier_;
    Period term_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Cap/Floor data class
/*!
  This class holds single market points of type
  - CAPFLOOR
  Specific data comprise
  - currency
  - term
  - underlying index tenor
  - at-the-money flag (is an at-the-money cap/floor quote?)
  - relative quotation flag (quote to be added to the at-the-money quote?)
  - strike

  \ingroup marketdata
*/
class CapFloorQuote : public MarketDatum {
public:
    CapFloorQuote() {}
    //! Constructor
    CapFloorQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period term,
                  Period underlying, bool atm, bool relative, Real strike = 0.0, const string& indexName = string(), bool isCap = true)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::CAPFLOOR), ccy_(ccy), term_(term),
          underlying_(underlying), atm_(atm), relative_(relative), strike_(strike), indexName_(indexName), isCap_(isCap) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CapFloorQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, term_,
                                                 underlying_, atm_, relative_, strike_, indexName_, isCap_);
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& term() const { return term_; }
    const Period& underlying() const { return underlying_; }
    bool atm() const { return atm_; }
    bool relative() const { return relative_; }
    Real strike() { return strike_; }
    const string& indexName() const { return indexName_; }
    bool isCap() const { return isCap_; }
    //@}
private:
    string ccy_;
    Period term_;
    Period underlying_;
    bool atm_;
    bool relative_;
    Real strike_;
    string indexName_;
    bool isCap_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Shift data class (for SLN cap/floor volatilities)
/*! This class holds, for a given currency and index tenor, single market points of type
    - SHIFT
    \ingroup marketdata
*/
class CapFloorShiftQuote : public MarketDatum {
public:
    CapFloorShiftQuote() {}
    CapFloorShiftQuote(Real value, const Date& asofDate, const string& name, QuoteType quoteType, const string& ccy,
                       const Period& indexTenor, const string& indexName = string())
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::CAPFLOOR), ccy_(ccy), indexTenor_(indexTenor),
          indexName_(indexName) {
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::SHIFT, "Quote type must be SHIFT for shift data");
    }

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CapFloorShiftQuote>(quote_->value(), asofDate_, name_, quoteType_, ccy_, indexTenor_,
                                                      indexName_);
    }

    const string& ccy() const { return ccy_; }
    const Period& indexTenor() const { return indexTenor_; }
    const string& indexName() const { return indexName_; }

private:
    string ccy_;
    Period indexTenor_;
    string indexName_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Foreign exchange rate data class
/*!
  This class holds single market points of type
  - FX_SPOT
  Specific data comprise
  - unit currency
  - currency

  The quote is then interpreted as follows:

  1 unit of "unit currency" = quote * 1 unit of "currency"

  \ingroup marketdata
*/
class FXSpotQuote : public MarketDatum {
public:
    FXSpotQuote() {}
    //! Constructor
    FXSpotQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string unitCcy, string ccy)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::FX_SPOT), unitCcy_(unitCcy), ccy_(ccy) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<FXSpotQuote>(quote_->value(), asofDate_, name_, quoteType_, unitCcy_, ccy_);
    }

    //! \name Inspectors
    //@{
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    //@}
private:
    string unitCcy_;
    string ccy_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Foreign exchange rate data class
/*!
  This class holds single market points of type
  - FX_FWD
  Specific data comprise
  - unit currency
  - currency
  - term
  - conversion factor

  The quote is expected in "forward points" = (FXFwd - FXSpot) / conversionFactor

  \ingroup marketdata
*/
class FXForwardQuote : public MarketDatum {
public:
    enum class FxFwdString { ON, TN, SN };

    FXForwardQuote() {}
    //! Constructor
    FXForwardQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string unitCcy, string ccy,
                   const boost::variant<QuantLib::Period, FxFwdString>& term, Real conversionFactor = 1.0)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::FX_FWD), unitCcy_(unitCcy), ccy_(ccy),
          term_(term), conversionFactor_(conversionFactor) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<FXForwardQuote>(quote_->value(), asofDate_, name_, quoteType_, unitCcy_, ccy_, term_, conversionFactor_);
    }

    //! \name Inspectors
    //@{
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    const boost::variant<QuantLib::Period, FxFwdString>& term() const { return term_; }
    Real conversionFactor() const { return conversionFactor_; }
    //@}
private:
    string unitCcy_;
    string ccy_;
    boost::variant<QuantLib::Period, FxFwdString> term_;
    Real conversionFactor_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! FX Option data class
/*!
  This class holds single market points of type
  - FX_OPTION
  Specific data comprise
  - unit currency
  - currency
  - expiry
  - "strike" (25 delta butterfly "25BF", 25 delta risk reversal "25RR", atm straddle ATM, or individual delta put/call
  quotes) we do not yet support ATMF.

  \ingroup marketdata
*/
class FXOptionQuote : public MarketDatum {
public:
    FXOptionQuote() {}
    //! Constructor
    FXOptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string unitCcy, string ccy,
                  Period expiry, string strike)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::FX_OPTION), unitCcy_(unitCcy), ccy_(ccy),
          expiry_(expiry), strike_(strike) {

        Strike s = parseStrike(strike);
        QL_REQUIRE(s.type == Strike::Type::DeltaCall || s.type == Strike::Type::DeltaPut ||
                       s.type == Strike::Type::ATM || s.type == Strike::Type::BF || s.type == Strike::Type::RR || s.type == Strike::Type::Absolute,
                   "Unsupported FXOptionQuote strike (" << strike << ")");
    }

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<FXOptionQuote>(quote_->value(), asofDate_, name_, quoteType_, unitCcy_, ccy_, expiry_, strike_);
    }

    //! \name Inspectors
    //@{
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    const Period& expiry() const { return expiry_; }
    const string& strike() const { return strike_; }
    //@}
private:
    string unitCcy_;
    string ccy_;
    Period expiry_;
    string strike_; // TODO: either: ATM, 25RR, 25BF. Should be an enum?
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! ZC Inflation swap data class
/*!
 This class holds single market points of type
 - ZC_INFLATIONSWAP
 Specific data comprise index, term.

 \ingroup marketdata
 */
class ZcInflationSwapQuote : public MarketDatum {
public:
    ZcInflationSwapQuote() {}
    ZcInflationSwapQuote(Real value, Date asofDate, const string& name, const string& index, Period term)
        : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::ZC_INFLATIONSWAP), index_(index),
          term_(term) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<ZcInflationSwapQuote>(quote_->value(), asofDate_, name_, index_, term_);
    }

    string index() { return index_; }
    Period term() { return term_; }

private:
    string index_;
    Period term_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Inflation Cap Floor data class
/*!
This class holds single market points of type
- INFLATION_CAPFLOOR
Specific data comprise type (can be price or nvol or slnvol),
index, term, cap/floor, strike

\ingroup marketdata
*/
class InflationCapFloorQuote : public MarketDatum {
public:
    InflationCapFloorQuote() {}
    InflationCapFloorQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, const string& index,
                           Period term, bool isCap, const string& strike, InstrumentType instrumentType)
        : MarketDatum(value, asofDate, name, quoteType, instrumentType), index_(index), term_(term), isCap_(isCap),
          strike_(strike) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<InflationCapFloorQuote>(quote_->value(), asofDate_, name_, quoteType_, index_, term_, isCap_, strike_, instrumentType_);
    }

    string index() { return index_; }
    Period term() { return term_; }
    bool isCap() { return isCap_; }
    string strike() { return strike_; }

private:
    string index_;
    Period term_;
    bool isCap_;
    string strike_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! ZC Cap Floor data class
/*!
 This class holds single market points of type
 - ZC_INFLATION_CAPFLOOR
 Specific data comprise type (can be price or nvol or slnvol),
 index, term, cap/floor, strike

 \ingroup marketdata
 */
class ZcInflationCapFloorQuote : public InflationCapFloorQuote {
public:
    ZcInflationCapFloorQuote() {}
    ZcInflationCapFloorQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, const string& index,
                             Period term, bool isCap, const string& strike)
        : InflationCapFloorQuote(value, asofDate, name, quoteType, index, term, isCap, strike,
                                 InstrumentType::ZC_INFLATIONCAPFLOOR) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<ZcInflationCapFloorQuote>(quote_->value(), asofDate_, name_, quoteType_, index(), term(), isCap(), strike());
    }

private:
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! YoY Inflation swap data class
/*!
 This class holds single market points of type
 - YOY_INFLATIONSWAP
 Specific data comprise index, term.

 \ingroup marketdata
 */
class YoYInflationSwapQuote : public MarketDatum {
public:
    YoYInflationSwapQuote() {}
    YoYInflationSwapQuote(Real value, Date asofDate, const string& name, const string& index, Period term)
        : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::YY_INFLATIONSWAP), index_(index),
          term_(term) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<YoYInflationSwapQuote>(quote_->value(), asofDate_, name_, index_, term_);
    }

    string index() { return index_; }
    Period term() { return term_; }

private:
    string index_;
    Period term_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! YY Cap Floor data class
/*!
This class holds single market points of type
- YY_INFLATION_CAPFLOOR
Specific data comprise type (can be price or nvol or slnvol),
index, term, cap/floor, strike

\ingroup marketdata
*/
class YyInflationCapFloorQuote : public InflationCapFloorQuote {
public:
    YyInflationCapFloorQuote() {}
    YyInflationCapFloorQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, const string& index,
                             Period term, bool isCap, const string& strike)
        : InflationCapFloorQuote(value, asofDate, name, quoteType, index, term, isCap, strike,
                                 InstrumentType::YY_INFLATIONCAPFLOOR) {}


    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<YyInflationCapFloorQuote>(quote_->value(), asofDate_, name_, quoteType_, index(), term(), isCap(), strike());
    }
private:
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Inflation seasonality data class
/*!
 This class holds single market points of type
 - SEASONALITY
 Specific data comprise inflation index, factor type (ADD, MULT) and month (JAN to DEC).

 \ingroup marketdata
 */
class SeasonalityQuote : public MarketDatum {
public:
    SeasonalityQuote() {}
    SeasonalityQuote(Real value, Date asofDate, const string& name, const string& index, const string& type,
                     const string& month)
        : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::SEASONALITY), index_(index), type_(type),
          month_(month) {}
    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<SeasonalityQuote>(quote_->value(), asofDate_, name_, index(), type(), month());
    }

    string index() { return index_; }
    string type() { return type_; }
    string month() { return month_; }
    QuantLib::Size applyMonth() const;

private:
    string index_;
    string type_;
    string month_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Equity/Index spot price data class
/*!
This class holds single market points of type
- EQUITY_SPOT
Specific data comprise
- Equity/Index name
- currency

\ingroup marketdata
*/
class EquitySpotQuote : public MarketDatum {
public:
    EquitySpotQuote() {}
    //! Constructor
    EquitySpotQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string equityName, string ccy)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::EQUITY_SPOT), eqName_(equityName), ccy_(ccy) {}


    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<EquitySpotQuote>(quote_->value(), asofDate_, name_, quoteType_, eqName_, ccy_);
    }

    //! \name Inspectors
    //@{
    const string& eqName() const { return eqName_; }
    const string& ccy() const { return ccy_; }
    //@}
private:
    string eqName_;
    string ccy_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Equity forward data class
/*!
This class holds single market points of type
- EQUITY_FWD
Specific data comprise
- Equity/Index name
- currency
- expiry date

The quote is expected as a forward price

\ingroup marketdata
*/
class EquityForwardQuote : public MarketDatum {
public:
    EquityForwardQuote() {}
    //! Constructor
    EquityForwardQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string equityName,
                       string ccy, const Date& expiryDate);

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<EquityForwardQuote>(quote_->value(), asofDate_, name_, quoteType_, eqName_, ccy_, expiry_);
    }

    //! \name Inspectors
    //@{
    const string& eqName() const { return eqName_; }
    const string& ccy() const { return ccy_; }
    const Date& expiryDate() const { return expiry_; }
    //@}
private:
    string eqName_;
    string ccy_;
    Date expiry_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Equity/Index Dividend yield data class
/*!
This class holds single market points of type
- EQUITY_DIVIDEND
Specific data comprise
- Equity/Index name
- currency
- yield tenor date

The quote is expected as a forward price

\ingroup marketdata
*/
class EquityDividendYieldQuote : public MarketDatum {
public:
    EquityDividendYieldQuote() {}
    //! Constructor
    EquityDividendYieldQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string equityName,
                             string ccy, const Date& tenorDate);

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<EquityDividendYieldQuote>(quote_->value(), asofDate_, name_, quoteType_, eqName_, ccy_, tenor_);
    }

    //! \name Inspectors
    //@{
    const string& eqName() const { return eqName_; }
    const string& ccy() const { return ccy_; }
    const Date& tenorDate() const { return tenor_; }
    //@}
private:
    string eqName_;
    string ccy_;
    Date tenor_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Equity/Index Option data class
/*!
This class holds single market points of type
- EQUITY_OPTION
Specific data comprise
- Equity/Index name
- currency
- expiry
- strike - supported are:
           - absolute strike, e.g. 1234.5
           - ATM/AtmFwd          (or as an alias ATMF)
           - MNY/[Spot/Fwd]/1.2
- C (call), P (put) flag, this is optional and defaults to C

\ingroup marketdata
*/
class EquityOptionQuote : public MarketDatum {
public:
    EquityOptionQuote() {}
    //! Constructor
    EquityOptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string equityName, string ccy,
                      string expiry, const QuantLib::ext::shared_ptr<BaseStrike>& strike, bool isCall = true);


    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<EquityOptionQuote>(quote_->value(), asofDate_, name_, quoteType_, eqName_, ccy_, expiry_, strike_, isCall_);
    }

    //! \name Inspectors
    //@{
    const string& eqName() const { return eqName_; }
    const string& ccy() const { return ccy_; }
    const string& expiry() const { return expiry_; }
    const QuantLib::ext::shared_ptr<BaseStrike>& strike() const { return strike_; }
    bool isCall() { return isCall_; }
    //@}
private:
    string eqName_;
    string ccy_;
    string expiry_;
    QuantLib::ext::shared_ptr<BaseStrike> strike_;
    bool isCall_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Bond spread data class
/*!
This class holds single market points of type
- BOND SPREAD
\ingroup marketdata
*/
class SecuritySpreadQuote : public MarketDatum {
public:
    SecuritySpreadQuote() {}
    //! Constructor
    SecuritySpreadQuote(Real value, Date asofDate, const string& name, const string& securityID)
        : MarketDatum(value, asofDate, name, QuoteType::YIELD_SPREAD, InstrumentType::BOND), securityID_(securityID) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<SecuritySpreadQuote>(quote_->value(), asofDate_, name_, securityID_);
    }

    //! \name Inspectors
    //@{
    const string& securityID() const { return securityID_; }
    //@}
private:
    string securityID_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Base correlation data class
/*!
This class holds single market points of type
- CDS_INDEX BASE_CORRELATION
\ingroup marketdata
*/
class BaseCorrelationQuote : public MarketDatum {
public:
    BaseCorrelationQuote() {}
    //! Constructor
    BaseCorrelationQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, const string& cdsIndexName,
                         Period term, Real detachmentPoint)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::CDS_INDEX), cdsIndexName_(cdsIndexName),
          term_(term), detachmentPoint_(detachmentPoint) {}


    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<BaseCorrelationQuote>(quote_->value(), asofDate_, name_, quoteType_, cdsIndexName_, term_, detachmentPoint_);
    }

    //! \name Inspectors
    //@{
    const string& cdsIndexName() const { return cdsIndexName_; }
    Real detachmentPoint() const { return detachmentPoint_; }
    Period term() const { return term_; }
    //@}
private:
    string cdsIndexName_;
    Period term_;
    Real detachmentPoint_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! CDS Index Option data class
/*! This class holds single market points of type \c INDEX_CDS_OPTION
    \ingroup marketdata
*/
class IndexCDSOptionQuote : public MarketDatum {
public:
    //! Default constructor
    IndexCDSOptionQuote() {}

    //! Detailed constructor
    /*! \param value     The volatility value
        \param asof      The quote date
        \param name      The quote name
        \param indexName The name of the CDS index
        \param expiry    Expiry object defining the quote's expiry
        \param indexTerm The term of the underlying CDS index e.g. 3Y, 5Y, 7Y, 10Y etc. If not given, defaults to
                         an empty string. Assumed here that the term is encoded in \c indexName.
        \param strike    Strike object defining the quote's strike. If not given, assumed that quote is ATM.
    */
    IndexCDSOptionQuote(QuantLib::Real value, const QuantLib::Date& asof, const std::string& name,
                        const std::string& indexName, const QuantLib::ext::shared_ptr<Expiry>& expiry,
                        const std::string& indexTerm = "", const QuantLib::ext::shared_ptr<BaseStrike>& strike = nullptr);


    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<IndexCDSOptionQuote>(quote_->value(), asofDate_, name_, indexName_, expiry_, indexTerm_, strike_);
    }

    //! \name Inspectors
    //@{
    const std::string& indexName() const { return indexName_; }
    const QuantLib::ext::shared_ptr<Expiry>& expiry() const { return expiry_; }
    const std::string& indexTerm() const { return indexTerm_; }
    const QuantLib::ext::shared_ptr<BaseStrike>& strike() const { return strike_; }
    //@}

private:
    std::string indexName_;
    QuantLib::ext::shared_ptr<Expiry> expiry_;
    std::string indexTerm_;
    QuantLib::ext::shared_ptr<BaseStrike> strike_;

    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Commodity spot quote class
/*! This class holds a spot price for a commodity in a given currency
    \ingroup marketdata
*/
class CommoditySpotQuote : public MarketDatum {
public:
    CommoditySpotQuote() {}
    //! Constructor
    CommoditySpotQuote(QuantLib::Real value, const QuantLib::Date& asofDate, const std::string& name,
                       QuoteType quoteType, const std::string& commodityName, const std::string& quoteCurrency)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::COMMODITY_SPOT), commodityName_(commodityName),
          quoteCurrency_(quoteCurrency) {
        QL_REQUIRE(quoteType == QuoteType::PRICE, "Commodity spot quote must be of type 'PRICE'");
    }

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CommoditySpotQuote>(quote_->value(), asofDate_, name_, quoteType_, commodityName_, quoteCurrency_);
    }

    //! \name Inspectors
    //@{
    const std::string& commodityName() const { return commodityName_; }
    const std::string& quoteCurrency() const { return quoteCurrency_; }
    //@}

private:
    std::string commodityName_;
    std::string quoteCurrency_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Commodity forward quote class
/*! This class holds a forward price for a commodity in a given currency
    \ingroup marketdata
*/
class CommodityForwardQuote : public MarketDatum {
public:
    CommodityForwardQuote() {}
    //! Date based commodity forward constructor
    CommodityForwardQuote(QuantLib::Real value, const QuantLib::Date& asofDate, const std::string& name,
                          QuoteType quoteType, const std::string& commodityName, const std::string& quoteCurrency,
                          const QuantLib::Date& expiryDate);

    //! Tenor based commodity forward constructor
    CommodityForwardQuote(QuantLib::Real value, const QuantLib::Date& asofDate, const std::string& name,
                          QuoteType quoteType, const std::string& commodityName, const std::string& quoteCurrency,
                          const QuantLib::Period& tenor, boost::optional<QuantLib::Period> startTenor = boost::none);

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        if (tenorBased_) {
            return QuantLib::ext::make_shared<CommodityForwardQuote>(quote_->value(), asofDate_, name_, quoteType_, commodityName_, quoteCurrency_, tenor_, startTenor_);
        } else {
            return QuantLib::ext::make_shared<CommodityForwardQuote>(quote_->value(), asofDate_, name_, quoteType_, commodityName_, quoteCurrency_, expiryDate_);
        }
    }

    //! \name Inspectors
    //@{
    const std::string& commodityName() const { return commodityName_; }
    const std::string& quoteCurrency() const { return quoteCurrency_; }

    //! The commodity forward's expiry if the quote is date based
    const QuantLib::Date& expiryDate() const { return expiryDate_; }

    //! The commodity forward's tenor if the quote is tenor based
    const QuantLib::Period& tenor() const { return tenor_; }

    /*! The period between the as of date and the date from which the forward tenor is applied. This is generally the
        spot tenor which is indicated by \c boost::none but there are special cases:
        - overnight forward: \c startTenor will be <code>0 * Days</code> and \c tenor will be <code>1 * Days</code>
        - tom-next forward: \c startTenor will be <code>1 * Days</code> and \c tenor will be <code>1 * Days</code>
    */
    const boost::optional<QuantLib::Period>& startTenor() const { return startTenor_; }

    //! Returns \c true if the forward is tenor based and \c false if forward is date based
    bool tenorBased() const { return tenorBased_; }
    //@}

private:
    std::string commodityName_;
    std::string quoteCurrency_;
    QuantLib::Date expiryDate_;
    QuantLib::Period tenor_;
    boost::optional<QuantLib::Period> startTenor_;
    bool tenorBased_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Commodity option data class
/*! This class holds single market points of type COMMODITY_OPTION
    \ingroup marketdata
*/
class CommodityOptionQuote : public MarketDatum {
public:
    CommodityOptionQuote() : optionType_(QuantLib::Option::Call) {}

    //! Constructor
    /*! \param value         The volatility value
        \param asof          The quote date
        \param name          The quote name
        \param quoteType     The quote type, should be RATE_LNVOL
        \param commodityName The name of the underlying commodity
        \param quoteCurrency The quote currency
        \param expiry        Expiry object defining the quote's expiry
        \param strike        Strike object defining the quote's strike
        \param optionType    The option type.
    */
    CommodityOptionQuote(QuantLib::Real value, const QuantLib::Date& asof, const std::string& name, QuoteType quoteType,
                         const std::string& commodityName, const std::string& quoteCurrency,
                         const QuantLib::ext::shared_ptr<Expiry>& expiry, const QuantLib::ext::shared_ptr<BaseStrike>& strike,
                         QuantLib::Option::Type optionType = QuantLib::Option::Call);

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CommodityOptionQuote>(quote_->value(), asofDate_, name_,
            quoteType_, commodityName_, quoteCurrency_, expiry_, strike_, optionType_);
    }

    //! \name Inspectors
    //@{
    const std::string& commodityName() const { return commodityName_; }
    const std::string& quoteCurrency() const { return quoteCurrency_; }
    const QuantLib::ext::shared_ptr<Expiry>& expiry() const { return expiry_; }
    const QuantLib::ext::shared_ptr<BaseStrike>& strike() const { return strike_; }
    QuantLib::Option::Type optionType() const { return optionType_; }
    //@}

private:
    std::string commodityName_;
    std::string quoteCurrency_;
    QuantLib::ext::shared_ptr<Expiry> expiry_;
    QuantLib::ext::shared_ptr<BaseStrike> strike_;
    QuantLib::Option::Type optionType_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Spread data class
/*! This class holds single market points of type SPREAD
    \ingroup marketdata
*/
class CorrelationQuote : public MarketDatum {
public:
    CorrelationQuote() {}
    //! Constructor
    /*! \param value         The correlation value
        \param asof          The quote date
        \param name          The quote name
        \param quoteType     The quote type, should be RATE or PRICE
        \param index1        The name of the first index
        \param index2        The name of the second index
        \param expiry        Expiry can be a period or a date
        \param strike        Can be underlying commodity price or ATM
    */
    CorrelationQuote(QuantLib::Real value, const QuantLib::Date& asof, const std::string& name, QuoteType quoteType,
                     const std::string& index1, const std::string& index2, const std::string& expiry,
                     const std::string& strike);

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CorrelationQuote>(quote_->value(), asofDate_, name_, quoteType_, index1_, index2_, expiry_, strike_);
    }

    //! \name Inspectors
    //@{
    const std::string& index1() const { return index1_; }
    const std::string& index2() const { return index2_; }
    const std::string& expiry() const { return expiry_; }
    const std::string& strike() const { return strike_; }
    //@}

private:
    std::string index1_;
    std::string index2_;
    std::string expiry_;
    std::string strike_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! CPR data class
/*!
This class holds single market points of type
- CPR
\ingroup marketdata
*/
class CPRQuote : public MarketDatum {
public:
    CPRQuote() {}
    //! Constructor
    CPRQuote(Real value, Date asofDate, const string& name, const string& securityId)
        : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::CPR), securityID_(securityId) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<CPRQuote>(quote_->value(), asofDate_, name_, securityID_);
    }

    //! \name Inspectors
    //@{
    const string& securityID() const { return securityID_; }
    //@}
private:
    string securityID_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Bond Price Quote
/*!
This class holds single market points of type
- Price
\ingroup marketdata
*/
class BondPriceQuote : public MarketDatum {
public:
    BondPriceQuote() {}
    //! Constructor
    BondPriceQuote(Real value, Date asofDate, const string& name, const string& securityId)
        : MarketDatum(value, asofDate, name, QuoteType::PRICE, InstrumentType::BOND), securityID_(securityId) {}

    //! Make a copy of the market datum
    QuantLib::ext::shared_ptr<MarketDatum> clone() override {
        return QuantLib::ext::make_shared<BondPriceQuote>(quote_->value(), asofDate_, name_, securityID_);
    }

    //! \name Inspectors
    //@{
    const string& securityID() const { return securityID_; }
    //@}
private:
    string securityID_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

//! Transition Probability data class
class TransitionProbabilityQuote : public MarketDatum {
public:
    TransitionProbabilityQuote() {}
    TransitionProbabilityQuote(Real value, Date asofDate, const string& name, const string& id,
                               const string& fromRating, const string& toRating)
        : MarketDatum(value, asofDate, name, QuoteType::TRANSITION_PROBABILITY, InstrumentType::RATING), id_(id),
          fromRating_(fromRating), toRating_(toRating) {}

    //! \name Inspectors
    //@{
    const string& id() const { return id_; }
    const string& fromRating() const { return fromRating_; }
    const string& toRating() const { return toRating_; }
    //@}
private:
    string id_;
    string fromRating_;
    string toRating_;
    //! Serialization
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int version);
};

} // namespace data
} // namespace ore

BOOST_CLASS_EXPORT_KEY(ore::data::MoneyMarketQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::FRAQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::ImmFraQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::SwapQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::ZeroQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::DiscountQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::MMFutureQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::OIFutureQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::BasisSwapQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::BMASwapQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CrossCcyBasisSwapQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CrossCcyFixFloatSwapQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CdsQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::HazardRateQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::RecoveryRateQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::SwaptionQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::SwaptionShiftQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::BondOptionQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::BondOptionShiftQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CapFloorQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CapFloorShiftQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::FXSpotQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::FXForwardQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::FXOptionQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::ZcInflationSwapQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::InflationCapFloorQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::ZcInflationCapFloorQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::YoYInflationSwapQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::YyInflationCapFloorQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::SeasonalityQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::EquitySpotQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::EquityForwardQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::EquityDividendYieldQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::EquityOptionQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::SecuritySpreadQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::BaseCorrelationQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::IndexCDSOptionQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CommoditySpotQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CommodityForwardQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CommodityOptionQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CorrelationQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::CPRQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::BondPriceQuote);
BOOST_CLASS_EXPORT_KEY(ore::data::TransitionProbabilityQuote);
