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
#include <ql/time/dategenerationrule.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>
#include <boost/optional/optional.hpp>

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

//! Base Correlation term structure configuration
/*!
  \ingroup configuration
*/
class BaseCorrelationCurveConfig : public CurveConfig {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    BaseCorrelationCurveConfig();

    //! Detailed constructor
    BaseCorrelationCurveConfig(
        const string& curveID,
        const string& curveDescription,
        const vector<string>& detachmentPoints,
        const vector<string>& terms,
        QuantLib::Size settlementDays,
        const QuantLib::Calendar& calendar,
        QuantLib::BusinessDayConvention businessDayConvention,
        QuantLib::DayCounter dayCounter,
        bool extrapolate,
        const std::string& quoteName = "",
        const QuantLib::Date& startDate = QuantLib::Date(),
        const QuantLib::Period& indexTerm = 0 * QuantLib::Days,
        boost::optional<QuantLib::DateGeneration::Rule> rule = boost::none,
        bool adjustForLosses = true);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const vector<string>& terms() const { return terms_; }
    const vector<string>& detachmentPoints() const { return detachmentPoints_; }
    const Size& settlementDays() const { return settlementDays_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const bool& extrapolate() const { return extrapolate_; }
    const std::string& quoteName() const { return quoteName_; }
    const QuantLib::Date& startDate() const { return startDate_; }
    const QuantLib::Period& indexTerm() const { return indexTerm_; }
    const boost::optional<QuantLib::DateGeneration::Rule>& rule() const { return rule_; }
    const bool& adjustForLosses() const { return adjustForLosses_; }
    const vector<string>& quotes() override;
    //@}

    //! \name Setters
    //@{
    vector<string>& terms() { return terms_; }
    vector<string>& detachmentPoints() { return detachmentPoints_; }
    Size& settlementDays() { return settlementDays_; }
    Calendar& calendar() { return calendar_; }
    BusinessDayConvention& businessDayConvention() { return businessDayConvention_; }
    DayCounter& dayCounter() { return dayCounter_; }
    bool& extrapolate() { return extrapolate_; }
    QuantLib::Period& indexTerm() { return indexTerm_; }
    //@}

private:
    vector<string> detachmentPoints_;
    vector<string> terms_;
    Size settlementDays_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    DayCounter dayCounter_;
    bool extrapolate_;
    std::string quoteName_;
    QuantLib::Date startDate_;
    QuantLib::Period indexTerm_;
    boost::optional<QuantLib::DateGeneration::Rule> rule_;
    bool adjustForLosses_;
};
} // namespace data
} // namespace ore
