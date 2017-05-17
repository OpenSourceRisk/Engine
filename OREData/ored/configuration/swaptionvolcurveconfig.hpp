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

/*! \file ored/configuration/swaptionvolcurveconfig.hpp
    \brief Swaption volatility curve configuration classes
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
using QuantLib::Calendar;
using QuantLib::BusinessDayConvention;

namespace ore {
namespace data {

//! Swaption Volatility curve configuration
/*!
  \ingroup configuration
*/
class SwaptionVolatilityCurveConfig : public XMLSerializable {
public:
    //! supported volatility dimensions
    enum class Dimension { ATM, Smile };
    // supported volatility types
    enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };

    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    SwaptionVolatilityCurveConfig() {}
    //! Detailed constructor
    SwaptionVolatilityCurveConfig(const string& curveID, const string& curveDescription, const Dimension& dimension,
                                  const VolatilityType& volatilityType, const bool extrapolate,
                                  const bool flatExtrapolation, const vector<Period>& optionTenors,
                                  const vector<Period>& swapTenors, const DayCounter& dayCounter,
                                  const Calendar& calendar, const BusinessDayConvention& businessDayConvention,
                                  const string& shortSwapIndexBase, const string& swapIndexBase);
    //! Default destructor
    virtual ~SwaptionVolatilityCurveConfig() {}
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Inspectors
    //@{
    const string& curveID() const { return curveID_; }
    const string& curveDescription() const { return curveDescription_; }
    const Dimension& dimension() const { return dimension_; }
    const VolatilityType& volatilityType() const { return volatilityType_; }
    const bool& extrapolate() const { return extrapolate_; }
    const bool& flatExtrapolation() const { return flatExtrapolation_; }
    const vector<Period>& optionTenors() const { return optionTenors_; }
    const vector<Period>& swapTenors() const { return swapTenors_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const string& shortSwapIndexBase() const { return shortSwapIndexBase_; }
    const string& swapIndexBase() const { return swapIndexBase_; }
    //@}

    //! \name Setters
    //@{
    string& curveID() { return curveID_; }
    string& curveDescription() { return curveDescription_; }
    Dimension& dimension() { return dimension_; }
    VolatilityType& volatilityType() { return volatilityType_; }
    bool& flatExtrapolation() { return flatExtrapolation_; }
    vector<Period>& optionTenors() { return optionTenors_; }
    vector<Period>& swapTenors() { return swapTenors_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Calendar& calendar() { return calendar_; }
    string& shortSwapIndexBase() { return shortSwapIndexBase_; }
    string& swapIndexBase() { return swapIndexBase_; }
    //@}

private:
    string curveID_;
    string curveDescription_;
    Dimension dimension_;
    VolatilityType volatilityType_;
    bool extrapolate_, flatExtrapolation_;
    vector<Period> optionTenors_, swapTenors_;
    DayCounter dayCounter_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    string shortSwapIndexBase_, swapIndexBase_;
};
} // namespace data
} // namespace ore
