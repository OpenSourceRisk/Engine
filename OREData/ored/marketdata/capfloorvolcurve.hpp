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

/*! \file ored/marketdata/capfloorvolcurve.hpp
    \brief Build optionlet volatility structures from cap floor configurations
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

/*! Class for building optionlet volatility structures from cap floor configurations
    \ingroup curves
*/
class CapFloorVolCurve {

public:
    //! Default constructor
    CapFloorVolCurve() {}

    //! Detailed constructor
    CapFloorVolCurve(
        const QuantLib::Date& asof, const CapFloorVolatilityCurveSpec& spec, const Loader& loader,
        const CurveConfigurations& curveConfigs, QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex,
        QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve, const QuantLib::ext::shared_ptr<IborIndex> sourceIndex,
        const QuantLib::ext::shared_ptr<IborIndex> targetIndex,
        const std::map<std::string, std::pair<QuantLib::ext::shared_ptr<ore::data::CapFloorVolCurve>,
                                              std::pair<std::string, QuantLib::Period>>>& requiredCapFloorVolCurves,
        const bool buildCalibrationInfo);

    //! \name Inspectors
    //@{
    //! The cap floor curve specification
    const CapFloorVolatilityCurveSpec& spec() const { return spec_; }

    //! The result of building the optionlet structure that has been configured
    const QuantLib::ext::shared_ptr<QuantLib::OptionletVolatilityStructure>& capletVolStructure() const { return capletVol_; }
    QuantLib::ext::shared_ptr<IrVolCalibrationInfo> calibrationInfo() const { return calibrationInfo_; }
    //@}

private:
    CapFloorVolatilityCurveSpec spec_;
    QuantLib::ext::shared_ptr<QuantLib::OptionletVolatilityStructure> capletVol_;
    QuantLib::ext::shared_ptr<IrVolCalibrationInfo> calibrationInfo_;

    //! Build ATM optionlet curve from term vol
    void termAtmOptCurve(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                     QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex,
                     QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve, QuantLib::Real shift);

    //! Build optionlet surface from term vol
    void termOptSurface(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                    QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex,
                    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve, QuantLib::Real shift);

    //! Build ATM optionlet curve from optionlet vol
    void optAtmOptCurve(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                         QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex,
                         QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve, QuantLib::Real shift);

    //! Build optionlet surface from optionlet vol
    void optOptSurface(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                       QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex,
                       QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve, QuantLib::Real shift);

    //! Build a cap floor term volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface>
    capSurface(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const;

    //! Build an ATM cap floor term volatility curve
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolCurve>
    atmCurve(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const;

    //! Build proxy curve
    void buildProxyCurve(
        const CapFloorVolatilityCurveConfig& config, const QuantLib::ext::shared_ptr<IborIndex>& sourceIndex,
        const QuantLib::ext::shared_ptr<IborIndex>& targetIndex,
        const std::map<std::string, std::pair<QuantLib::ext::shared_ptr<ore::data::CapFloorVolCurve>,
                                              std::pair<std::string, QuantLib::Period>>>& requiredCapFloorVolCurves);

    //! Get a shift quote value from the configured quotes
    Real shiftQuote(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const;

    //! Transform QuantExt::OptionletStripper to QuantLib::StrippedOptionlet
    QuantLib::ext::shared_ptr<QuantLib::StrippedOptionlet> transform(const QuantExt::OptionletStripper& os) const;

    //! Create a stripped optionlet curve from ATM optionlet dates and optionlet vols
    QuantLib::ext::shared_ptr<QuantLib::StrippedOptionlet>
    transform(const QuantLib::Date& asof, std::vector<QuantLib::Date> dates,
              const std::vector<QuantLib::Volatility>& volatilities, QuantLib::Natural settleDays,
              const QuantLib::Calendar& cal, QuantLib::BusinessDayConvention bdc,
              QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex, const QuantLib::DayCounter& dc,
              QuantLib::VolatilityType type, QuantLib::Real displacement) const;

    //! Generate fixing days from end date for optionlet vol
    vector<Date> populateFixingDates(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config,
                                     QuantLib::ext::shared_ptr<QuantLib::IborIndex> iborIndex, const vector<Period>& configTenors);

    //! Build calibration info
    void buildCalibrationInfo(const Date& asof, const CurveConfigurations& curveConfigs,
                              const QuantLib::ext::shared_ptr<CapFloorVolatilityCurveConfig> config,
                              const QuantLib::ext::shared_ptr<IborIndex>& iborIndex);
};

} // namespace data
} // namespace ore
