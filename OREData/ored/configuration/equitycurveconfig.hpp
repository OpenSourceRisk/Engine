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

#include <ql/types.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/time/calendar.hpp>
#include <ored/utilities/xmlutils.hpp>

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

//! Equity curve configuration
/*!
  \ingroup configuration
*/
class EquityCurveConfig : public XMLSerializable {
public:
    //! Supported equity curve types
    enum class Type { DividendYield, ForwardPrice };
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    EquityCurveConfig(
        const string& curveID, const string& curveDescription, 
        const string& currency, const Type& type,
        const string& equitySpotQuote, const vector<string>& quotes, 
        const string& dayCountID = "", bool extrapolation = true);
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
    const string& curveID() const { return curveID_; }
    const string& curveDescription() const { return curveDescription_; }
    const string& currency() const { return currency_; }
    const Type& type() const { return type_; }
    const string& equitySpotQuoteID() const { return equitySpotQuoteID_; }
    const string& dayCountID() const { return dayCountID_; }
    const vector<string>& quotes() const { return quotes_; }
    bool extrapolation() const { return extrapolation_; }
    //@}

    //! \name Setters
    //@{
    string& curveID() { return curveID_; }
    string& curveDescription() { return curveDescription_; }
    string& currency() { return currency_; }
    Type& type() { return type_; }
    string& equitySpotQuoteID() { return equitySpotQuoteID_; }
    string& dayCountID() { return dayCountID_; }
    vector<string>& quotes() { return quotes_; }
    bool& extrapolation() { return extrapolation_; }
    //@}

private:
    string curveID_;
    string curveDescription_;
    string currency_;
    Type type_;
    string equitySpotQuoteID_;
    string dayCountID_;
    vector<string> quotes_;
    bool extrapolation_;
};
}
}
