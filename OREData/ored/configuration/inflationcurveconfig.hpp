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

/*!
  \file ored/configuration/inflationcurveconfig.hpp
  \brief Inflation curve config
  \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/frequency.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using QuantLib::Calendar;
using QuantLib::Date;
using QuantLib::DayCounter;
using QuantLib::Frequency;
using QuantLib::Period;
using std::string;
using std::vector;

class InflationCurveConfig : public CurveConfig {
public:
    enum class Type { ZC, YY };

    InflationCurveConfig() {}
    InflationCurveConfig(const string& curveID, const string& curveDescription, const string& nominalTermStructure,
                         const Type type, const vector<string>& quotes, const string& conventions,
                         const bool extrapolate, const Calendar& calendar, const DayCounter& dayCounter,
                         const Period& lag, const Frequency& frequency, const Real baseRate, const Real tolerance, 
                         const bool useLastAvailableFixingAsBaseDate, const Date& seasonalityBaseDate, 
                         const Frequency& seasonalityFrequency, const vector<string>& seasonalityFactors,
                         const vector<double>& overrideSeasonalityFactors = std::vector<double>());

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    // Inspectors
    const string& nominalTermStructure() const { return nominalTermStructure_; }
    const Type& type() const { return type_; }
    const string& conventions() const { return conventions_; }
    const bool& extrapolate() const { return extrapolate_; }
    const Calendar& calendar() const { return calendar_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Period& lag() const { return lag_; }
    const Frequency& frequency() const { return frequency_; }
    const Real& baseRate() const { return baseRate_; }
    const Real& tolerance() const { return tolerance_; }
    const bool& useLastAvailableFixingAsBaseDate() const { return useLastAvailableFixingAsBaseDate_; }
    const Date& seasonalityBaseDate() const { return seasonalityBaseDate_; }
    const Frequency& seasonalityFrequency() const { return seasonalityFrequency_; }
    const vector<string>& seasonalityFactors() const { return seasonalityFactors_; }
    const vector<double>& overrideSeasonalityFactors() const { return overrideSeasonalityFactors_; }
    const vector<string>& swapQuotes() { return swapQuotes_; }

    // Setters
    string& nominalTermStructure() { return nominalTermStructure_; }
    Type& type() { return type_; }
    string& conventions() { return conventions_; }
    bool& extrapolate() { return extrapolate_; }
    Calendar& calendar() { return calendar_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Period& lag() { return lag_; }
    Frequency& frequency() { return frequency_; }
    Real& baseRate() { return baseRate_; }
    Real& tolerance() { return tolerance_; }
    bool& useLastAvailableFixingAsBaseDate() { return useLastAvailableFixingAsBaseDate_; }
    Date& seasonalityBaseDate() { return seasonalityBaseDate_; }
    Frequency& seasonalityFrequency() { return seasonalityFrequency_; }
    vector<string>& seasonalityFactors() { return seasonalityFactors_; }
    vector<double>& overrideSeasonalityFactors() { return overrideSeasonalityFactors_; }

private:
    void populateRequiredCurveIds();

    vector<string> swapQuotes_;
    string nominalTermStructure_;
    Type type_;
    string conventions_;
    string interpolationMethod_;
    bool extrapolate_;
    Calendar calendar_;
    DayCounter dayCounter_;
    Period lag_;
    Frequency frequency_;
    Real baseRate_;
    Real tolerance_;
    bool useLastAvailableFixingAsBaseDate_;
    Date seasonalityBaseDate_;
    Frequency seasonalityFrequency_;
    vector<string> seasonalityFactors_;
    vector<double> overrideSeasonalityFactors_;
};
} // namespace data
} // namespace ore
