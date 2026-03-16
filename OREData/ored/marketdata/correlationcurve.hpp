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
#include <ored/marketdata/capfloorvolcurve.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/swaptionvolcurve.hpp>
#include <ored/marketdata/yieldcurve.hpp>
#include <qle/termstructures/flatcorrelation.hpp>
#include <qle/termstructures/interpolatedcorrelationcurve.hpp>

namespace ore {
namespace data {
using ore::data::CurveConfigurations;
using QuantLib::Date;

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
                     const CurveConfigurations& curveConfigs, map<string, QuantLib::ext::shared_ptr<SwapIndex>>& swapIndices,
                     map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                     map<string, QuantLib::ext::shared_ptr<GenericYieldVolCurve>>& swaptionVolCurves);
    //@}

    //! \name Inspectors
    //@{
    const CorrelationCurveSpec& spec() const { return spec_; }

    const QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure>& corrTermStructure() { return corr_; }
    //@}
private:
    void calibrateCMSSpreadCorrelations(const QuantLib::ext::shared_ptr<CorrelationCurveConfig>& config, Date asof,
                                        const vector<Handle<Quote>>& prices, vector<Handle<Quote>>& quotes,
                                        QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure>& curve,
                                        map<string, QuantLib::ext::shared_ptr<SwapIndex>>& swapIndices,
                                        map<string, QuantLib::ext::shared_ptr<YieldCurve>>& yieldCurves,
                                        map<string, QuantLib::ext::shared_ptr<GenericYieldVolCurve>>& swaptionVolCurves);
    CorrelationCurveSpec spec_;
    QuantLib::ext::shared_ptr<QuantExt::CorrelationTermStructure> corr_;

    //! Calibration cost function class
    class CalibrationFunction;
    friend class CalibrationFunction;
};
} // namespace data
} // namespace ore
