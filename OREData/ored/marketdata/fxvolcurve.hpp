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

/*! \file ored/marketdata/fxvolcurve.hpp
    \brief Wrapper class for building FX volatility structures
    \ingroup curves
*/

#pragma once

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>

using QuantLib::Date;
using QuantLib::BlackVolTermStructure;
using openriskengine::data::CurveConfigurations;

namespace openriskengine {
namespace data {

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
    FXVolCurve(Date asof, FXVolatilityCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs);
    //@}

    //! \name Inspectors
    //@{
    const FXVolatilityCurveSpec& spec() const { return spec_; }

    const boost::shared_ptr<BlackVolTermStructure>& volTermStructure() { return vol_; }
    //@}
private:
    FXVolatilityCurveSpec spec_;
    boost::shared_ptr<BlackVolTermStructure> vol_;
};
}
}
