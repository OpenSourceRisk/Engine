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
#include <qle/indexes/fxindex.hpp>

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
    explicit MarketImpl(const bool handlePseudoCurrencies) : Market(handlePseudoCurrencies) {}

    //! \name Market interface
    //@{
    //! Get the asof Date
    Date asofDate() const override { return asof_; }

    //! Yield Curves
    Handle<YieldTermStructure> yieldCurve(const YieldCurveType& type, const string& ccy,
                                          const string& configuration = Market::defaultConfiguration) const override;
    Handle<YieldTermStructure> discountCurveImpl(const string& ccy,
                                             const string& configuration = Market::defaultConfiguration) const override;
    Handle<YieldTermStructure> yieldCurve(const string& name,
                                          const string& configuration = Market::defaultConfiguration) const override;
    Handle<IborIndex> iborIndex(const string& indexName,
                                const string& configuration = Market::defaultConfiguration) const override;
    Handle<SwapIndex> swapIndex(const string& indexName,
                                const string& configuration = Market::defaultConfiguration) const override;

    //! Swaptions
    Handle<QuantLib::SwaptionVolatilityStructure>
    swaptionVol(const string& key, const string& configuration = Market::defaultConfiguration) const override;
    string shortSwapIndexBase(const string& key,
                                    const string& configuration = Market::defaultConfiguration) const override;
    string swapIndexBase(const string& key, const string& configuration = Market::defaultConfiguration) const override;

    //! Yield volatility
    Handle<QuantLib::SwaptionVolatilityStructure>
    yieldVol(const string& securityID, const string& configuration = Market::defaultConfiguration) const override;

    //! FX
    QuantLib::Handle<QuantExt::FxIndex>
    fxIndexImpl(const string& fxIndex, const string& configuration = Market::defaultConfiguration) const override;
    Handle<Quote> fxRateImpl(const string& ccypair,
                             const string& configuration = Market::defaultConfiguration) const override;
    Handle<Quote> fxSpotImpl(const string& ccypair,
                             const string& configuration = Market::defaultConfiguration) const override;
    Handle<BlackVolTermStructure> fxVolImpl(const string& ccypair,
                                            const string& configuration = Market::defaultConfiguration) const override;

    //! Default Curves and Recovery Rates
    Handle<QuantExt::CreditCurve> defaultCurve(const string&,
                                  const string& configuration = Market::defaultConfiguration) const override;
    Handle<Quote> recoveryRate(const string&, const string& configuration = Market::defaultConfiguration) const override;

    //! CDS volatilities
    Handle<QuantExt::CreditVolCurve> cdsVol(const string& name,
                                            const string& configuration = Market::defaultConfiguration) const override;

    //! Base correlation structures
    Handle<QuantExt::BaseCorrelationTermStructure>
    baseCorrelation(const string& name, const string& configuration = Market::defaultConfiguration) const override;

    //! CapFloor volatilities
    Handle<OptionletVolatilityStructure> capFloorVol(const string& key,
                                                     const string& configuration = Market::defaultConfiguration) const override;
    std::pair<string, QuantLib::Period>
    capFloorVolIndexBase(const string& key, const string& configuration = Market::defaultConfiguration) const override;

    //! YoY Inflation CapFloor volatilities
    Handle<QuantExt::YoYOptionletVolatilitySurface>
    yoyCapFloorVol(const string& name, const string& configuration = Market::defaultConfiguration) const override;

    //! Inflation Indexes
    virtual Handle<ZeroInflationIndex>
    zeroInflationIndex(const string& indexName, const string& configuration = Market::defaultConfiguration) const override;
    virtual Handle<YoYInflationIndex>
    yoyInflationIndex(const string& indexName, const string& configuration = Market::defaultConfiguration) const override;

    //! Inflation Cap Floor Volatility Surfaces
    virtual Handle<CPIVolatilitySurface>
    cpiInflationCapFloorVolatilitySurface(const string& indexName,
                                          const string& configuration = Market::defaultConfiguration) const override;

    //! Equity curves
    Handle<Quote> equitySpot(const string& eqName, const string& configuration = Market::defaultConfiguration) const override;
    Handle<QuantExt::EquityIndex2> equityCurve(const string& eqName,
                                              const string& configuration = Market::defaultConfiguration) const override;

    Handle<YieldTermStructure> equityDividendCurve(const string& eqName,
                                                   const string& configuration = Market::defaultConfiguration) const override;

    //! Equity volatilities
    Handle<BlackVolTermStructure> equityVol(const string& eqName,
                                            const string& configuration = Market::defaultConfiguration) const override;

    //! Equity forecasting curves
    Handle<YieldTermStructure> equityForecastCurve(const string& eqName,
                                                   const string& configuration = Market::defaultConfiguration) const override;

    //! Bond Spreads
    Handle<Quote> securitySpread(const string& securityID,
                                 const string& configuration = Market::defaultConfiguration) const override;

    //! Cpi Base Quotes
    Handle<QuantExt::InflationIndexObserver> baseCpis(const string& index,
                                                      const string& configuration = Market::defaultConfiguration) const;

    //! Commodity curves
    QuantLib::Handle<QuantExt::PriceTermStructure>
    commodityPriceCurve(const string& commodityName, const string& configuration = Market::defaultConfiguration) const override;

    //! Commodity index
    QuantLib::Handle<QuantExt::CommodityIndex> commodityIndex(const std::string& commodityName,
        const std::string& configuration = Market::defaultConfiguration) const override;

    //! Commodity volatility
    QuantLib::Handle<QuantLib::BlackVolTermStructure>
    commodityVolatility(const string& commodityName, const string& configuration = Market::defaultConfiguration) const override;
    //@}

    //! Correlation curves
    Handle<QuantExt::CorrelationTermStructure>
    correlationCurve(const string& index1, const string& index2,
                     const string& configuration = Market::defaultConfiguration) const override;
    //! \name Conditional Prepayment Rates
    //@{
    QuantLib::Handle<Quote> cpr(const string& securityID,
                                const string& configuration = Market::defaultConfiguration) const override;
    //@}

    //! \name Disable copying
    //@{
    MarketImpl(const MarketImpl&) = delete;
    MarketImpl& operator=(const MarketImpl&) = delete;
    //@}

    //! Send an explicit update() call to all term structures
    void refresh(const string& configuration = Market::defaultConfiguration) override;

