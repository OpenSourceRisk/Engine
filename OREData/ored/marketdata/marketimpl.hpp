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

    //! Constructor taking a pointer to conventions
    MarketImpl(const boost::shared_ptr<Conventions>& conventions)
        : conventions_ref_(conventions), conventions_(*conventions_ref_) {
        initialise();
    }

    /*! Constructor taking a reference to conventions. This ctor is deprecated.
        TODO remove this ctor, remove the conventions member variable holding the copy. */
    MarketImpl(const Conventions& conventions) : conventions_(conventions) { initialise(); }

    void initialise() {
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

    //! Inflation Cap Floor Volatility Surfaces
    virtual Handle<CPIVolatilitySurface>
    cpiInflationCapFloorVolatilitySurface(const string& indexName,
                                          const string& configuration = Market::defaultConfiguration) const;

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

    //! Commodity index
    QuantLib::Handle<QuantExt::CommodityIndex> commodityIndex(const std::string& commodityName,
        const std::string& configuration = Market::defaultConfiguration) const;

    //! Commodity volatility
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
    virtual void require(const MarketObject o, const string& name, const string& configuration) const {}

    Date asof_;
    // maps (configuration, key) => term structure
    mutable map<tuple<string, YieldCurveType, string>, Handle<YieldTermStructure>> yieldCurves_;
    mutable map<pair<string, string>, Handle<IborIndex>> iborIndices_;
    mutable map<pair<string, string>, Handle<SwapIndex>> swapIndices_;
    mutable map<pair<string, string>, Handle<QuantLib::SwaptionVolatilityStructure>> swaptionCurves_;
    mutable map<pair<string, string>, pair<string, string>> swaptionIndexBases_;
    mutable map<pair<string, string>, Handle<QuantLib::SwaptionVolatilityStructure>> yieldVolCurves_;
    mutable map<string, FXTriangulation> fxSpots_;
    mutable map<pair<string, string>, Handle<BlackVolTermStructure>> fxVols_;
    mutable map<pair<string, string>, Handle<DefaultProbabilityTermStructure>> defaultCurves_;
    mutable map<pair<string, string>, Handle<BlackVolTermStructure>> cdsVols_;
    mutable map<pair<string, string>, Handle<BaseCorrelationTermStructure<BilinearInterpolation>>> baseCorrelations_;
    mutable map<pair<string, string>, Handle<Quote>> recoveryRates_;
    mutable map<pair<string, string>, Handle<OptionletVolatilityStructure>> capFloorCurves_;
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
    mutable map<pair<string, string>, QuantLib::Handle<QuantExt::EquityIndex>> equityCurves_;
    mutable map<pair<string, string>, Handle<Quote>> cprs_;

    boost::shared_ptr<Conventions> conventions_ref_;
    // needed for the deprecated ctor taking a reference to conventions, TODO remove
    Conventions conventions_;

    //! add a swap index to the market
    void addSwapIndex(const string& swapindex, const string& discountIndex,
                      const string& configuration = Market::defaultConfiguration) const;

    // set of term structure pointers for refresh (per configuration)
    map<string, std::set<boost::shared_ptr<TermStructure>>> refreshTs_;
};

} // namespace data
} // namespace ore
