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

/*! \file ored/marketdata/equitycurve.hpp
    \brief Wrapper class for building Equity curves
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <qle/indexes/equityindex.hpp>

namespace ore {
namespace data {
using ore::data::Conventions;
using ore::data::CurveConfigurations;
using QuantLib::Date;

//! Wrapper class for building Equity curves (spot quote, yield term structure, risk free IR term structure)
/*!
  \ingroup curves
*/
class EquityCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    EquityCurve() {}
    //! Detailed constructor
    EquityCurve(Date asof, EquityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
                const map<string, QuantLib::ext::shared_ptr<YieldCurve>>& requiredYieldCurves, const bool buildCalibrationInfo);
    //@}
    //! \name Inspectors
    //@{
    const EquityCurveSpec& spec() const { return spec_; };
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityIndex() const { return equityIndex_; };
    QuantLib::ext::shared_ptr<YieldCurveCalibrationInfo> calibrationInfo() const { return calibrationInfo_; }
    //@}
private:
    EquityCurveSpec spec_;
    EquityCurveConfig::Type curveType_;
    vector<Real> quotes_;
    vector<Date> terms_;
    DayCounter dc_;
    YieldCurve::InterpolationVariable dividendInterpVariable_;
    YieldCurve::InterpolationMethod dividendInterpMethod_;
    QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> equityIndex_;
    QuantLib::ext::shared_ptr<YieldCurveCalibrationInfo> calibrationInfo_;
};
} // namespace data
} // namespace ore
