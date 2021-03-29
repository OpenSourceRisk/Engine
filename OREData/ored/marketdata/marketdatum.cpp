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

using boost::bad_lexical_cast;
using boost::lexical_cast;

namespace ore {
namespace data {

std::ostream& operator<<(std::ostream& out, const MarketDatum::QuoteType& type) {
    switch (type) {
    case MarketDatum::QuoteType::BASIS_SPREAD:
        return out << "BASIS_SPREAD";
    case MarketDatum::QuoteType::CREDIT_SPREAD:
        return out << "CREDIT_SPREAD";
    case MarketDatum::QuoteType::YIELD_SPREAD:
        return out << "YIELD_SPREAD";
    case MarketDatum::QuoteType::RATE:
        return out << "RATE";
    case MarketDatum::QuoteType::RATIO:
        return out << "RATIO";
    case MarketDatum::QuoteType::PRICE:
        return out << "PRICE";
    case MarketDatum::QuoteType::RATE_LNVOL:
        return out << "RATE_LNVOL";
    case MarketDatum::QuoteType::RATE_NVOL:
        return out << "RATE_NVOL";
    case MarketDatum::QuoteType::RATE_SLNVOL:
        return out << "RATE_SLNVOL";
    case MarketDatum::QuoteType::BASE_CORRELATION:
        return out << "BASE_CORRELATION";
    case MarketDatum::QuoteType::SHIFT:
        return out << "SHIFT";
    default:
        return out << "?";
    }
}

EquityOptionQuote::EquityOptionQuote(Real value, Date asofDate, const string& name, QuoteType quoteType,
                                     string equityName, string ccy, string expiry,
                                     const boost::shared_ptr<BaseStrike>& strike, bool isCall)
    : MarketDatum(value, asofDate, name, quoteType, InstrumentType::EQUITY_OPTION), eqName_(equityName), ccy_(ccy),
      expiry_(expiry), strike_(strike), isCall_(isCall) {

    // we will call a parser on the expiry string, to ensure it is a correctly-formatted date or tenor
    Date tmpDate;
    Period tmpPeriod;
    bool tmpBool;
    parseDateOrPeriod(expiry, tmpDate, tmpPeriod, tmpBool);
}

IndexCDSOptionQuote::IndexCDSOptionQuote(QuantLib::Real value, const Date& asof, const string& name,
                                         const string& indexName, const boost::shared_ptr<Expiry>& expiry,
                                         const string& indexTerm, const boost::shared_ptr<BaseStrike>& strike)
    : MarketDatum(value, asof, name, QuoteType::RATE_LNVOL, InstrumentType::INDEX_CDS_OPTION), indexName_(indexName),
      expiry_(expiry), indexTerm_(indexTerm), strike_(strike) {}

namespace {
Natural yearFromExpiryString(const std::string& expiry) {
    QL_REQUIRE(expiry.length() == 7, "The expiry string must be of "
                                     "the form YYYY-MM");
    string strExpiryYear = expiry.substr(0, 4);
    Natural expiryYear;
    try {
        expiryYear = lexical_cast<Natural>(strExpiryYear);
    } catch (const bad_lexical_cast&) {
        QL_FAIL("Could not convert year string, " << strExpiryYear << ", to number.");
    }
    return expiryYear;
}

Month monthFromExpiryString(const std::string& expiry) {
    QL_REQUIRE(expiry.length() == 7, "The expiry string must be of "
                                     "the form YYYY-MM");
    string strExpiryMonth = expiry.substr(5);
    Natural expiryMonth;
    try {
        expiryMonth = lexical_cast<Natural>(strExpiryMonth);
    } catch (const bad_lexical_cast&) {
        QL_FAIL("Could not convert month string, " << strExpiryMonth << ", to number.");
    }
    return static_cast<Month>(expiryMonth);
}
} // namespace

Natural MMFutureQuote::expiryYear() const { return yearFromExpiryString(expiry_); }

Month MMFutureQuote::expiryMonth() const { return monthFromExpiryString(expiry_); }

Natural OIFutureQuote::expiryYear() const { return yearFromExpiryString(expiry_); }

Month OIFutureQuote::expiryMonth() const { return monthFromExpiryString(expiry_); }

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
                                           const string& commodityName, const string& quoteCurrency,
                                           const boost::shared_ptr<Expiry>& expiry,
                                           const boost::shared_ptr<BaseStrike>& strike)
    : MarketDatum(value, asof, name, quoteType, InstrumentType::COMMODITY_OPTION), commodityName_(commodityName),
      quoteCurrency_(quoteCurrency), expiry_(expiry), strike_(strike) {}

CorrelationQuote::CorrelationQuote(Real value, const Date& asof, const string& name, QuoteType quoteType,
                                   const string& index1, const string& index2, const string& expiry,
                                   const string& strike)
    : MarketDatum(value, asof, name, quoteType, InstrumentType::CORRELATION), index1_(index1), index2_(index2),
      expiry_(expiry), strike_(strike) {

    // If strike is not ATM, it must parse to Real
    if (strike != "ATM") {
        Real result;
        QL_REQUIRE(tryParseReal(strike_, result),
                   "Commodity option quote strike (" << strike_ << ") must be either ATM or an actual strike price");
    }

    // Call parser to check that the expiry_ resolves to a period or a date
    Date outDate;
    Period outPeriod;
    bool outBool;
    parseDateOrPeriod(expiry_, outDate, outPeriod, outBool);
}

} // namespace data
} // namespace ore

