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

/*! \file ored/marketdata/marketimpl.hpp
    \brief An implementation of the Market class that stores the required objects in maps
    \ingroup marketdata
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/marketdata/market.hpp>

#include <qle/indexes/inflationindexobserver.hpp>

#include <map>

namespace ore {
namespace data {
using namespace QuantLib;
using ore::data::Convention;
using ore::data::Conventions;
using std::map;
using std::pair;
using std::string;
using std::tuple;

// TODO: rename class
//! Market Implementation
/*!
  The MarketImpl class differs from the Market base class in that it contains concrete maps
  of term structures, and it implements the interface.

  \ingroup marketdata
 */
class MarketImpl : public Market {
public:
    //! Default constructor
    MarketImpl() {}
    MarketImpl(const Conventions& conventions) : conventions_(conventions) {
        // if no fx spots are defined we still need an empty triangulation
        fxSpots_[Market::defaultConfiguration] = FXTriangulation();
    }

    //! \name Market interface
    //@{
    //! Get the asof Date
    Date asofDate() const { return asof_; }

    //! Yield Curves
    Handle<YieldTermStructure> yieldCurve(const YieldCurveType& type, const string& ccy,
                                          const string& configuration = Market::defaultConfiguration) const;
    Handle<YieldTermStructure> discountCurve(const string& ccy,
                                             const string& configuration = Market::defaultConfiguration) const;
    Handle<YieldTermStructure> yieldCurve(const string& name,
                                          const string& configuration = Market::defaultConfiguration) const;
    Handle<IborIndex> iborIndex(const string& indexName,
                                const string& configuration = Market::defaultConfiguration) const;
    Handle<SwapIndex> swapIndex(const string& indexName,
                                const string& configuration = Market::defaultConfiguration) const;

    //! Swaptions
    Handle<QuantLib::SwaptionVolatilityStructure>
    swaptionVol(const string& ccy, const string& configuration = Market::defaultConfiguration) const;
    const string shortSwapIndexBase(const string& ccy,
                                    const string& configuration = Market::defaultConfiguration) const;
    const string swapIndexBase(const string& ccy, const string& configuration = Market::defaultConfiguration) const;

    //! Yield volatility
    Handle<QuantLib::SwaptionVolatilityStructure>
    yieldVol(const string& securityID, const string& configuration = Market::defaultConfiguration) const;

    //! FX
    Handle<Quote> fxSpot(const string& ccypair, const string& configuration = Market::defaultConfiguration) const;
    Handle<BlackVolTermStructure> fxVol(const string& ccypair,
                                        const string& configuration = Market::defaultConfiguration) const;

    //! Default Curves and Recovery Rates
    Handle<DefaultProbabilityTermStructure>
    defaultCurve(const string&, const string& configuration = Market::defaultConfiguration) const;
    Handle<Quote> recoveryRate(const string&, const string& configuration = Market::defaultConfiguration) const;

    //! CDS volatilities
    Handle<BlackVolTermStructure> cdsVol(const string& name,
                                         const string& configuration = Market::defaultConfiguration) const;

    //! Base correlation structures
    Handle<BaseCorrelationTermStructure<BilinearInterpolation>>
    baseCorrelation(const string& name, const string& configuration = Market::defaultConfiguration) const;

    //! CapFloor volatilities
    Handle<OptionletVolatilityStructure> capFloorVol(const string& ccy,
                                                     const string& configuration = Market::defaultConfiguration) const;

    //! YoY Inflation CapFloor volatilities
    Handle<QuantExt::YoYOptionletVolatilitySurface>
    yoyCapFloorVol(const string& name, const string& configuration = Market::defaultConfiguration) const;

    //! Inflation Indexes
    virtual Handle<ZeroInflationIndex>
    zeroInflationIndex(const string& indexName, const string& configuration = Market::defaultConfiguration) const;
    virtual Handle<YoYInflationIndex>
    yoyInflationIndex(const string& indexName, const string& configuration = Market::defaultConfiguration) const;

    //! CPI Inflation Cap Floor Price Surfaces
    // virtual Handle<CPICapFloorTermPriceSurface>
    // cpiInflationCapFloorPriceSurface(const string& indexName,
    //                                  const string& configuration = Market::defaultConfiguration) const;

    //! Inflation Cap Floor Volatility Surfaces
    virtual Handle<CPIVolatilitySurface>
    cpiInflationCapFloorVolatilitySurface(const string& indexName,
                                          const string& configuration = Market::defaultConfiguration) const;

    //! YoY Inflation Cap Floor Price Surfaces
    // virtual Handle<YoYCapFloorTermPriceSurface>
    // yoyInflationCapFloorPriceSurface(const string& indexName,
    //                                  const string& configuration = Market::defaultConfiguration) const;

    //! Equity curves
    Handle<Quote> equitySpot(const string& eqName, const string& configuration = Market::defaultConfiguration) const;
    Handle<QuantExt::EquityIndex> equityCurve(const string& eqName,
                                              const string& configuration = Market::defaultConfiguration) const;

