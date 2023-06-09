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

/*! \file ored/marketdata/market.hpp
    \brief Base Market class
    \ingroup marketdata
*/

#pragma once


#include <ql/experimental/inflation/cpicapfloortermpricesurface.hpp>
#include <ql/experimental/inflation/yoycapfloortermpricesurface.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/date.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>

#include <qle/indexes/commodityindex.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>
#include <qle/termstructures/credit/basecorrelationstructure.hpp>
#include <qle/termstructures/creditcurve.hpp>
#include <qle/termstructures/creditvolcurve.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

namespace ore {
namespace data {
using namespace QuantLib;
using std::string;

enum class YieldCurveType {
    Discount = 0, // Chosen to match MarketObject::DiscountCurve
    Yield = 1,    // Chosen to match MarketObject::YieldCurve
    EquityDividend = 2
};

enum class MarketObject {
    DiscountCurve = 0,
    YieldCurve = 1,
    IndexCurve = 2,
    SwapIndexCurve = 3,
    FXSpot = 4,
    FXVol = 5,
    SwaptionVol = 6,
    DefaultCurve = 7,
    CDSVol = 8,
    BaseCorrelation = 9,
    CapFloorVol = 10,
    ZeroInflationCurve = 11,
    YoYInflationCurve = 12,
    ZeroInflationCapFloorVol = 13,
    YoYInflationCapFloorVol = 14,
    EquityCurve = 15,
    EquityVol = 16,
    Security = 17,
    CommodityCurve = 18,
    CommodityVolatility = 19,
    Correlation = 20,
    YieldVol = 21
};

//! Struct to store parameters for commodities to be treatred as pseudo currencies
struct PseudoCurrencyMarketParameters {
    bool treatAsFX;                  // flag to pass through to Pure FX
    string baseCurrency;             // pseudo currency base currency, typically USD
    std::map<string, string> curves; // map from pseudo currency to commodity curve, e.g. "XAU" -> "PM:XAUUSD", "BTC" -> "CRYPTO:BTCUSD"
    string fxIndexTag;               // tag for FX correlations
    Real defaultCorrelation;         // default correlation or Null<Real> if none is set
};

std::ostream& operator<<(std::ostream&, const struct PseudoCurrencyMarketParameters&);

//! Function to build parameters from PricingEngine GlobalParametrs.
/*! If no PricingEngine Global Parameters (PEGP) are provided the default params are returned which have treatAsFX =
 * true. If PEGP are present, we look for the following fields
 *
 *  name="PseudoCurrency.TreatAsFX" value = true or false
 *  name="PseudoCurrency.BaseCurrency" value = currency code
 *  name="PseudoCurrency.FXIndexTag" value = Tag name for FX indices, e.g. GENERIC means we request correlation for
 * "FX-GENERIC-USD-EUR" name="PseudeoCurrency.Curves.XXX" value = curve name, here XXX should be a 3 letter Precious metal or Crypto currency
 * code name="PseudoCurrency.DefaultCorrelation" value = correlation. This is optional, if present we use this when the
 * market has no correlation
 *
 *  A typical configuration is
 *  \<pre\>
 *    \<GlobalParameters\>
 *      \<Parameter name="PseudoCurrency.TreatAsFX"\>false\</Parameter\>
 *      \<Parameter name="PseudoCurrency.BaseCurrency"\>USD\</Parameter\>
 *      \<Parameter name="PseudoCurrency.FXIndexTag"\>GENERIC\</Parameter\>
 *      \<Parameter name="PseudoCurrency.Curve.XAU"\>PM:XAUUSD\</Parameter\>
 *      \<Parameter name="PseudoCurrency.Curve.XBT"\>CRYPTO:XBTUSD\</Parameter\>
 *    \</GlobalParameters\>
 *  \</pre\>
 */
struct PseudoCurrencyMarketParameters buildPseudoCurrencyMarketParameters(
    const std::map<string, string>& pricingEngineGlobalParameters = std::map<string, string>());

//! Singleton to store Global parameters, this should be initialised at some point with PEGP
class GlobalPseudoCurrencyMarketParameters
    : public Singleton<GlobalPseudoCurrencyMarketParameters, std::integral_constant<bool, true>> {
    friend class Singleton<GlobalPseudoCurrencyMarketParameters, std::integral_constant<bool, true>>;

public:
    const PseudoCurrencyMarketParameters& get() const {
        boost::shared_lock<boost::shared_mutex> lock(mutex_);
        return params_;
    }

    void set(const PseudoCurrencyMarketParameters& params) {
        boost::unique_lock<boost::shared_mutex> lock(mutex_);
        params_ = params;
    }
    void set(const std::map<string, string>& pegp) {
        boost::unique_lock<boost::shared_mutex> lock(mutex_);
        params_ = buildPseudoCurrencyMarketParameters(pegp);
    }

private:
    GlobalPseudoCurrencyMarketParameters() {
        // default behaviour is from above
        params_ = buildPseudoCurrencyMarketParameters();
    };

    PseudoCurrencyMarketParameters params_;
    mutable boost::shared_mutex mutex_;
};
    
//! Market
/*!
  Base class for central repositories containing all term structure objects
  needed in instrument pricing.

  \ingroup marketdata
*/
class Market {
public:
    //! Constructor
    explicit Market(const bool handlePseudoCurrencies) : handlePseudoCurrencies_(handlePseudoCurrencies) {}

