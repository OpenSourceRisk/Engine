/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/configuration/volatilityconfig.hpp>
#include <ored/marketdata/marketdatum.hpp>
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
                                const boost::shared_ptr<VolatilityConfig>& volatilityConfig,
                                const string& dayCounter = "A365", const string& calendar = "NullCalendar",
                                const std::string& equityCurveId = "", const std::string& yieldCurveId = "");
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const string& ccy() const { return ccy_; }
    const MarketDatum::QuoteType& quoteType() const { return volatilityConfig_->quoteType(); }
    const QuantLib::Exercise::Type& exerciseType() const { return volatilityConfig_->exerciseType(); }
    const string& dayCounter() const { return dayCounter_; }
    const string& calendar() const { return calendar_; }
    const boost::shared_ptr<VolatilityConfig>& volatilityConfig() const { return volatilityConfig_; };
    const string& proxySurface() const { return proxySurface_; }
    const string quoteStem() const;
    bool isProxySurface() { return !proxySurface_.empty(); };
    const std::string& equityCurveId() const { return equityCurveId_; };
    const std::string& yieldCurveId() const { return yieldCurveId_; };
    //@}

    //! \name Setters
    //@{
    string& ccy() { return ccy_; }
    string& dayCounter() { return dayCounter_; }
    //@}

private:
    string ccy_;
    boost::shared_ptr<VolatilityConfig> volatilityConfig_;
    string dayCounter_;
    string calendar_;
    string proxySurface_;
    std::string equityCurveId_;
    std::string yieldCurveId_;

    //! Populate CurveConfig::quotes_ with the required quotes.
    void populateQuotes();
};
} // namespace data
} // namespace ore
