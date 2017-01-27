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

#include <string>
#include <ql/types.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/quotes/simplequote.hpp>
#include <boost/make_shared.hpp>

using std::string;
using QuantLib::Real;
using QuantLib::Date;
using QuantLib::Period;
using QuantLib::Quote;
using QuantLib::SimpleQuote;
using QuantLib::Handle;
using QuantLib::DayCounter;
using QuantLib::Natural;
using QuantLib::Month;
using QuantLib::Months;

namespace ore {
namespace data {

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
    //! Supported market instrument types
    enum class InstrumentType {
        ZERO,
        DISCOUNT,
        MM,
        MM_FUTURE,
        FRA,
        IR_SWAP,
        BASIS_SWAP,
        CC_BASIS_SWAP,
        CDS,
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
        SEASONALITY
    };

    //! Supported market quote types
    enum class QuoteType {
        BASIS_SPREAD,
        CREDIT_SPREAD,
        YIELD_SPREAD,
        HAZARD_RATE,
        RATE,
        RATIO,
        PRICE,
        RATE_LNVOL,
        RATE_NVOL,
        RATE_SLNVOL,
        SHIFT
    };

    //! Constructor
    MarketDatum(Real value, Date asofDate, const string& name, QuoteType quoteType, InstrumentType instrumentType)
        : quote_(boost::make_shared<SimpleQuote>(value)), asofDate_(asofDate), name_(name),
          instrumentType_(instrumentType), quoteType_(quoteType) {}

    //! Default destructor
    virtual ~MarketDatum() {}

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
};

//! Money market data class
/*!
  This class holds single market points of type
  - MM

  Specific data comprise currency, fwdStart, term

  \ingroup marketdata
*/
class MoneyMarketQuote : public MarketDatum {
public:
    //! Constructor
    MoneyMarketQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period fwdStart,
                     Period term)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::MM), ccy_(ccy), fwdStart_(fwdStart),
          term_(term) {}
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
    //! Constructor
    FRAQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period fwdStart,
             Period term)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::FRA), ccy_(ccy), fwdStart_(fwdStart),
          term_(term) {}

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
};

//! Swap market data class
/*!
  This class holds single market points of type
  - IR_SWAP

  Specific data comprise currency, fwdStart, tenor, term

  \ingroup marketdata
*/
class SwapQuote : public MarketDatum {
public:
    //! Constructor
    SwapQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period fwdStart,
              Period term, Period tenor)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::IR_SWAP), ccy_(ccy), fwdStart_(fwdStart),
          term_(term), tenor_(tenor) {}

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& fwdStart() const { return fwdStart_; }
    const Period& term() const { return term_; }
    const Period& tenor() const { return tenor_; }
    //@}
private:
    string ccy_;
    Period fwdStart_;
    Period term_;
    Period tenor_;
};

//! Zero market data class
/*!
  This class holds single market points of type
  - ZERO.
  Specific data comprise currency, date and day counter.

  Zero rates are hardly quoted in the market, but derived from quoted
  yields such as deposits, swaps, as well as futures prices.
  This data type is included here nevertheless
  to enable consistency checks between Wrap and reference systems.

  \ingroup marketdata
*/
class ZeroQuote : public MarketDatum {
public:
    //! Constructor
    ZeroQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, const string& ccy, Date date,
              DayCounter dayCounter, Period tenor = Period())
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::ZERO), ccy_(ccy), date_(date),
          dayCounter_(dayCounter), tenor_(tenor) {
        // Minimal adjustment in the absence of a calendar
        QL_REQUIRE(date_ != Date() || tenor != Period(), "ZeroQuote: either date or period is required");
        tenorBased_ = (date_ == Date());
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
    //! Constructor
    DiscountQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Date date)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::DISCOUNT), ccy_(ccy), date_(date) {}

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    Date date() const { return date_; }
    //@}
private:
    string ccy_;
    Date date_;
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
    //! Constructor
    MMFutureQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, string expiry,
                  string contract = "", Period tenor = 3 * Months)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::MM_FUTURE), ccy_(ccy), expiry_(expiry),
          contract_(contract), tenor_(tenor) {}

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
    //! Constructor
    BasisSwapQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, Period flatTerm, Period term,
                   string ccy = "USD", Period maturity = 3 * Months)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::BASIS_SWAP), flatTerm_(flatTerm), term_(term),
          ccy_(ccy), maturity_(maturity) {}

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

  \ingroup marketdata‚