    //! Destructor
    virtual ~Market() {}

    //! Get the asof Date
    virtual Date asofDate() const = 0;

    //! \name Yield Curves
    //@{
    virtual Handle<YieldTermStructure> yieldCurve(const YieldCurveType& type, const string& name,
                                                  const string& configuration = Market::defaultConfiguration) const = 0;
    Handle<YieldTermStructure> discountCurve(const string& ccy,
                                             const string& configuration = Market::defaultConfiguration) const;
    virtual Handle<YieldTermStructure>
    discountCurveImpl(const string& ccy, const string& configuration = Market::defaultConfiguration) const = 0;

    virtual Handle<YieldTermStructure> yieldCurve(const string& name,
                                                  const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<IborIndex> iborIndex(const string& indexName,
                                        const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<SwapIndex> swapIndex(const string& indexName,
                                        const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Swaptions
    //@{
    virtual Handle<SwaptionVolatilityStructure>
    swaptionVol(const string& key, const string& configuration = Market::defaultConfiguration) const = 0;
    virtual string shortSwapIndexBase(const string& key,
                                            const string& configuration = Market::defaultConfiguration) const = 0;
    virtual string swapIndexBase(const string& key,
                                       const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Yield volatilities
    //@{
    virtual Handle<SwaptionVolatilityStructure>
    yieldVol(const string& securityID, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Foreign Exchange
    //@{
    QuantLib::Handle<QuantExt::FxIndex> fxIndex(const string& fxIndex,
                                                const string& configuration = Market::defaultConfiguration) const;
    virtual QuantLib::Handle<QuantExt::FxIndex>
    fxIndexImpl(const string& fxIndex, const string& configuration = Market::defaultConfiguration) const = 0;
    // Fx Rate is the fx rate as of today
    Handle<Quote> fxRate(const string& ccypair, const string& configuration = Market::defaultConfiguration) const;
    virtual Handle<Quote> fxRateImpl(const string& ccypair,
                                     const string& configuration = Market::defaultConfiguration) const = 0;
    // Fx Spot is the spot rate quoted in the market
    Handle<Quote> fxSpot(const string& ccypair, const string& configuration = Market::defaultConfiguration) const;
    virtual Handle<Quote> fxSpotImpl(const string& ccypair,
                                     const string& configuration = Market::defaultConfiguration) const = 0;
    Handle<BlackVolTermStructure> fxVol(const string& ccypair,
                                        const string& configuration = Market::defaultConfiguration) const;
    virtual Handle<BlackVolTermStructure>
    fxVolImpl(const string& ccypair, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Default Curves and Recovery Rates
    //@{
    virtual Handle<QuantExt::CreditCurve>
    defaultCurve(const string&, const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<Quote> recoveryRate(const string&,
                                       const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name (Index) CDS Option volatilities
    //@{
    virtual Handle<QuantExt::CreditVolCurve>
    cdsVol(const string&, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Base Correlation term structures
    //@{
    virtual Handle<QuantExt::BaseCorrelationTermStructure>
    baseCorrelation(const string&, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Stripped Cap/Floor volatilities i.e. caplet/floorlet volatilities
    //@{
    virtual Handle<OptionletVolatilityStructure>
    capFloorVol(const string& key, const string& configuration = Market::defaultConfiguration) const = 0;
    // get - ibor index name (might be empty = unspecified) and
    //     - rate computation period for OIS indices (might be 0*Days = unspecified)
    virtual std::pair<std::string, QuantLib::Period>
    capFloorVolIndexBase(const string& key, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Stripped YoY Inflation Cap/Floor volatilities i.e. caplet/floorlet volatilities
    //@{
    virtual Handle<QuantExt::YoYOptionletVolatilitySurface>
    yoyCapFloorVol(const string& indexName, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! Inflation Indexes
    virtual Handle<ZeroInflationIndex>
    zeroInflationIndex(const string& indexName, const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<YoYInflationIndex>
    yoyInflationIndex(const string& indexName, const string& configuration = Market::defaultConfiguration) const = 0;

    //! CPI Inflation Cap Floor Volatility Surfaces
    virtual Handle<CPIVolatilitySurface>
    cpiInflationCapFloorVolatilitySurface(const string& indexName,
                                          const string& configuration = Market::defaultConfiguration) const = 0;

    //! \name Equity curves
    //@{
    virtual Handle<Quote> equitySpot(const string& eqName,
                                     const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<YieldTermStructure>
    equityDividendCurve(const string& eqName, const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<YieldTermStructure>
    equityForecastCurve(const string& eqName, const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<QuantExt::EquityIndex>
    equityCurve(const string& eqName, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Equity volatilities
    //@{
    virtual Handle<BlackVolTermStructure>
    equityVol(const string& eqName, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! Refresh term structures for a given configuration
    virtual void refresh(const string&) {}

    //! Default configuration label
    static const string defaultConfiguration;

    //! InCcy configuration label
    static const string inCcyConfiguration;

    //! \name BondSpreads
    //@{
    virtual Handle<Quote> securitySpread(const string& securityID,
                                         const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Commodity price curves and indices
    //@{
    virtual QuantLib::Handle<QuantExt::PriceTermStructure>
    commodityPriceCurve(const std::string& commodityName,
                        const std::string& configuration = Market::defaultConfiguration) const = 0;

    virtual QuantLib::Handle<QuantExt::CommodityIndex> commodityIndex(const std::string& commodityName,
        const std::string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Commodity volatility
    //@{
    virtual QuantLib::Handle<QuantLib::BlackVolTermStructure>
    commodityVolatility(const std::string& commodityName,
                        const std::string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Correlation
    //@{
    virtual QuantLib::Handle<QuantExt::CorrelationTermStructure>
    correlationCurve(const std::string& index1, const std::string& index2,
                     const std::string& configuration = Market::defaultConfiguration) const = 0;
    //@}
    //! \name Conditional Prepayment Rates
    //@{
    virtual Handle<Quote> cpr(const string& securityID,
                              const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    // public utility
    string commodityCurveLookup(const string& pm) const;

    bool handlePseudoCurrencies() const { return handlePseudoCurrencies_; }

protected:
    bool handlePseudoCurrencies_ = false;

private:
    // caches for the market data objects
    mutable std::map<string, Handle<Quote>> spotCache_;
    mutable std::map<string, Handle<BlackVolTermStructure>> volCache_;
    mutable std::map<string, Handle<YieldTermStructure>> discountCurveCache_;

    mutable std::map<string, Handle<Quote>> fxRateCache_;
    mutable std::map<std::pair<string, string>, QuantLib::Handle<QuantExt::FxIndex>> fxIndicesCache_;

    // private utilities
    Handle<Quote> getFxBaseQuote(const string& ccy, const string& config) const;
    Handle<Quote> getFxSpotBaseQuote(const string& ccy, const string& config) const;
    Handle<BlackVolTermStructure> getVolatility(const string& ccy, const string& config) const;
    string getCorrelationIndexName(const string& ccy) const;
};
} // namespace data
} // namespace ore
