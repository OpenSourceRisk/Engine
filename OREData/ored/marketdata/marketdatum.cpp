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

#include <boost/lexical_cast.hpp>
#include <ored/marketdata/marketdatum.hpp>
#include <ored/utilities/parsers.hpp>

using boost::lexical_cast;
using boost::bad_lexical_cast;

namespace ore {
namespace data {

EquityOptionQuote::EquityOptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType,
                                     string equityName, string ccy, string expiry, string strike)
    : MarketDatum(value, asofDate, name, quoteType, InstrumentType::EQUITY_OPTION), eqName_(equityName), ccy_(ccy),
      expiry_(expiry), strike_(strike) {

    // we will call a parser on the expiry string, to ensure it is a correctly-formatted date or tenor
    Date tmpDate;
    Period tmpPeriod;
    bool tmpBool;
    parseDateOrPeriod(expiry, tmpDate, tmpPeriod, tmpBool);
}

Natural MMFutureQuote::expiryYear() const {
    QL_REQUIRE(expiry_.length() == 7, "The expiry string must be of "
                                      "the form YYYY-MM");
    string strExpiryYear = expiry_.substr(0, 4);
    Natural expiryYear;
    try {
        expiryYear = lexical_cast<Natural>(strExpiryYear);
    } catch (const bad_lexical_cast&) {
        QL_FAIL("Could not convert year string, " << strExpiryYear << ", to number.");
    }
    return expiryYear;
}

Month MMFutureQuote::expiryMonth() const {
    QL_REQUIRE(expiry_.length() == 7, "The expiry string must be of "
                                      "the form YYYY-MM");
    string strExpiryMonth = expiry_.substr(5);
    Natural expiryMonth;
    try {
        expiryMonth = lexical_cast<Natural>(strExpiryMonth);
    } catch (const bad_lexical_cast&) {
        QL_FAIL("Could not convert month string, " << strExpiryMonth << ", to number.");
    }
    return static_cast<Month>(expiryMonth);
}

QuantLib::Size SeasonalityQuote::applyMonth() const {
    QL_REQUIRE(month_.length() == 3, "The month string must be of "
                                     "the form MMM");
    std::vector<std::string> allMonths = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    QuantLib::Size applyMonth;
    auto it = std::find(allMonths.begin(), allMonths.end(), month_);
    if (it != allMonths.end()) {
        applyMonth = std::distance(allMonths.begin(), it) + 1;
    } else {
        QL_FAIL("Unknown month string: " << month_);
    }
    return applyMonth;
}

CommodityOptionQuote::CommodityOptionQuote(Real value, const Date& asof, const string& name, QuoteType quoteType,
    const string& commodityName, const string& quoteCurrency, const string& expiry, const string& strike)
    : MarketDatum(value, asof, name, quoteType, InstrumentType::COMMODITY_OPTION),
      commodityName_(commodityName), quoteCurrency_(quoteCurrency), expiry_(expiry), strike_(strike) {

    // If strike is not ATMF, it must parse to Real
    if (strike != "ATMF") {
        Real result;
        QL_REQUIRE(tryParseReal(strike_, result), "Commodity option quote strike (" << strike_ << 
            ") must be either ATMF or an actual strike price");
    }

    // Call parser to check that the expiry_ resolves to a period or a date
    Date outDate;
    Period outPeriod;
    bool outBool;
    parseDateOrPeriod(expiry_, outDate, outPeriod, outBool);
}

} // namespace data
} // namespace ore
