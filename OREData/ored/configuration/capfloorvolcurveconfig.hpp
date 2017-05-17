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
    \brief CapFloor volatility curve configuration class
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>

#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

using std::string;
using std::vector;
using ore::data::XMLSerializable;
using ore::data::XMLNode;
using ore::data::XMLDocument;
using QuantLib::Period;
using QuantLib::DayCounter;
using QuantLib::Natural;
using QuantLib::Calendar;
using QuantLib::BusinessDayConvention;

namespace ore {
namespace data {

//! CapFloor volatility curve configuration class
/*! \ingroup configuration
*/
class CapFloorVolatilityCurveConfig : public XMLSerializable {
public:
    enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };

    CapFloorVolatilityCurveConfig() {}
    CapFloorVolatilityCurveConfig(const string& curveID, const string& curveDescription,
                                  const VolatilityType& volatilityType, const bool extrapolate, bool inlcudeAtm,
                                  const vector<Period>& tenors, const vector<double>& strikes,
                                  const DayCounter& dayCounter, Natural settleDays, const Calendar& calendar,
                                  const BusinessDayConvention& businessDayConvention, const string& iborIndex,
                                  const string& discountCurve);
    virtual ~CapFloorVolatilityCurveConfig() {}

    //! \name XMLSerializable interface
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Inspectors
    //@{
    const string& curveID() const { return curveID_; }
    const string& curveDescription() const { return curveDescription_; }
    const VolatilityType& volatilityType() const { return volatilityType_; }
    const bool& extrapolate() const { return extrapolate_; }
    const bool& includeAtm() const { return includeAtm_; }
    const vector<Period>& tenors() const { return tenors_; }
    const vector<double>& strikes() const { return strikes_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Natural& settleDays() const { return settleDays_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const string& iborIndex() const { return iborIndex_; }
    const string& discountCurve() const { return discountCurve_; }
    //@}

    //! \name Setters
    //@{
    string& curveID() { return curveID_; }
    string& curveDescription() { return curveDescription_; }
    VolatilityType& volatilityType() { return volatilityType_; }
    bool& extrapolate() { return extrapolate_; }
    bool& includeAtm() { return includeAtm_; }
    vector<Period>& tenors() { return tenors_; }
    vector<double>& strikes() { return strikes_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Natural& settleDays() { return settleDays_; }
    Calendar& calendar() { return calendar_; }
    string& iborIndex() { return iborIndex_; }
    string& discountCurve() { return discountCurve_; }
    //@}

private:
    string curveID_;
    string curveDescription_;
    VolatilityType volatilityType_;
    bool extrapolate_, includeAtm_;
    vector<Period> tenors_;
    vector<double> strikes_;
    DayCounter dayCounter_;
    Natural settleDays_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    string iborIndex_;
    string discountCurve_;
};
}
}