BOOST_CLASS_EXPORT_GUID(ore::data::MoneyMarketQuote, "MoneyMarketQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::FRAQuote, "FRAQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::ImmFraQuote, "ImmFraQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SwapQuote, "SwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::ZeroQuote, "ZeroQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::DiscountQuote, "DiscountQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::MMFutureQuote, "MMFutureQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::OIFutureQuote, "OIFutureQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BasisSwapQuote, "BasisSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BMASwapQuote, "BMASwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CrossCcyBasisSwapQuote, "CrossCcyBasisSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CrossCcyFixFloatSwapQuote, "CrossCcyFixFloatSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CdsQuote, "CdsQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::HazardRateQuote, "HazardRateQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::RecoveryRateQuote, "RecoveryRateQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SwaptionQuote, "SwaptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SwaptionShiftQuote, "SwaptionShiftQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BondOptionQuote, "BondOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BondOptionShiftQuote, "BondOptionShiftQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CapFloorQuote, "CapFloorQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CapFloorShiftQuote, "CapFloorShiftQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::FXSpotQuote, "FXSpotQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::FXForwardQuote, "FXForwardQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::FXOptionQuote, "FXOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::ZcInflationSwapQuote, "ZcInflationSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::InflationCapFloorQuote, "InflationCapFloorQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::ZcInflationCapFloorQuote, "ZcInflationCapFloorQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::YoYInflationSwapQuote, "YoYInflationSwapQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::YyInflationCapFloorQuote, "YyInflationCapFloorQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SeasonalityQuote, "SeasonalityQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::EquitySpotQuote, "EquitySpotQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::EquityForwardQuote, "EquityForwardQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::EquityDividendYieldQuote, "EquityDividendYieldQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::EquityOptionQuote, "EquityOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::SecuritySpreadQuote, "SecuritySpreadQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BaseCorrelationQuote, "BaseCorrelationQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::IndexCDSOptionQuote, "IndexCDSOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CommoditySpotQuote, "CommoditySpotQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CommodityForwardQuote, "CommodityForwardQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CommodityOptionQuote, "CommodityOptionQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CorrelationQuote, "CorrelationQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::CPRQuote, "CPRQuote");
BOOST_CLASS_EXPORT_GUID(ore::data::BondPriceQuote, "BondPriceQuote");
