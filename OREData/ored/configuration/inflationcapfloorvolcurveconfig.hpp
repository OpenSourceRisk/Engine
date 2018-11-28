/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/inflationcapfloorvolcurveconfig.hpp 
    \brief Inflation CapFloor volatility curve configuration class
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>


namespace ore {
namespace data {
using std::string;
using std::vector;
using ore::data::XMLNode;
using QuantLib::Period;
using QuantLib::DayCounter;
using QuantLib::Natural;
using QuantLib::Calendar;
using QuantLib::BusinessDayConvention;

//! Inflation CapFloor volatility curve configuration class
/*! \ingroup configuration
*/
class InflationCapFloorVolatilityCurveConfig : public CurveConfig {
public:
    enum class Type { ZC, YY };
    enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };

    InflationCapFloorVolatilityCurveConfig() {}
    InflationCapFloorVolatilityCurveConfig(const string& curveID, const string& curveDescription,
        const Type type, const VolatilityType& volatilityType, const bool extrapolate,
        const vector<Period>& tenors, const vector<double>& strikes,
        const DayCounter& dayCounter, Natural settleDays, const Calendar& calendar,
        const BusinessDayConvention& businessDayConvention, const string& index, 
        const string& indexCurve, const string& yieldTermStructure);

    //! \name XMLSerializable interface
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const Type& type() const { return type_; }
    const VolatilityType& volatilityType() const { return volatilityType_; }
    const bool& extrapolate() const { return extrapolate_; }
    const vector<Period>& tenors() const { return tenors_; }
    const vector<double>& strikes() const { return strikes_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Natural& settleDays() const { return settleDays_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const string& index() const { return index_; }
    const string& indexCurve() const { return indexCurve_; }
    const string& yieldTermStructure() const { return yieldTermStructure_; }
    const vector<string>& quotes() override;
    //@}

    //! \name Setters
    //@{
    Type& type() { return type_; }
    VolatilityType& volatilityType() { return volatilityType_; }
    bool& extrapolate() { return extrapolate_; }
    vector<Period>& tenors() { return tenors_; }
    vector<double>& strikes() { return strikes_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Natural& settleDays() { return settleDays_; }
    Calendar& calendar() { return calendar_; }
    string& index() { return index_; }
    string& indexCurve() { return indexCurve_; }
    string& yieldTermStructure() { return yieldTermStructure_; }
    //@}

private:
    Type type_;
    VolatilityType volatilityType_;
    bool extrapolate_;
    vector<Period> tenors_;
    vector<double> strikes_;
    DayCounter dayCounter_;
    Natural settleDays_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    string index_;
    string indexCurve_;
    string yieldTermStructure_;
};

std::ostream& operator<<(std::ostream& out, InflationCapFloorVolatilityCurveConfig::VolatilityType t);
} // namespace data
} // namespace ore
