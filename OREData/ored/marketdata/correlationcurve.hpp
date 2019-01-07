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

/*! \file ored/marketdata/swaptionvolcurve.hpp
    \brief Wrapper class for building Swaption volatility structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/conventions.hpp>
#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <qle/termstructures/interpolatedcorrelationcurve.hpp>
#include <qle/termstructures/flatcorrelation.hpp>


namespace ore {
namespace data {
using QuantLib::Date;
using ore::data::CurveConfigurations;

//! Wrapper class for building correlation structures
/*!
  \ingroup curves
*/
class CorrelationCurve {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    CorrelationCurve() {}
    //! Detailed constructor
    CorrelationCurve(Date asof, CorrelationCurveSpec spec, const Loader& loader,
                     const CurveConfigurations& curveConfigs);
    //@}

    //! \name Inspectors
    //@{
    const CorrelationCurveSpec& spec() const { return spec_; }

    const boost::shared_ptr<QuantExt::CorrelationTermStructure>& corrTermStructure() { return corr_; }
    //@}
private:
    CorrelationCurveSpec spec_;
    boost::shared_ptr<QuantExt::CorrelationTermStructure> corr_;
};
} // namespace data
} // namespace ore
