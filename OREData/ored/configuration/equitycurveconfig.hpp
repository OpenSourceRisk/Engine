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

/*! \file ored/configuration/equitycurveconfig.hpp
    \brief Equity curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/exercise.hpp>
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
using QuantLib::Period;
using std::string;
using std::vector;

//! Equity curve configuration
/*!
  \ingroup configuration
*/
class EquityCurveConfig : public CurveConfig {
public:
    //! Supported equity curve types
    enum class Type { DividendYield, ForwardPrice, OptionPremium, NoDividends, ForwardDividendPrice};
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    EquityCurveConfig(const string& curveID, const string& curveDescription, const string& forecastingCurve,
                      const string& currency, const string& calendar, const Type& type, const string& equitySpotQuote,
                      const vector<string>& quotes, const string& dayCountID = "",
                      const string& dividendInterpVariable = "Zero", const string& dividendInterpMethod = "Linear",
                      const bool dividendExtrapolation = false, const bool extrapolation = false,
                      const QuantLib::Exercise::Type& exerciseStyle = QuantLib::Exercise::Type::European);
    //! Default constructor
    EquityCurveConfig() {}
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& forecastingCurve() const { return forecastingCurve_; }
    const string& currency() const { return parseCurrencyWithMinors(currency_).code(); }
    const string& calendar() const { return calendar_; }
    const Type& type() const { return type_; }
    const string& equitySpotQuoteID() const { return equitySpotQuoteID_; }
    const string& dayCountID() const { return dayCountID_; }
    const string& dividendInterpolationVariable() const { return divInterpVariable_; }
    const string& dividendInterpolationMethod() const { return divInterpMethod_; }
    bool dividendExtrapolation() const { return dividendExtrapolation_; }
    bool extrapolation() const { return extrapolation_; }
    const QuantLib::Exercise::Type exerciseStyle() const { return exerciseStyle_; }
    const vector<string>& fwdQuotes() { return fwdQuotes_; }
    //@}

    //! \name Setters
    //@{
    string& forecastingCurve() { return forecastingCurve_; }
    Type& type() { return type_; }
    string& equitySpotQuoteID() { return equitySpotQuoteID_; }
    string& dayCountID() { return dayCountID_; }
    string& dividendInterpolationVariable() { return divInterpVariable_; }
    string& dividendInterpolationMethod() { return divInterpMethod_; }
    bool& dividendExtrapolation() { return dividendExtrapolation_; }
    bool& extrapolation() { return extrapolation_; }
    QuantLib::Exercise::Type& exerciseStyle() { return exerciseStyle_; }

    void setCurrency(const string& currency) { currency_ = currency; }
    void setCalendar(const string& calendar) { calendar_ = calendar; }
    //@}

private:
    void populateRequiredCurveIds();

    vector<string> fwdQuotes_;
    string forecastingCurve_;
    string currency_;
    string calendar_;
    Type type_;
    string equitySpotQuoteID_;
    string dayCountID_;
    string divInterpVariable_;
    string divInterpMethod_;
    bool dividendExtrapolation_;
    bool extrapolation_;
    QuantLib::Exercise::Type exerciseStyle_;
};

std::ostream& operator<<(std::ostream& out, EquityCurveConfig::Type t);

EquityCurveConfig::Type parseEquityCurveConfigType(const std::string& str);

} // namespace data
} // namespace ore
