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

/*! \file ored/configuration/swaptionvolcurveconfig.hpp
    \brief Swaption volatility curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ored/marketdata/marketdatum.hpp>
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
using QuantLib::Null;
using QuantLib::Period;
using QuantLib::Spread;
using std::string;
using std::vector;

//! Correlation curve configuration
/*!
  \ingroup configuration
*/
class CorrelationCurveConfig : public CurveConfig {
public:
    //! supported Correlation types
    enum class CorrelationType { CMSSpread, Generic };
    //! supported Correlation dimensions
    enum class Dimension { ATM, Constant };

    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    CorrelationCurveConfig() {}
    //! Detailed constructor
    CorrelationCurveConfig(const string& curveID, const string& curveDescription, const Dimension& dimension,
                           const CorrelationType& corrType, const string& conventions, const MarketDatum::QuoteType& quoteType,
                           const bool extrapolate, const vector<string>& optionTenors, const DayCounter& dayCounter,
                           const Calendar& calendar, const BusinessDayConvention& businessDayConvention,
                           const string& index1, const string& index2, const string& currency,
                           const string& swaptionVol = "", const string& discountCurve = "");
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    const CorrelationType& correlationType() const { return correlationType_; }
    const string& conventions() const { return conventions_; }
    const Dimension& dimension() const { return dimension_; }
    const MarketDatum::QuoteType& quoteType() const { return quoteType_; }
    const bool& extrapolate() const { return extrapolate_; }
    const vector<string>& optionTenors() const { return optionTenors_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const string& index1() const { return index1_; }
    const string& index2() const { return index2_; }
    const string& currency() const { return currency_; }
    const string& swaptionVolatility() const { return swaptionVol_; }
    const string& discountCurve() const { return discountCurve_; }
    const vector<string>& quotes() override;

    //@}

    //! \name Setters
    //@{
    CorrelationType& correlationType() { return correlationType_; }
    string& conventions() { return conventions_; }
    Dimension& dimension() { return dimension_; }
    MarketDatum::QuoteType& quoteType() { return quoteType_; }
    bool& extrapolate() { return extrapolate_; }
    vector<string>& optionTenors() { return optionTenors_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Calendar& calendar() { return calendar_; }
    string& index1() { return index1_; }
    string& index2() { return index2_; }
    string& currency() { return currency_; }
    string& swaptionVolatility() { return swaptionVol_; }
    string& discountCurve() { return discountCurve_; }

    //@}

private:
    void populateRequiredCurveIds();

    Dimension dimension_;
    CorrelationType correlationType_;
    string conventions_;
    MarketDatum::QuoteType quoteType_;
    bool extrapolate_;
    vector<string> optionTenors_;
    DayCounter dayCounter_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    string index1_, index2_;
    string currency_;
    string swaptionVol_;
    string discountCurve_;
};

// Correlation pairs ordering, by convention we use pairs Index1:Index2 with
// Index2 < Index1, where the ordering on index names is defined via
// 1) CMS > Ibor > FX > EQ > COM
// 2) Tenor
// 3) currency / name (alphabetical)
// Eg. EUR-CMS-10Y:GBP-LIBOR-6M, GBP-LIBOR-6M:FX-ECB-EUR-USD,
// EUR-CMS-10Y:EUR-CMS-2Y, GBP-CMS-10Y:EUR-CMS-2Y
bool indexNameLessThan(const std::string& index1, const std::string& index2);

} // namespace data
} // namespace ore
