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

/*! \file ored/marketdata/optionletvolcurve.hpp
    \brief Build optionlet volatility structures from optionlet vol configurations
    \ingroup curves
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionlet.hpp>
#include <qle/termstructures/capfloortermvolcurve.hpp>
#include <qle/termstructures/capfloortermvolsurface.hpp>
#include <qle/termstructures/optionletstripper.hpp>

namespace ore {
namespace data {

/*! Class for building optionlet volatility structures from optionlet vol configurations
    \ingroup curves
*/
class OptionletVolCurve {

public:
    //! Default constructor
    OptionletVolCurve() {}

    //! Detailed constructor
    OptionletVolCurve(
        const QuantLib::Date& asof, const OptionletVolatilityCurveSpec& spec, const Loader& loader,
        const CurveConfigurations& curveConfigs, boost::shared_ptr<QuantLib::IborIndex> iborIndex,
        QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve,
        const std::map<std::string, std::pair<boost::shared_ptr<ore::data::OptionletVolCurve>,
                                              std::pair<std::string, QuantLib::Period>>>& requiredOptionletVolCurves,
        const bool buildCalibrationInfo);

    //! \name Inspectors
    //@{
    //! The optionlet vol curve specification
    const OptionletVolatilityCurveSpec& spec() const { return spec_; }

    //! The result of building the optionlet structure that has been configured
    const boost::shared_ptr<QuantLib::OptionletVolatilityStructure>& capletVolStructure() const { return capletVol_; }
    boost::shared_ptr<IrVolCalibrationInfo> calibrationInfo() const { return calibrationInfo_; }
    //@}

private:
    OptionletVolatilityCurveSpec spec_;
    boost::shared_ptr<QuantLib::OptionletVolatilityStructure> capletVol_;
    boost::shared_ptr<IrVolCalibrationInfo> calibrationInfo_;

    //! Build optionlet surface
    void optSurface(const QuantLib::Date& asof, OptionletVolatilityCurveConfig& config, const Loader& loader,
                    boost::shared_ptr<QuantLib::IborIndex> iborIndex,
                    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve);
};

} // namespace data
} // namespace ore
