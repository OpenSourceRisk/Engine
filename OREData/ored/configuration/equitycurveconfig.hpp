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

namespace ore {
namespace data {

//! Equity curve configuration
/*!
  \ingroup configuration
*/
class EquityCurveConfig : public CurveConfig {
public:
    //! Supported equity curve types
    enum class Type { DividendYield, ForwardPrice };
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    EquityCurveConfig(const string& curveID, const string& curveDescription, const string& forecastingCurve, const string& currency, const Type& type,
                      const string& equitySpotQuote, const vector<string>& quotes, const string& dayCountID = "",
                      bool extrapolation = true);
    //! Default constructor
    EquityCurveConfig() {}
    //! Default destructor
    virtual ~EquityCurveConfig() {}
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}

    //! \name Inspectors
    //@{
    const string& forecastingCurve() const { return forecastingCurve_; }
    const string& currency() const { return currency_; }
    const Type& type() const { return type_; }
    const string& equitySpotQuoteID() const { return quotes_[0]; }
    const string& dayCountID() const { return dayCountID_; }
    bool extrapolation() const { return extrapolation_; }
    //@}

    //! \name Setters
    //@{
    string& forecastingCurve() { return forecastingCurve_; }
    string& currency() { return currency_; }
    Type& type() { return type_; }
    string& equitySpotQuoteID() { return equitySpotQuoteID_; }
    string& dayCountID() { return dayCountID_; }
    bool& extrapolation() { return extrapolation_; }
    //@}

private:
    string forecastingCurve_;
    string currency_;
    Type type_;
    string equitySpotQuoteID_;
    string dayCountID_;
    bool extrapolation_;
};
} // namespace data
} // namespace ore
