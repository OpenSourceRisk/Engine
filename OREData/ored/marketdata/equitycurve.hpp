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
#include <ored/marketdata/yieldcurve.hpp>

using QuantLib::Date;
using ore::data::CurveConfigurations;
using ore::data::Conventions;

namespace ore {
namespace data {

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
                const Conventions& conventions);
    //@}
    //! \name Inspectors
    //@{
    const EquityCurveSpec& spec() const { return spec_; }
    boost::shared_ptr<YieldTermStructure> divYieldTermStructure(const Date& asof,
                                                                const Handle<YieldTermStructure>& equityIrCurve) const;
    const Real equitySpot() const { return equitySpot_; }
    //@}
private:
    EquityCurveSpec spec_;
    Real equitySpot_;
    EquityCurveConfig::Type curveType_;
    vector<Real> quotes_;
    vector<Date> terms_;
    DayCounter dc_;
};
}
}
