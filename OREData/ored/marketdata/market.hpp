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

#include <ql/experimental/credit/basecorrelationstructure.hpp>
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

#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/termstructures/correlationtermstructure.hpp>
#include <qle/termstructures/pricetermstructure.hpp>

namespace ore {
namespace data {
using namespace QuantLib;
using std::string;

typedef BaseCorrelationTermStructure<BilinearInterpolation> BilinearBaseCorrelationTermStructure;

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

//! Market
/*!
  Base class for central repositories containing all term structure objects
  needed in instrument pricing.

  \ingroup marketdata
*/
class Market {
public:
    //! Destructor
    virtual ~Market() {}

    //! Get the asof Date
    virtual Date asofDate() const = 0;

    //! \name Yield Curves
    //@{
    virtual Handle<YieldTermStructure> yieldCurve(const YieldCurveType& type, const string& name,
                                                  const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<YieldTermStructure>
    discountCurve(const string& ccy, const string& configuration = Market::defaultConfiguration) const = 0;
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
    swaptionVol(const string& ccy, const string& configuration = Market::defaultConfiguration) const = 0;
    virtual const string shortSwapIndexBase(const string& ccy,
                                            const string& configuration = Market::defaultConfiguration) const = 0;
    virtual const string swapIndexBase(const string& ccy,
                                       const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Yield volatilities
    //@{
    virtual Handle<SwaptionVolatilityStructure>
    yieldVol(const string& securityID, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Foreign Exchange
    //@{
    virtual Handle<Quote> fxSpot(const string& ccypair,
                                 const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<BlackVolTermStructure> fxVol(const string& ccypair,
                                                const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Default Curves and Recovery Rates
    //@{
    virtual Handle<DefaultProbabilityTermStructure>
    defaultCurve(const string&, const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<Quote> recoveryRate(const string&,
                                       const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name (Index) CDS Option volatilities
    //@{
    virtual Handle<BlackVolTermStructure> cdsVol(const string&,
                                                 const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Base Correlation term structures
    //@{
    virtual Handle<BilinearBaseCorrelationTermStructure>
    baseCorrelation(const string&, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Stripped Cap/Floor volatilities i.e. caplet/floorlet volatilities
    //@{
    virtual Handle<OptionletVolatilityStructure>
    capFloorVol(const string& ccy, const string& configuration = Market::defaultConfiguration) const = 0;
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
};
} // namespace data
} // namespace ore
