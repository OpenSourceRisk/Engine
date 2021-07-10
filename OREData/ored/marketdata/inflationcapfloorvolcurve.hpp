/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/inflationcapfloorvolcurve.hpp
    \brief Wrapper class for building YoY Inflation CapFloor volatility structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/inflationcurve.hpp>
#include <ored/marketdata/loader.hpp>

#include <ql/termstructures/volatility/inflation/cpivolatilitystructure.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>

namespace ore {
namespace data {
using ore::data::CurveConfigurations;
using QuantLib::Date;
using QuantLib::IborIndex;
using QuantLib::YieldTermStructure;

//! Wrapper class for building CapFloor volatility structures
//! \ingroup curves
class InflationCapFloorVolCurve {
public:
    InflationCapFloorVolCurve() {}
    InflationCapFloorVolCurve(Date asof, InflationCapFloorVolatilityCurveSpec spec, const Loader& loader,
                              const CurveConfigurations& curveConfigs,
                              map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                              map<string, boost::shared_ptr<InflationCurve>>& inflationCurves,
                              const boost::shared_ptr<Conventions>& conventions = nullptr);

    //! \name Inspectors
    //@{
    const InflationCapFloorVolatilityCurveSpec& spec() const { return spec_; }
    //! Caplet/Floorlet curve or surface i.e. result of stripping
    const boost::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> yoyInflationCapFloorVolSurface() const {
        return yoyVolSurface_;
    }
    const boost::shared_ptr<QuantLib::CPIVolatilitySurface> cpiInflationCapFloorVolSurface() const {
        return cpiVolSurface_;
    }

    //@}
private:
    void buildFromVolatilities(Date asof, InflationCapFloorVolatilityCurveSpec spec, const Loader& loader,
                               const boost::shared_ptr<InflationCapFloorVolatilityCurveConfig>& config,
                               map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                               map<string, boost::shared_ptr<InflationCurve>>& inflationCurves);
    void buildFromPrices(Date asof, InflationCapFloorVolatilityCurveSpec spec, const Loader& loader,
                         const boost::shared_ptr<InflationCapFloorVolatilityCurveConfig>& config,
                         map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                         map<string, boost::shared_ptr<InflationCurve>>& inflationCurves);

    InflationCapFloorVolatilityCurveSpec spec_;
    boost::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> yoyVolSurface_;
    boost::shared_ptr<QuantLib::CPIVolatilitySurface> cpiVolSurface_;
    boost::shared_ptr<InflationTermStructure> surface_;
    bool useMarketYoyCurve_;
    boost::shared_ptr<YoYInflationTermStructure> yoyTs_;
    boost::shared_ptr<Conventions> conventions_;
    Handle<YieldTermStructure> discountCurve_;
};
} // namespace data
} // namespace ore