*/
class CrossCcyBasisSwapQuote : public MarketDatum {
public:
    //! Constructor
    CrossCcyBasisSwapQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string flatCcy,
                           Period flatTerm, string ccy, Period term, Period maturity = 3 * Months)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::CC_BASIS_SWAP), flatCcy_(flatCcy),
          flatTerm_(flatTerm), ccy_(ccy), term_(term), maturity_(maturity) {}

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
};

//! CDS Spread data class
/*!
  This class holds single market points of type
  - CREDIT_SPREAD

  \ingroup marketdata
*/
class CdsSpreadQuote : public MarketDatum {
public:
    //! COnstructor
    CdsSpreadQuote(Real value, Date asofDate, const string& name, const string& underlyingName, const string& seniority,
                   const string& ccy, Period term)
        : MarketDatum(value, asofDate, name, QuoteType::CREDIT_SPREAD, InstrumentType::CDS),
          underlyingName_(underlyingName), seniority_(seniority), ccy_(ccy), term_(term) {}

    //! \name Inspectors
    //@{
    const Period& term() const { return term_; }
    const string& seniority() const { return seniority_; }
    const string& ccy() const { return ccy_; }
    const string& underlyingName() const { return underlyingName_; }
    //@}
private:
    string underlyingName_;
    string seniority_;
    string ccy_;
    Period term_;
};

//! Hazard rate data class
/*!
  This class holds single market points of type
  - HAZARD_RATE

  \ingroup marketdata
*/
class HazardRateQuote : public MarketDatum {
public:
    //! Constructor
    HazardRateQuote(Real value, Date asofDate, const string& name, const string& underlyingName,
                    const string& seniority, const string& ccy, Period term)
        : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::HAZARD_RATE),
          underlyingName_(underlyingName), seniority_(seniority), ccy_(ccy), term_(term) {}

    //! \name Inspectors
    //@{
    const Period& term() const { return term_; }
    const string& seniority() const { return seniority_; }
    const string& ccy() const { return ccy_; }
    const string& underlyingName() const { return underlyingName_; }
    //@}
private:
    string underlyingName_;
    string seniority_;
    string ccy_;
    Period term_;
};

//! Recovery rate data class
/*!
  This class holds single market points of type
  - RECOVERY_RATE
  \ingroup marketdata
*/
class RecoveryRateQuote : public MarketDatum {
public:
    //! Constructor
    RecoveryRateQuote(Real value, Date asofDate, const string& name, const string& underlyingName,
                      const string& seniority, const string& ccy)
        : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::RECOVERY_RATE),
          underlyingName_(underlyingName), seniority_(seniority), ccy_(ccy) {}

    //! \name Inspectors
    //@{
    const string& seniority() const { return seniority_; }
    const string& ccy() const { return ccy_; }
    const string& underlyingName() const { return underlyingName_; }
    //@}
private:
    string underlyingName_;
    string seniority_;
    string ccy_;
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
    //! Constructor
    SwaptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period expiry,
                  Period term, string dimension, Real strike = 0.0)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::SWAPTION), ccy_(ccy), expiry_(expiry),
          term_(term), dimension_(dimension), strike_(strike) {}

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& expiry() const { return expiry_; }
    const Period& term() const { return term_; }
    const string& dimension() const { return dimension_; }
    Real strike() { return strike_; }
    //@}
private:
    string ccy_;
    Period expiry_;
    Period term_;
    string dimension_;
    Real strike_;
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
    //! Constructor
    SwaptionShiftQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period term)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::SWAPTION), ccy_(ccy), term_(term) {
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::SHIFT, "quote type must be SHIFT for shift data");
    }

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& expiry() const { return expiry_; }
    const Period& term() const { return term_; }
    //@}
private:
    string ccy_;
    Period expiry_;
    Period term_;
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
    //! Constructor
    CapFloorQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string ccy, Period term,
                  Period underlying, bool atm, bool relative, Real strike = 0.0)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::CAPFLOOR), ccy_(ccy), term_(term),
          underlying_(underlying), atm_(atm), relative_(relative), strike_(strike) {}

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const Period& term() const { return term_; }
    const Period& underlying() const { return underlying_; }
    bool atm() const { return atm_; }
    bool relative() const { return relative_; }
    Real strike() { return strike_; }
    //@}
private:
    string ccy_;
    Period term_;
    Period underlying_;
    bool atm_;
    bool relative_;
    Real strike_;
};

