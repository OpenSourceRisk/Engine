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

/*! \file ored/configuration/defaultcurveconfig.hpp
    \brief Default curve configuration classes
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

//! Default curve configuration
/*!
  \ingroup configuration
*/
class DefaultCurveConfig : public XMLSerializable {
public:
    //! Supported default curve types
    enum class Type { SpreadCDS, HazardRate, Benchmark };
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    DefaultCurveConfig(const string& curveID, const string& curveDescription, const string& currency, const Type& type,
                       const string& discountCurveID, const string& benchmarkCurveID, const string& sourceCurveID,
                       const string& recoveryRateQuote, const DayCounter& dayCounter, const string& conventionID,
                       const std::vector<string>& quotes, const std::vector<Period>& pillars, const Calendar& calendar,
                       const Size spotLag, bool extrapolation = true);
    //! Default constructor
    DefaultCurveConfig() {}
    //! Default destructor
    virtual ~DefaultCurveConfig() {}
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
    const string& discountCurveID() const { return discountCurveID_; }
    const string& benchmarkCurveID() const { return benchmarkCurveID_; }
    const string& sourceCurveID() const { return benchmarkCurveID_; }
    const string& recoveryRateQuote() const { return recoveryRateQuote_; }
    const string& conventionID() const { return conventionID_; }
    const vector<string>& quotes() const { return quotes_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const std::vector<Period>& pillars() const { return pillars_; }
    const Calendar& calendar() const { return calendar_; }
    const Size& spotLag() const { return spotLag_; }
    bool extrapolation() const { return extrapolation_; }
    //@}

    //! \name Setters
    //@{
    string& curveID() { return curveID_; }
    string& curveDescription() { return curveDescription_; }
    string& currency() { return currency_; }
    Type& type() { return type_; }
    string& discountCurveID() { return discountCurveID_; }
    string& benchmarkCurveID() { return benchmarkCurveID_; }
    string& sourceCurveID() { return sourceCurveID_; }
    string& recoveryRateQuote() { return recoveryRateQuote_; }
    string& conventionID() { return conventionID_; }
    vector<string>& quotes() { return quotes_; }
    DayCounter& dayCounter() { return dayCounter_; }
    std::vector<Period> pillars() { return pillars_; }
    Calendar calendar() { return calendar_; }
    Size spotLag() { return spotLag_; }
    bool& extrapolation() { return extrapolation_; }
    //@}

private:
    string curveID_;
    string curveDescription_;
    string currency_;
    Type type_;
    string discountCurveID_;
    string benchmarkCurveID_;
    string sourceCurveID_;
    string recoveryRateQuote_;
    DayCounter dayCounter_;
    string conventionID_;
    vector<string> quotes_;
    vector<Period> pillars_;
    Calendar calendar_;
    Size spotLag_;
    bool extrapolation_;
};
} // namespace data
} // namespace ore
