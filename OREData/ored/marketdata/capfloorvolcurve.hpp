/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/marketdata/capfloorvolcurve.hpp
    \brief Wrapper class for building CapFloor volatility structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>

using QuantLib::Date;
using QuantLib::IborIndex;
using QuantLib::YieldTermStructure;
using QuantLib::OptionletVolatilityStructure;
using openriskengine::data::CurveConfigurations;

namespace openriskengine {
namespace data {

//! Wrapper class for building CapFloor volatility structures
//! \ingroup curves
class CapFloorVolCurve {
public:
    CapFloorVolCurve() {}
    CapFloorVolCurve(Date asof, CapFloorVolatilityCurveSpec spec, const Loader& loader,
                     const CurveConfigurations& curveConfigs, boost::shared_ptr<IborIndex> iborIndex,
                     Handle<YieldTermStructure> discountCurve);

    //! \name Inspectors
    //@{
    const CapFloorVolatilityCurveSpec& spec() const { return spec_; }
    //! Caplet/Floorlet curve or surface i.e. result of stripping
    const boost::shared_ptr<OptionletVolatilityStructure>& capletVolStructure() const { return capletVol_; }
    //@}
private:
    CapFloorVolatilityCurveSpec spec_;
    boost::shared_ptr<OptionletVolatilityStructure> capletVol_;
};
}
}
