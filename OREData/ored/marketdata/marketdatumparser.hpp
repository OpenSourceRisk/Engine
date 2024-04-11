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

/*! \file ored/marketdata/marketdatumparser.hpp
    \brief Market Datum parser
    \ingroup marketdata
*/

#pragma once

#include <ored/marketdata/marketdatum.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>
#include <string>

namespace ore {
namespace data {
using QuantLib::Date;
using QuantLib::Real;
using std::string;

//! Function to parse a market datum
/*! \ingroup marketdata
 */
QuantLib::ext::shared_ptr<MarketDatum> parseMarketDatum(const Date&, const string&, const Real&);

//! Get a date from a date string or period
/*! \ingroup marketdata
 */
Date getDateFromDateOrPeriod(const string& token, Date asof, QuantLib::Calendar cal = QuantLib::WeekendsOnly(), 
    QuantLib::BusinessDayConvention bdc = QuantLib::BusinessDayConvention::Following);

//! Convert text to QuantLib::Period of Fx forward string
/*!
  \ingroup marketdata
 */
boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString> parseFxPeriod(const string& s);


QuantLib::Period fxFwdQuoteTenor(const boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString>& term);


QuantLib::Period fxFwdQuoteStartTenor(const boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString>& term,
                                      const QuantLib::ext::shared_ptr<FXConvention>& fxConvention = nullptr);

bool matchFxFwdStringTerm(const boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString>& term,
                          const FXForwardQuote::FxFwdString& fxfwdString);

} // namespace data
} // namespace ore