    Handle<YieldTermStructure> equityDividendCurve(const string& eqName,
                                                   const string& configuration = Market::defaultConfiguration) const;

    //! Equity volatilities
    Handle<BlackVolTermStructure> equityVol(const string& eqName,
                                            const string& configuration = Market::defaultConfiguration) const;

    //! Equity forecasting curves
    Handle<YieldTermStructure> equityForecastCurve(const string& eqName,
                                                   const string& configuration = Market::defaultConfiguration) const;

    //! Bond Spreads
    Handle<Quote> securitySpread(const string& securityID,
                                 const string& configuration = Market::defaultConfiguration) const;

    //! Cpi Base Quotes
    Handle<QuantExt::InflationIndexObserver> baseCpis(const string& index,
                                                      const string& configuration = Market::defaultConfiguration) const;

    //! Commodity curves
    QuantLib::Handle<QuantExt::PriceTermStructure>
    commodityPriceCurve(const string& commodityName, const string& configuration = Market::defaultConfiguration) const;

    QuantLib::Handle<QuantLib::BlackVolTermStructure>
    commodityVolatility(const string& commodityName, const string& configuration = Market::defaultConfiguration) const;
    //@}

    //! Correlation curves
    Handle<QuantExt::CorrelationTermStructure>
    correlationCurve(const string& index1, const string& index2,
                     const string& configuration = Market::defaultConfiguration) const;
    //! \name Conditional Prepayment Rates
    //@{
    QuantLib::Handle<Quote> cpr(const string& securityID,
                                const string& configuration = Market::defaultConfiguration) const;
    //@}

    //! \name Disable copying
    //@{
    MarketImpl(const MarketImpl&) = delete;
    MarketImpl& operator=(const MarketImpl&) = delete;
    //@}

    //! Send an explicit update() call to all term structures
    void refresh(const string& configuration = Market::defaultConfiguration);

protected:
    Date asof_;
    // maps (configuration, key) => term structure
    map<tuple<string, YieldCurveType, string>, Handle<YieldTermStructure>> yieldCurves_;
    map<pair<string, string>, Handle<IborIndex>> iborIndices_;
    map<pair<string, string>, Handle<SwapIndex>> swapIndices_;
    map<pair<string, string>, Handle<QuantLib::SwaptionVolatilityStructure>> swaptionCurves_;
    map<pair<string, string>, pair<string, string>> swaptionIndexBases_;
    map<pair<string, string>, Handle<QuantLib::SwaptionVolatilityStructure>> yieldVolCurves_;
    map<string, FXTriangulation> fxSpots_;
    mutable map<pair<string, string>, Handle<BlackVolTermStructure>> fxVols_;
    map<pair<string, string>, Handle<DefaultProbabilityTermStructure>> defaultCurves_;
    map<pair<string, string>, Handle<BlackVolTermStructure>> cdsVols_;
    map<pair<string, string>, Handle<BaseCorrelationTermStructure<BilinearInterpolation>>> baseCorrelations_;
    map<pair<string, string>, Handle<Quote>> recoveryRates_;
    map<pair<string, string>, Handle<OptionletVolatilityStructure>> capFloorCurves_;
    map<pair<string, string>, Handle<QuantExt::YoYOptionletVolatilitySurface>> yoyCapFloorVolSurfaces_;
    map<pair<string, string>, Handle<ZeroInflationIndex>> zeroInflationIndices_;
    map<pair<string, string>, Handle<YoYInflationIndex>> yoyInflationIndices_;
    map<pair<string, string>, Handle<CPIVolatilitySurface>> cpiInflationCapFloorVolatilitySurfaces_;
    map<pair<string, string>, Handle<Quote>> equitySpots_;
    map<pair<string, string>, Handle<BlackVolTermStructure>> equityVols_;
    map<pair<string, string>, Handle<Quote>> securitySpreads_;
    map<pair<string, string>, Handle<QuantExt::InflationIndexObserver>> baseCpis_;
    map<tuple<string, string, string>, Handle<QuantExt::CorrelationTermStructure>> correlationCurves_;
    map<pair<string, string>, QuantLib::Handle<QuantExt::PriceTermStructure>> commodityCurves_;
    map<pair<string, string>, QuantLib::Handle<QuantLib::BlackVolTermStructure>> commodityVols_;
    map<pair<string, string>, QuantLib::Handle<QuantExt::EquityIndex>> equityCurves_;
    map<pair<string, string>, Handle<Quote>> cprs_;
    Conventions conventions_;

    //! add a swap index to the market
    void addSwapIndex(const string& swapindex, const string& discountIndex,
                      const string& configuration = Market::defaultConfiguration);

    // set of term structure pointers for refresh (per configuration)
    map<string, std::set<boost::shared_ptr<TermStructure>>> refreshTs_;
};
} // namespace data
} // namespace ore
