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
    CapFloorVolCurve(const QuantLib::Date& asof, const CapFloorVolatilityCurveSpec& spec, const Loader& loader,
                     const CurveConfigurations& curveConfigs, boost::shared_ptr<QuantLib::IborIndex> iborIndex,
                     QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve);

    //! \name Inspectors
    //@{
    //! The cap floor curve specification
    const CapFloorVolatilityCurveSpec& spec() const { return spec_; }

    //! The result of building the optionlet structure that has been configured
    const boost::shared_ptr<QuantLib::OptionletVolatilityStructure>& capletVolStructure() const { return capletVol_; }
    //@}

private:
    CapFloorVolatilityCurveSpec spec_;
    boost::shared_ptr<QuantLib::OptionletVolatilityStructure> capletVol_;

    //! Build ATM optionlet curve
    void atmOptCurve(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                     boost::shared_ptr<QuantLib::IborIndex> iborIndex,
                     QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve, QuantLib::Real shift);

    //! Build optionlet surface
    void optSurface(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader,
                    boost::shared_ptr<QuantLib::IborIndex> iborIndex,
                    QuantLib::Handle<QuantLib::YieldTermStructure> discountCurve, QuantLib::Real shift);

    //! Build a cap floor term volatility surface
    boost::shared_ptr<QuantExt::CapFloorTermVolSurface>
    capSurface(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const;

    //! Build an ATM cap floor term volatility curve
    boost::shared_ptr<QuantExt::CapFloorTermVolCurve>
    atmCurve(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const;

    //! Get a shift quote value from the configured quotes
    Real shiftQuote(const QuantLib::Date& asof, CapFloorVolatilityCurveConfig& config, const Loader& loader) const;

    //! Transform QuantExt::OptionletStripper to QuantLib::StrippedOptionlet
    boost::shared_ptr<QuantLib::StrippedOptionlet> transform(const QuantExt::OptionletStripper& os) const;

    //! Create a stripped optionlet curve from ATM optionlet dates and optionlet vols
    boost::shared_ptr<QuantLib::StrippedOptionlet>
    transform(const QuantLib::Date& asof, std::vector<QuantLib::Date> dates,
              const std::vector<QuantLib::Volatility>& volatilities, QuantLib::Natural settleDays,
              const QuantLib::Calendar& cal, QuantLib::BusinessDayConvention bdc,
              boost::shared_ptr<QuantLib::IborIndex> iborIndex, const QuantLib::DayCounter& dc,
              QuantLib::VolatilityType type, QuantLib::Real displacement) const;
};

} // namespace data
} // namespace ore
