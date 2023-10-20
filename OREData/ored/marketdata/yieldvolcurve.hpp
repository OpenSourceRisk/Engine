/*
Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/yieldvolcurve.hpp
\brief Wrapper class for building yield volatility structures
\ingroup curves
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/genericyieldvolcurve.hpp>

namespace ore {
namespace data {
using ore::data::CurveConfigurations;
using QuantLib::Date;

//! Wrapper class for building Yield volatility structures
/*!
  \ingroup curves
*/
class YieldVolCurve : public GenericYieldVolCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    YieldVolCurve() {}
    //! Detailed constructor
    YieldVolCurve(Date asof, YieldVolatilityCurveSpec spec, const Loader& loader,
                  const CurveConfigurations& curveConfigs, const bool buildCalibrationInfo);
    //@}

    //! \name Inspectors
    //@{
    const YieldVolatilityCurveSpec& spec() const { return spec_; }
    //@}

private:
    YieldVolatilityCurveSpec spec_;
};
} // namespace data
} // namespace ore