protected:
    /*! Require a market object, this can be used in derived classes to build objects lazily. If the
        method is not overwritten in a derived class, it is assumed that the class builds all market
        object upfront.

        For FXVols and Correlations the require is not 'hard', e.g. both EURUSD and USDEUR might
        be required for FXVols, but only one of them is expected to be actually built (the other
        one is then constructed on the fly from the first one). Therefore no error should be thrown
        in the implementation of require(), if an object is ultimately not found, an appropriate error
        will be thrown from this class.

        An object is required for a single configuration. If it can't be built for this configuration,
        it should be tried to be built for the "default" configuration instead, because this is used
        as a fallback.

        Notice that correlation curves are required with '&' as a delimiter between the indexes. */
    virtual void require(const MarketObject o, const string& name, const string& configuration,
                         const bool forceBuild = false) const {}
    
    Date asof_;
    // fx quotes / indices, this is shared between all configurations
    QuantLib::ext::shared_ptr<FXTriangulation> fx_;
    // maps (configuration, key) => term structure
    mutable map<tuple<string, YieldCurveType, string>, Handle<YieldTermStructure>> yieldCurves_;
    mutable map<pair<string, string>, Handle<IborIndex>> iborIndices_;
    mutable map<pair<string, string>, Handle<SwapIndex>> swapIndices_;
    mutable map<pair<string, string>, Handle<QuantLib::SwaptionVolatilityStructure>> swaptionCurves_;
    mutable map<pair<string, string>, pair<string, string>> swaptionIndexBases_;
    mutable map<pair<string, string>, Handle<QuantLib::SwaptionVolatilityStructure>> yieldVolCurves_;
    mutable map<pair<string, string>, Handle<BlackVolTermStructure>> fxVols_;
    mutable map<pair<string, string>, Handle<QuantExt::CreditCurve>> defaultCurves_;
    mutable map<pair<string, string>, Handle<QuantExt::CreditVolCurve>> cdsVols_;
    mutable map<pair<string, string>, Handle<QuantExt::BaseCorrelationTermStructure>> baseCorrelations_;
    mutable map<pair<string, string>, Handle<Quote>> recoveryRates_;
    mutable map<pair<string, string>, Handle<OptionletVolatilityStructure>> capFloorCurves_;
    mutable map<pair<string, string>, std::pair<string, QuantLib::Period>> capFloorIndexBase_;
    mutable map<pair<string, string>, Handle<YoYOptionletVolatilitySurface>> yoyCapFloorVolSurfaces_;
    mutable map<pair<string, string>, Handle<ZeroInflationIndex>> zeroInflationIndices_;
    mutable map<pair<string, string>, Handle<YoYInflationIndex>> yoyInflationIndices_;
    mutable map<pair<string, string>, Handle<CPIVolatilitySurface>> cpiInflationCapFloorVolatilitySurfaces_;
    mutable map<pair<string, string>, Handle<Quote>> equitySpots_;
    mutable map<pair<string, string>, Handle<BlackVolTermStructure>> equityVols_;
    mutable map<pair<string, string>, Handle<Quote>> securitySpreads_;
    mutable map<pair<string, string>, Handle<QuantExt::InflationIndexObserver>> baseCpis_;
    mutable map<tuple<string, string, string>, Handle<QuantExt::CorrelationTermStructure>> correlationCurves_;
    mutable map<pair<string, string>, QuantLib::Handle<QuantExt::CommodityIndex>> commodityIndices_;
    mutable map<pair<string, string>, QuantLib::Handle<QuantLib::BlackVolTermStructure>> commodityVols_;
    mutable map<pair<string, string>, QuantLib::Handle<QuantExt::EquityIndex2>> equityCurves_;
    mutable map<pair<string, string>, Handle<Quote>> cprs_;

    //! add a swap index to the market
    void addSwapIndex(const string& swapindex, const string& discountIndex,
                      const string& configuration = Market::defaultConfiguration) const;

    // set of term structure pointers for refresh (per configuration)
    map<string, std::set<QuantLib::ext::shared_ptr<TermStructure>>> refreshTs_;

private:
    pair<string, string> swapIndexBases(const string& key,
                                        const string& configuration = Market::defaultConfiguration) const;
};

} // namespace data
} // namespace ore
