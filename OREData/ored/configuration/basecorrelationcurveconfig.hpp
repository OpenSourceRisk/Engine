/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file ored/configuration/basecorrelationcurveconfig.hpp
    \brief Base Correlation curve configuration classes
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
using QuantLib::Natural;
using QuantLib::Calendar;
using QuantLib::BusinessDayConvention;

namespace ore {
namespace data {

//! Base Correlation term structure configuration
/*!
  \ingroup configuration
*/
class BaseCorrelationCurveConfig : public CurveConfig {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    BaseCorrelationCurveConfig() {}
    //! Detailed constructor
    BaseCorrelationCurveConfig(const string& curveID, const string& curveDescription,
                               const vector<Real>& detachmentPoints, const vector<Period>& terms);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const vector<Period>& terms() const { return terms_; }
    const vector<Real>& detachmentPoints() const { return detachmentPoints_; }
    const Size& settlementDays() const { return settlementDays_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const bool& extrapolate() const { return extrapolate_; }
    const vector<string>& quotes() override;
    //@}

    //! \name Setters
    //@{
    vector<Period>& terms() { return terms_; }
    vector<Real>& detachmentPoints() { return detachmentPoints_; }
    Size& settlementDays() { return settlementDays_; }
    Calendar& calendar() { return calendar_; }
    BusinessDayConvention& businessDayConvention() { return businessDayConvention_; }
    DayCounter& dayCounter() { return dayCounter_; }
    bool& extrapolate() { return extrapolate_; }
    //@}
private:
    vector<Real> detachmentPoints_;
    vector<Period> terms_;
    Size settlementDays_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    DayCounter dayCounter_;
    bool extrapolate_;
};
} // namespace data
} // namespace ore
