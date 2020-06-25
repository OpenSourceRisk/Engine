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

/*! \file ored/marketdata/equityvolcurve.hpp
    \brief Wrapper class for building Equity volatility structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/equitycurve.hpp>
#include <ored/marketdata/loader.hpp>
#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>

namespace ore {
namespace data {
using ore::data::CurveConfigurations;
using QuantLib::BlackVolTermStructure;
using QuantLib::Date;

//! Wrapper class for building Equity volatility structures
/*!
  \ingroup curves
*/
class EquityVolCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    EquityVolCurve() {}
    //! Detailed constructor
    EquityVolCurve(Date asof, EquityVolatilityCurveSpec spec, const Loader& loader,
                   const CurveConfigurations& curveConfigs, const QuantLib::Handle<QuantExt::EquityIndex>& eqIndex,
                   const map<string, boost::shared_ptr<EquityCurve>>& requiredEquityCurves = {},
                   const map<string, boost::shared_ptr<EquityVolCurve>>& requiredEquityVolCurves = {});
    //@}

    //! \name Inspectors
    //@{
    const EquityVolatilityCurveSpec& spec() const { return spec_; }

    //! Build a volatility structure from a single constant volatlity quote
    void buildVolatility(const QuantLib::Date& asof, const EquityVolatilityCurveConfig& vc,
                         const ConstantVolatilityConfig& cvc, const Loader& loader);

    //! Build a volatility curve from a 1-D curve of volatlity quotes
    void buildVolatility(const QuantLib::Date& asof, const EquityVolatilityCurveConfig& vc,
                         const VolatilityCurveConfig& vcc, const Loader& loader);

    //! Build a volatility surface from a collection of expiry and absolute strike pairs.
    void buildVolatility(const QuantLib::Date& asof, EquityVolatilityCurveConfig& vc,
                         const VolatilityStrikeSurfaceConfig& vssc, const Loader& loader,
                         const QuantLib::Handle<QuantExt::EquityIndex>& eqIndex);

    //! Build a volatility surface as a proxy from another volatility surface
    void buildVolatility(const QuantLib::Date& asof, const EquityVolatilityCurveSpec& spec,
                         const CurveConfigurations& curveConfigs,
                         const map<string, boost::shared_ptr<EquityCurve>>& eqCurves,
                         const map<string, boost::shared_ptr<EquityVolCurve>>& eqVolCurves);

    const boost::shared_ptr<BlackVolTermStructure>& volTermStructure() { return vol_; }
    //@}
private:
    EquityVolatilityCurveSpec spec_;
    boost::shared_ptr<BlackVolTermStructure> vol_;
    QuantLib::Calendar calendar_;
    QuantLib::DayCounter dayCounter_;
};
} // namespace data
} // namespace ore
