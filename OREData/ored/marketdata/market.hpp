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
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/defaulttermstructure.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/date.hpp>

using namespace QuantLib;
using std::string;

typedef BaseCorrelationTermStructure<BilinearInterpolation> BilinearBaseCorrelationTermStructure;

namespace ore {
namespace data {

enum class YieldCurveType {
    Discount = 0, // Chosen to match MarketObject::DiscountCurve
    Yield = 1,    // Chosen to match MarketObject::YieldCurve
    EquityDividend = 2,
    EquityForecast = 3
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

    //! Inflation Indexes
    virtual Handle<ZeroInflationIndex>
    zeroInflationIndex(const string& indexName, const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<YoYInflationIndex>
    yoyInflationIndex(const string& indexName, const string& configuration = Market::defaultConfiguration) const = 0;

    //! Inflation Cap Floor Price Surfaces
    virtual Handle<CPICapFloorTermPriceSurface>
    inflationCapFloorPriceSurface(const string& indexName,
                                  const string& configuration = Market::defaultConfiguration) const = 0;

    //! \name Equity curves
    //@{
    virtual Handle<Quote> equitySpot(const string& eqName,
                                     const string& configuration = Market::defaultConfiguration) const = 0;
    virtual Handle<YieldTermStructure>
    equityDividendCurve(const string& eqName, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Equity volatilities
    //@{
    virtual Handle<BlackVolTermStructure>
    equityVol(const string& eqName, const string& configuration = Market::defaultConfiguration) const = 0;
    //@}

    //! \name Equity volatilities
    //@{
    virtual Handle<YieldTermStructure>
    equityForecastCurve(const string& eqName, const string& configuration = Market::defaultConfiguration) const = 0;
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
};
} // namespace data
} // namespace ore
