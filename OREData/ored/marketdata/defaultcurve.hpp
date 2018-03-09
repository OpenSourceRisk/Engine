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

/*! \file ored/marketdata/defaultcurve.hpp
    \brief Wrapper class for building Default curves
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <ql/termstructures/credit/interpolatedhazardratecurve.hpp>
#include <ql/termstructures/credit/piecewisedefaultcurve.hpp>

using QuantLib::Date;
using ore::data::CurveConfigurations;
using ore::data::Conventions;

namespace ore {
namespace data {

//! Wrapper class for building Swaption volatility structures
/*!
  \ingroup curves
*/
class DefaultCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    DefaultCurve() {}
    //! Detailed constructor
    DefaultCurve(Date asof, DefaultCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
                 const Conventions& conventions, map<string, boost::shared_ptr<YieldCurve>>& yieldCurves);
    //@}
    //! \name Inspectors
    //@{
    const DefaultCurveSpec& spec() const { return spec_; }
    const boost::shared_ptr<DefaultProbabilityTermStructure>& defaultTermStructure() { return curve_; }
    Real recoveryRate() { return recoveryRate_; }
    //@}
private:
    DefaultCurveSpec spec_;
    boost::shared_ptr<DefaultProbabilityTermStructure> curve_;
    Real recoveryRate_;
};
} // namespace data
} // namespace ore
