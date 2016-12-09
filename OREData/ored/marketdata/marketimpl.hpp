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
    \ingroup curves
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/marketdata/marketimpl.hpp>
#include <ored/configuration/conventions.hpp>

#include <map>

using namespace QuantLib;
using ore::data::Convention;
using ore::data::Conventions;
using std::string;
using std::map;
using std::pair;

namespace ore {
namespace data {

// TODO: rename class
//! Market Implementation
/*!
  The MarketImpl class differs from the Market base class in that it contains concrete maps
  of term structures, and it implements the interface.

  \ingroup curves
 */
class MarketImpl : public Market {
public:
    //! Default constructor
    MarketImpl() {}
    MarketImpl(const Conventions& conventions) : conventions_(conventions) {}

    //! \name Market interface
    //@{
    //! Get the asof Date
    Date asofDate() const { return asof_; }

    //! Yield Curves
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

    //! FX
    Handle<Quote> fxSpot(const string& ccypair, const string& configuration = Market::defaultConfiguration) const;
    Handle<BlackVolTermStructure> fxVol(const string& ccypair,
                                        const string& configuration = Market::defaultConfiguration) const;

    //! Default Curves and Recovery Rates
    Handle<DefaultProbabilityTermStructure>
    defaultCurve(const string&, const string& configuration = Market::defaultConfiguration) const;
    Handle<Quote> recoveryRate(const string&, const string& configuration = Market::defaultConfiguration) const;

    //! CapFloor volatilities
    Handle<OptionletVolatilityStructure> capFloorVol(const string& ccy,
                                                     const string& configuration = Market::defaultConfiguration) const;
    
    //! Inflation Indexes
    virtual Handle<InflationIndex> inflationIndex(const string& indexName, const bool interpoated,
                                                  const string& configuration = Market::defaultConfiguration) const;
    
    //! Inflation Cap Floor Price Surfaces
    virtual Handle<CPICapFloorTermPriceSurface>
    inflationCapFloorPriceSurface(const string& indexName,
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
    map<pair<string, string>, Handle<YieldTermStructure>> discountCurves_;
    map<pair<string, string>, Handle<YieldTermStructure>> yieldCurves_;
    map<pair<string, string>, Handle<IborIndex>> iborIndices_;
    map<pair<string, string>, Handle<SwapIndex>> swapIndices_;
    map<pair<string, string>, Handle<QuantLib::SwaptionVolatilityStructure>> swaptionCurves_;
    map<pair<string, string>, pair<string, string>> swaptionIndexBases_;
    map<string, FXTriangulation> fxSpots_;
    mutable map<pair<string, string>, Handle<BlackVolTermStructure>> fxVols_;
    map<pair<string, string>, Handle<DefaultProbabilityTermStructure>> defaultCurves_;
    map<pair<string, string>, Handle<Quote>> recoveryRates_;
    map<pair<string, string>, Handle<OptionletVolatilityStructure>> capFloorCurves_;
    map<pair<string, pair<string, bool>>, Handle<InflationIndex>> inflationIndices_;
    map<pair<string, string>, Handle<CPICapFloorTermPriceSurface>> inflationCapFloorPriceSurfaces_;
    Conventions conventions_;

    //! add a swap index to the market
    void addSwapIndex(const string& swapindex, const string& discountIndex,
                      const string& configuration = Market::defaultConfiguration);

    // set of term structure pointers for refresh (per configuration)
    map<string, std::set<boost::shared_ptr<TermStructure>>> refreshTs_;
};
} // namespace data
} // namespace ore
