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

/*! \file ored/marketdata/fxvolcurve.hpp
    \brief Wrapper class for building FX volatility structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/fxspot.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace ore {
namespace data {
using ore::data::CurveConfigurations;
using QuantLib::BlackVolTermStructure;
using QuantLib::Date;

// to get FX quote from container (map or FXTriangulation)
class FXLookup {
public:
    virtual ~FXLookup() {}
    virtual Handle<Quote> fxPairLookup(const string& fxPair) const = 0;
};

//! Wrapper class for building FX volatility structures
/*!
  \ingroup curves
*/
class FXVolCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    FXVolCurve() {}
    //! Detailed constructor
    FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
               const std::map<string, boost::shared_ptr<FXSpot>>& fxSpots,
               const std::map<string, boost::shared_ptr<YieldCurve>>& yieldCurves, const Conventions& conventions);
    //! Detailed constructor
    FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
               const ore::data::FXTriangulation& fxSpots,
               const std::map<string, boost::shared_ptr<YieldCurve>>& yieldCurves, const Conventions& conventions);
    //@}

    //! \name Inspectors
    //@{
    const FXVolatilityCurveSpec& spec() const { return spec_; }

    const boost::shared_ptr<BlackVolTermStructure>& volTermStructure() { return vol_; }
    //@}
private:
    FXVolatilityCurveSpec spec_;
    boost::shared_ptr<BlackVolTermStructure> vol_;

    void init(Date asof, FXVolatilityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
              const FXLookup& fxSpots, const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
              const Conventions& conventions);

    void buildSmileDeltaCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                              boost::shared_ptr<FXVolatilityCurveConfig> config, const FXLookup& fxSpots,
                              const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                              const Conventions& conventions);

    void buildVannaVolgaOrATMCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader,
                                   boost::shared_ptr<FXVolatilityCurveConfig> config, const FXLookup& fxSpots,
                                   const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                                   const Conventions& conventions);
};
} // namespace data
} // namespace ore
