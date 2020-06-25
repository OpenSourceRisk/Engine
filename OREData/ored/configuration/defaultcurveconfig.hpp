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

#include <ored/configuration/bootstrapconfig.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
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

//! Default curve configuration
/*!
  \ingroup configuration
*/
class DefaultCurveConfig : public CurveConfig {
public:
    //! Supported default curve types
    enum class Type { SpreadCDS, HazardRate, Benchmark, Price };
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    DefaultCurveConfig(const string& curveID, const string& curveDescription, const string& currency, const Type& type,
                       const string& discountCurveID, const string& recoveryRateQuote, const DayCounter& dayCounter,
                       const string& conventionID, const std::vector<std::pair<std::string, bool>>& cdsQuotes,
                       bool extrapolation = true, const string& benchmarkCurveID = "", const string& sourceCurveID = "",
                       const std::vector<string>& pillars = std::vector<string>(),
                       const Calendar& calendar = Calendar(), const Size spotLag = 0,
                       const QuantLib::Date& startDate = QuantLib::Date(),
                       const BootstrapConfig& bootstrapConfig = BootstrapConfig(),
                       QuantLib::Real runningSpread = QuantLib::Null<Real>());
    //! Default constructor
    DefaultCurveConfig() {}
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const string& currency() const { return currency_; }
    const Type& type() const { return type_; }
    const string& discountCurveID() const { return discountCurveID_; }
    const string& benchmarkCurveID() const { return benchmarkCurveID_; }
    const string& sourceCurveID() const { return benchmarkCurveID_; }
    const string& recoveryRateQuote() const { return recoveryRateQuote_; }
    const string& conventionID() const { return conventionID_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const std::vector<string>& pillars() const { return pillars_; }
    const Calendar& calendar() const { return calendar_; }
    const Size& spotLag() const { return spotLag_; }
    bool extrapolation() const { return extrapolation_; }
    const std::vector<std::pair<std::string, bool>>& cdsQuotes() { return cdsQuotes_; }
    const QuantLib::Date& startDate() const { return startDate_; }
    const BootstrapConfig& bootstrapConfig() const { return bootstrapConfig_; }
    const Real runningSpread() const { return runningSpread_; }
    //@}

    //! \name Setters
    //@{
    string& currency() { return currency_; }
    Type& type() { return type_; }
    string& discountCurveID() { return discountCurveID_; }
    string& benchmarkCurveID() { return benchmarkCurveID_; }
    string& sourceCurveID() { return sourceCurveID_; }
    string& recoveryRateQuote() { return recoveryRateQuote_; }
    string& conventionID() { return conventionID_; }
    DayCounter& dayCounter() { return dayCounter_; }
    std::vector<string> pillars() { return pillars_; }
    Calendar calendar() { return calendar_; }
    Size spotLag() { return spotLag_; }
    bool& extrapolation() { return extrapolation_; }
    QuantLib::Date& startDate() { return startDate_; }
    void setBootstrapConfig(const BootstrapConfig& bootstrapConfig) { bootstrapConfig_ = bootstrapConfig; }
    Real& runningSpread() { return runningSpread_; }
    //@}

private:
    //! Quote and optional flag pair
    std::vector<std::pair<std::string, bool>> cdsQuotes_;
    string currency_;
    Type type_;
    string discountCurveID_;
    string recoveryRateQuote_;
    DayCounter dayCounter_;
    string conventionID_;
    bool extrapolation_;
    string benchmarkCurveID_;
    string sourceCurveID_;
    vector<string> pillars_;
    Calendar calendar_;
    Size spotLag_;
    QuantLib::Date startDate_;
    BootstrapConfig bootstrapConfig_;
    Real runningSpread_;
};
} // namespace data
} // namespace ore
