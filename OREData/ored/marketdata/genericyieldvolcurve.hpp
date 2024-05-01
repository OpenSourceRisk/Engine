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

/*! \file ored/marketdata/swaptionvolcurve.hpp
    \brief Wrapper class for building Generic yield volatility structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/configuration/genericyieldvolcurveconfig.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>

namespace ore {
namespace data {
using QuantLib::Date;
using QuantLib::SwaptionVolatilityStructure;

//! Wrapper class for building Generic yield volatility structures
/*!
  \ingroup curves
*/
class GenericYieldVolCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    GenericYieldVolCurve() {}
    //! dtor
    virtual ~GenericYieldVolCurve() {}
    //! Detailed constructor
    GenericYieldVolCurve(
        const Date& asof, const Loader& loader, const CurveConfigurations& curveConfigs,
        const QuantLib::ext::shared_ptr<GenericYieldVolatilityCurveConfig>& config,
        const map<string, QuantLib::ext::shared_ptr<SwapIndex>>& requiredSwapIndices,
        const map<string, QuantLib::ext::shared_ptr<GenericYieldVolCurve>>& requiredVolCurves,
        const std::function<bool(const QuantLib::ext::shared_ptr<MarketDatum>& md, Period& expiry, Period& term)>&
            matchAtmQuote,
        const std::function<bool(const QuantLib::ext::shared_ptr<MarketDatum>& md, Period& expiry, Period& term, Real& strike)>&
            matchSmileQuote,
        const std::function<bool(const QuantLib::ext::shared_ptr<MarketDatum>& md, Period& term)>& matchShiftQuote,
        const bool buildCalibrationInfo);
    //@}

    //! \name Inspectors
    //@{
    const QuantLib::ext::shared_ptr<SwaptionVolatilityStructure>& volTermStructure() { return vol_; }
    QuantLib::ext::shared_ptr<IrVolCalibrationInfo> calibrationInfo() const { return calibrationInfo_; }
    //@}

private:
    QuantLib::ext::shared_ptr<SwaptionVolatilityStructure> vol_;
    QuantLib::ext::shared_ptr<IrVolCalibrationInfo> calibrationInfo_;
};
} // namespace data
} // namespace ore
