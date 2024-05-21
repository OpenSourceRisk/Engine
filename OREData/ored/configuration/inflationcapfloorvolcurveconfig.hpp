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

using ore::data::XMLNode;
using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::DayCounter;
using QuantLib::Natural;
using QuantLib::Period;
using std::string;
using std::vector;

//! Inflation CapFloor volatility curve configuration class
/*! \ingroup configuration
 */
class InflationCapFloorVolatilityCurveConfig : public CurveConfig {
public:
    enum class Type { ZC, YY };
    enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };
    enum class QuoteType { Price, Volatility };

    InflationCapFloorVolatilityCurveConfig() {}
    InflationCapFloorVolatilityCurveConfig(
        const string& curveID, const string& curveDescription, const Type type, const QuoteType& quoteType,
        const VolatilityType& volatilityType, const bool extrapolate, const vector<string>& tenors,
        const vector<string>& capStrikes, const vector<string>& floorStrikes, const vector<string>& strikes,
        const DayCounter& dayCounter, Natural settleDays, const Calendar& calendar,
        const BusinessDayConvention& businessDayConvention, const string& index, const string& indexCurve,
        const string& yieldTermStructure, const Period& observationLag, const std::string& quoteIndex = "",
        const std::string& conventions = "", const bool useLastAvailableFixingDate = false);

    //! \name XMLSerializable interface
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const Type& type() const { return type_; }
    const QuoteType& quoteType() const { return quoteType_; }
    const VolatilityType& volatilityType() const { return volatilityType_; }
    const bool& extrapolate() const { return extrapolate_; }
    const vector<string>& tenors() const { return tenors_; }
    const vector<string>& strikes() const { return strikes_; }
    const vector<string>& capStrikes() const { return capStrikes_; }
    const vector<string>& floorStrikes() const { return floorStrikes_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Natural& settleDays() const { return settleDays_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const string& index() const { return index_; }
    const string& indexCurve() const { return indexCurve_; }
    const string& yieldTermStructure() const { return yieldTermStructure_; }
    const vector<string>& quotes() override;
    const Period& observationLag() const { return observationLag_; }
    const std::string& quoteIndex() const { return quoteIndex_; }
    const std::string& conventions() const { return conventions_; }
    const bool& useLastAvailableFixingDate() const { return useLastAvailableFixingDate_; }
    //@}

    //! \name Setters
    //@{
    Type& type() { return type_; }
    QuoteType& quoteType() { return quoteType_; }
    VolatilityType& volatilityType() { return volatilityType_; }
    bool& extrapolate() { return extrapolate_; }
    vector<string>& tenors() { return tenors_; }
    vector<string>& strikes() { return strikes_; }
    vector<string>& capStrikes() { return capStrikes_; }
    vector<string>& floorStrikes() { return floorStrikes_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Natural& settleDays() { return settleDays_; }
    Calendar& calendar() { return calendar_; }
    string& index() { return index_; }
    string& indexCurve() { return indexCurve_; }
    string& yieldTermStructure() { return yieldTermStructure_; }
    Period& observationLag() { return observationLag_; }
    std::string& quoteIndex() { return quoteIndex_; }
    std::string& conventions() { return conventions_; }
    bool& useLastAvailableFixingDate() { return useLastAvailableFixingDate_; }
    //@}

private:
    void populateRequiredCurveIds();

    Type type_;
    QuoteType quoteType_;
    VolatilityType volatilityType_;
    bool extrapolate_;
    vector<string> tenors_;
    vector<string> capStrikes_;   // price surfaces with different strikes for cap and floor premia
    vector<string> floorStrikes_; // price surfaces with different strikes for cap and floor premia
    vector<string> strikes_;      // union of cap and floor strikes
    DayCounter dayCounter_;
    Natural settleDays_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    string index_;
    string indexCurve_;
    string yieldTermStructure_;
    Period observationLag_;
    // Can be different from the index_ string to allow the surface to be configured against another index's quotes.
    std::string quoteIndex_;
    std::string conventions_;
    bool useLastAvailableFixingDate_;
};

std::ostream& operator<<(std::ostream& out, InflationCapFloorVolatilityCurveConfig::VolatilityType t);
std::ostream& operator<<(std::ostream& out, InflationCapFloorVolatilityCurveConfig::QuoteType t);

} // namespace data
} // namespace ore
