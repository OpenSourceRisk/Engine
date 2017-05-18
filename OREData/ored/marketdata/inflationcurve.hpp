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

/*! \file ored/marketdata/inflationcurve.hpp
    \brief inflation curve class
    \ingroup marketdata
*/

#pragma once

#include <ql/termstructures/inflation/inflationhelpers.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/yieldcurve.hpp>

using QuantLib::Date;
using QuantLib::InflationTermStructure;
using ore::data::CurveConfigurations;
using ore::data::Conventions;

namespace ore {
using namespace data;
namespace data {

//! Wrapper class for building inflation curves
/*!
  \ingroup curves
*/
class InflationCurve {
public:
    InflationCurve() {}
    InflationCurve(Date asof, InflationCurveSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
                   const Conventions& conventions, map<string, boost::shared_ptr<YieldCurve>>& yieldCurves);

    //! getters
    const InflationCurveSpec& spec() const { return spec_; }

    const boost::shared_ptr<InflationTermStructure> inflationTermStructure() const { return curve_; }

    const bool interpolatedIndex() const { return interpolatedIndex_; }

private:
    InflationCurveSpec spec_;
    boost::shared_ptr<InflationTermStructure> curve_;
    bool interpolatedIndex_;
};
} // namespace data
} // namespace ore
