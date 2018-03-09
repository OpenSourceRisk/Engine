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

#include <ored/configuration/curveconfig.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

using std::string;
using std::vector;
using ore::data::XMLNode;
using ore::data::XMLDocument;
using QuantLib::Period;
using QuantLib::DayCounter;
using QuantLib::Calendar;
using QuantLib::BusinessDayConvention;
using QuantLib::Spread;

namespace ore {
namespace data {

//! Swaption Volatility curve configuration
/*!
  \ingroup configuration
*/
class SwaptionVolatilityCurveConfig : public CurveConfig {
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
                                  const string& shortSwapIndexBase, const string& swapIndexBase,
                                  // Only required for smile
                                  const vector<Period>& smileOptionTenors = vector<Period>(),
                                  const vector<Period>& smileSwapTenors = vector<Period>(),
                                  const vector<Spread>& smileSpreads = vector<Spread>());
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
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
    const vector<Period>& smileOptionTenors() const { return smileOptionTenors_; }
    const vector<Period>& smileSwapTenors() const { return smileSwapTenors_; }
    const vector<Spread>& smileSpreads() const { return smileSpreads_; }
    const vector<string>& quotes() override;
    //@}

    //! \name Setters
    //@{
    Dimension& dimension() { return dimension_; }
    VolatilityType& volatilityType() { return volatilityType_; }
    bool& flatExtrapolation() { return flatExtrapolation_; }
    vector<Period>& optionTenors() { return optionTenors_; }
    vector<Period>& swapTenors() { return swapTenors_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Calendar& calendar() { return calendar_; }
    string& shortSwapIndexBase() { return shortSwapIndexBase_; }
    string& swapIndexBase() { return swapIndexBase_; }
    vector<Period>& smileOptionTenors() { return smileOptionTenors_; }
    vector<Period>& smileSwapTenors() { return smileSwapTenors_; }
    vector<Spread>& smileSpreads() { return smileSpreads_; }
    //@}

private:
    Dimension dimension_;
    VolatilityType volatilityType_;
    bool extrapolate_, flatExtrapolation_;
    vector<Period> optionTenors_, swapTenors_;
    DayCounter dayCounter_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    string shortSwapIndexBase_, swapIndexBase_;
    vector<Period> smileOptionTenors_;
    vector<Period> smileSwapTenors_;
    vector<Spread> smileSpreads_;
};

std::ostream& operator<<(std::ostream& out, SwaptionVolatilityCurveConfig::VolatilityType t);
} // namespace data
} // namespace ore
