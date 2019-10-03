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

/*! \file ored/configuration/capfloorvolcurveconfig.hpp
    \brief Cap floor volatility curve configuration class
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <qle/termstructures/capfloortermvolsurface.hpp>
#include <ql/termstructures/volatility/volatilitytype.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

namespace ore {
namespace data {

/*! Cap floor volatility curve configuration class
    \ingroup configuration
 */
class CapFloorVolatilityCurveConfig : public CurveConfig {
public:
    //! The type of volatility quotes that have been configured.
    enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };

    //! The type of structure that has been configured
    enum class Type { Atm, Surface, SurfaceWithAtm };

    //! Default constructor
    CapFloorVolatilityCurveConfig() {}

    //! Detailed constructor
    CapFloorVolatilityCurveConfig(
        const std::string& curveID,
        const std::string& curveDescription,
        const VolatilityType& volatilityType,
        bool extrapolate,
        bool flatExtrapolation,
        bool inlcudeAtm,
        const std::vector<std::string>& tenors,
        const std::vector<std::string>& strikes,
        const QuantLib::DayCounter& dayCounter,
        QuantLib::Natural settleDays,
        const QuantLib::Calendar& calendar,
        const QuantLib::BusinessDayConvention& businessDayConvention,
        const std::string& iborIndex,
        const std::string& discountCurve,
        const std::string& interpolationMethod = "BicubicSpline",
        const std::string& interpolateOn = "TermVolatilities",
        const std::string& timeInterpolation = "LinearFlat",
        const std::string& strikeInterpolation = "LinearFlat",
        const std::vector<std::string>& atmTenors = {},
        QuantLib::Real accuracy = 1.0e-12,
        QuantLib::Real globalAccuracy = 1.0e-10,
        bool dontThrow = false);

    //! \name XMLSerializable interface
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const VolatilityType& volatilityType() const { return volatilityType_; }
    bool extrapolate() const { return extrapolate_; }
    bool flatExtrapolation() const { return flatExtrapolation_; }
    bool includeAtm() const { return includeAtm_; }
    const std::vector<std::string>& tenors() const { return tenors_; }
    const std::vector<std::string>& strikes() const { return strikes_; }
    const QuantLib::DayCounter& dayCounter() const { return dayCounter_; }
    const QuantLib::Natural& settleDays() const { return settleDays_; }
    const QuantLib::Calendar& calendar() const { return calendar_; }
    const QuantLib::BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const std::string& iborIndex() const { return iborIndex_; }
    const std::string& discountCurve() const { return discountCurve_; }
    QuantExt::CapFloorTermVolSurface::InterpolationMethod interpolationMethod() const;
    const std::string& interpolateOn() const { return interpolateOn_; }
    const std::string& timeInterpolation() const { return timeInterpolation_; }
    const std::string& strikeInterpolation() const { return strikeInterpolation_; }
    const std::vector<std::string>& atmTenors() const { return atmTenors_; }
    QuantLib::Real accuracy() const { return accuracy_; }
    QuantLib::Real globalAccuracy() const { return globalAccuracy_; }
    bool dontThrow() const { return dontThrow_; }
    Type type() const { return type_; }
    //@}

    //! Convert VolatilityType \p type to string
    std::string toString(VolatilityType type) const;

private:
    VolatilityType volatilityType_;
    bool extrapolate_;
    bool flatExtrapolation_;
    bool includeAtm_;
    std::vector<std::string> tenors_;
    std::vector<std::string> strikes_;
    QuantLib::DayCounter dayCounter_;
    QuantLib::Natural settleDays_;
    QuantLib::Calendar calendar_;
    QuantLib::BusinessDayConvention businessDayConvention_;
    std::string iborIndex_;
    std::string discountCurve_;
    std::string interpolationMethod_;
    std::string interpolateOn_;
    std::string timeInterpolation_;
    std::string strikeInterpolation_;
    std::vector<std::string> atmTenors_;
    QuantLib::Real accuracy_;
    QuantLib::Real globalAccuracy_;
    bool dontThrow_;
    Type type_;
    std::string extrapolation_;
    
    //! Populate the quotes vector
    void populateQuotes();

    /*! Set the values of \c extrapolate_ and \c flatExtrapolation_ based on the value of \p extrapolation.
        The \p extrapolation string can take the values \c "Linear", \c "Flat" or \c "None".
        - \c "Linear" is for backwards compatibility and means extrapolation is on and flat extrapolation is off
        - \c "Flat" means extrapolation is on and it is flat
        - \c "None" means extrapolation is off
    */
    void configureExtrapolation(const std::string& extrapolation);

    //! Set the value of \c volatilityType_ based on the value of \p type
    void configureVolatilityType(const std::string& type);

    //! Set the value of \c type_ i.e. the type of cap floor structure that is configured
    void configureType();

    //! Validate the configuration
    void validate() const;
};

//! Imply market datum quote type from CapFloorVolatilityCurveConfig::VolatilityType 
std::string quoteType(CapFloorVolatilityCurveConfig::VolatilityType type);

//! Imply QuantLib::VolatilityType from CapFloorVolatilityCurveConfig::VolatilityType 
QuantLib::VolatilityType volatilityType(CapFloorVolatilityCurveConfig::VolatilityType type);

}
}