//! Shift data class (for SLN cap/floor volatilities)
/*! This class holds, for a given currency and index tenor, single market points of type
    - SHIFT
    \ingroup marketdata
*/
class CapFloorShiftQuote : public MarketDatum {
public:
    CapFloorShiftQuote(Real value, const Date& asofDate, const string& name, QuoteType quoteType, const string& ccy,
                       const Period& indexTenor)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::CAPFLOOR), ccy_(ccy), indexTenor_(indexTenor) {
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::SHIFT, "Quote type must be SHIFT for shift data");
    }

    const string& ccy() const { return ccy_; }
    const Period& indexTenor() const { return indexTenor_; }

private:
    string ccy_;
    Period indexTenor_;
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
    //! Constructor
    FXSpotQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string unitCcy, string ccy)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::FX_SPOT), unitCcy_(unitCcy), ccy_(ccy) {}

    //! \name Inspectors
    //@{
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    //@}
private:
    string unitCcy_;
    string ccy_;
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
    //! Constructor
    FXForwardQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string unitCcy, string ccy,
                   const Period& term, Real conversionFactor = 1.0)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::FX_FWD), unitCcy_(unitCcy), ccy_(ccy),
          term_(term), conversionFactor_(conversionFactor) {}

    //! \name Inspectors
    //@{
    const string& unitCcy() const { return unitCcy_; }
    const string& ccy() const { return ccy_; }
    const Period& term() const { return term_; }
    Real conversionFactor() const { return conversionFactor_; }
    //@}
private:
    string unitCcy_;
    string ccy_;
    Period term_;
    Real conversionFactor_;
};

//! FX Option data class
/*!
  This class holds single market points of type
  - FX_OPTION
  Specific data comprise
  - unit currency
  - currency
  - expiry
  - "strike" (25 delta butterfly "25BF", 25 delta risk reversal "25RR", atm straddle ATM)
  we do not yet support ATMF or individual delta put/call quotes.

  \ingroup marketdata
*/
class FXOptionQuote : public MarketDatum {
public:
    //! Constructor
    FXOptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, string unitCcy, string ccy,
                  Period expiry, string strike)
        : MarketDatum(value, asofDate, name, quoteType, InstrumentType::FX_OPTION), unitCcy_(unitCcy), ccy_(ccy),
          expiry_(expiry), strike_(strike) {
        QL_REQUIRE(strike == "ATM" || strike == "25BF" || strike == "25RR", "Invalid FXOptionQuote strike (" << strike
                                                                                                             << ")");
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
    ZcInflationSwapQuote(Real value, Date asofDate, const string& name, const string& index, Period term)
    : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::ZC_INFLATIONSWAP), index_(index),
    term_(term) {}
    string index() { return index_; }
    Period term() { return term_; }

private:
    string index_;
    Period term_;
};

//! ZC Cap Floor data class
/*!
 This class holds single market points of type
 - ZC_INFLATION_CAPFLOOR
 Specific data comprise type (can be price or nvol or slnvol),
 index, term, cap/floor, strike

 \ingroup marketdata
 */
class ZcInflationCapFloorQuote : public MarketDatum {
public:
    ZcInflationCapFloorQuote(Real value, Date asofDate, const string& name, QuoteType quoteType, const string& index,
                             Period term, bool isCap, const string& strike)
    : MarketDatum(value, asofDate, name, quoteType, InstrumentType::ZC_INFLATIONCAPFLOOR), index_(index),
    term_(term), isCap_(isCap), strike_(strike) {}
    string index() { return index_; }
    Period term() { return term_; }
    bool isCap() { return isCap_; }
    string strike() { return strike_; }

private:
    string index_;
    Period term_;
    bool isCap_;
    string strike_;
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
    ZcInflationSwapQuote(Real value, Date asofDate, const string& name, const string& index, Period term)
    : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::YoY_INFLATIONSWAP), index_(index),
    term_(term) {}
    string index() { return index_; }
    Period term() { return term_; }

private:
    string index_;
    Period term_;
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
    SeasonalityQuote(Real value, Date asofDate, const string& name, const string& index, const string& type, const string& month)
    : MarketDatum(value, asofDate, name, QuoteType::RATE, InstrumentType::SEASONALITY), index_(index), type_(type), month_(month) {}
    string index() { return index_; }
    string type() { return type_; }
    string month() { return month_; }
    int applyMonth() const;

private:
    string index_;
    string type_;
    string month_;
};
}
}
