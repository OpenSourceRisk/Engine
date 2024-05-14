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

/*! \file ored/configuration/equityvolcurveconfig.hpp
    \brief Equity volatility curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ored/configuration/onedimsolverconfig.hpp>
#include <ored/configuration/volatilityconfig.hpp>
#include <ored/configuration/reportconfig.hpp>
#include <ored/marketdata/marketdatum.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/types.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using QuantLib::DayCounter;
using QuantLib::Period;
using std::string;
using std::vector;

//! Equity volatility structure configuration
/*!
  \ingroup configuration
*/
class EquityVolatilityCurveConfig : public CurveConfig {
public:
    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    EquityVolatilityCurveConfig() {}
    //! Detailed constructor
    EquityVolatilityCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                                const std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>>& volatilityConfig,
                                const string& equityId = string(),
                                const string& dayCounter = "A365", const string& calendar = "NullCalendar",
                                const OneDimSolverConfig& solverConfig = OneDimSolverConfig(),
                                const boost::optional<bool>& preferOutOfTheMoney = boost::none);
    EquityVolatilityCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                                const QuantLib::ext::shared_ptr<VolatilityConfig>& volatilityConfig,
                                const string& equityId = string(),
                                const string& dayCounter = "A365", const string& calendar = "NullCalendar",
                                const OneDimSolverConfig& solverConfig = OneDimSolverConfig(),
                                const boost::optional<bool>& preferOutOfTheMoney = boost::none);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const string& equityId() const { return equityId_.empty() ? curveID_ : equityId_; }
    const string& ccy() const { return parseCurrencyWithMinors(ccy_).code(); }
    const string& dayCounter() const { return dayCounter_; }
    const string& calendar() const { return calendar_; }
    const std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>>& volatilityConfig() const { return volatilityConfig_; }
    const string quoteStem(const std::string& volType) const;
    void populateQuotes();
    bool isProxySurface();
    OneDimSolverConfig solverConfig() const;
    const boost::optional<bool>& preferOutOfTheMoney() const {
        return preferOutOfTheMoney_;
    }
    const ReportConfig& reportConfig() const { return reportConfig_; }
    //@}

    //! \name Setters
    //@{
    string& ccy() { return ccy_; }
    string& dayCounter() { return dayCounter_; }
    //@}

private:
    void populateRequiredCurveIds();

    string ccy_;
    std::vector<QuantLib::ext::shared_ptr<VolatilityConfig>> volatilityConfig_;
    string equityId_;
    string dayCounter_;
    string calendar_;
    OneDimSolverConfig solverConfig_;
    boost::optional<bool> preferOutOfTheMoney_;
    ReportConfig reportConfig_;

    // Return a default solver configuration. Used by solverConfig() if solverConfig_ is empty.
    static OneDimSolverConfig defaultSolverConfig();
};
} // namespace data
} // namespace ore
