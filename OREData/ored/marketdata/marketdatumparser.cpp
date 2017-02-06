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

#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include <map>

using namespace std;
using QuantLib::WeekendsOnly;

namespace ore {
namespace data {

static MarketDatum::InstrumentType parseInstrumentType(const string& s) {
    static map<string, MarketDatum::InstrumentType> b = {{"ZERO", MarketDatum::InstrumentType::ZERO},
                                                         {"DISCOUNT", MarketDatum::InstrumentType::DISCOUNT},
                                                         {"MM", MarketDatum::InstrumentType::MM},
                                                         {"MM_FUTURE", MarketDatum::InstrumentType::MM_FUTURE},
                                                         {"FRA", MarketDatum::InstrumentType::FRA},
                                                         {"IR_SWAP", MarketDatum::InstrumentType::IR_SWAP},
                                                         {"BASIS_SWAP", MarketDatum::InstrumentType::BASIS_SWAP},
                                                         {"CC_BASIS_SWAP", MarketDatum::InstrumentType::CC_BASIS_SWAP},
                                                         {"CDS", MarketDatum::InstrumentType::CDS},
                                                         {"FX", MarketDatum::InstrumentType::FX_SPOT},
                                                         {"FX_SPOT", MarketDatum::InstrumentType::FX_SPOT},
                                                         {"FXFWD", MarketDatum::InstrumentType::FX_FWD},
                                                         {"FX_FWD", MarketDatum::InstrumentType::FX_FWD},
                                                         {"HAZARD_RATE", MarketDatum::InstrumentType::HAZARD_RATE},
                                                         {"RECOVERY_RATE", MarketDatum::InstrumentType::RECOVERY_RATE},
                                                         {"FX_FWD", MarketDatum::InstrumentType::FX_FWD},
                                                         {"SWAPTION", MarketDatum::InstrumentType::SWAPTION},
                                                         {"CAPFLOOR", MarketDatum::InstrumentType::CAPFLOOR},
                                                         {"FX_OPTION", MarketDatum::InstrumentType::FX_OPTION},
                                                         {"ZC_INFLATIONSWAP", MarketDatum::InstrumentType::ZC_INFLATIONSWAP},
                                                         {"ZC_INFLATIONCAPFLOOR", MarketDatum::InstrumentType::ZC_INFLATIONCAPFLOOR},
                                                         {"YY_INFLATIONSWAP", MarketDatum::InstrumentType::YY_INFLATIONSWAP},
                                                         {"SEASONALITY", MarketDatum::InstrumentType::SEASONALITY}};

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert " << s << " to InstrumentType");
    }
}

static MarketDatum::QuoteType parseQuoteType(const string& s) {
    static map<string, MarketDatum::QuoteType> b = {
        {"BASIS_SPREAD", MarketDatum::QuoteType::BASIS_SPREAD},
        {"CREDIT_SPREAD", MarketDatum::QuoteType::CREDIT_SPREAD},
        {"YIELD_SPREAD", MarketDatum::QuoteType::YIELD_SPREAD},
        {"RATE", MarketDatum::QuoteType::RATE},
        {"RATIO", MarketDatum::QuoteType::RATIO},
        {"PRICE", MarketDatum::QuoteType::PRICE},
        {"RATE_LNVOL", MarketDatum::QuoteType::RATE_LNVOL},
        {"RATE_GVOL", MarketDatum::QuoteType::RATE_LNVOL}, // deprecated
        {"RATE_NVOL", MarketDatum::QuoteType::RATE_NVOL},
        {"RATE_SLNVOL", MarketDatum::QuoteType::RATE_SLNVOL},
        {"SHIFT", MarketDatum::QuoteType::SHIFT},
    };

    if (s == "RATE_GVOL")
        LOG("Use of deprecated quote type RATE_GVOL");

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert " << s << " to QuoteType");
    }
}

//! Function to parse a market datum
boost::shared_ptr<MarketDatum> parseMarketDatum(const Date& asof, const string& datumName, const Real& value) {

    vector<string> tokens;
    boost::split(tokens, datumName, boost::is_any_of("/"));
    QL_REQUIRE(tokens.size() >= 2, "more than 2 tokens expected in " << datumName);

    MarketDatum::InstrumentType instrumentType = parseInstrumentType(tokens[0]);
    MarketDatum::QuoteType quoteType = parseQuoteType(tokens[1]);

    switch (instrumentType) {

    case MarketDatum::InstrumentType::ZERO: {
        // ZERO/RATE/EUR/EUR1D/A365/1Y
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE || quoteType == MarketDatum::QuoteType::YIELD_SPREAD,
                   "Invalid quote type for " << datumName);
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        DayCounter dc = parseDayCounter(tokens[4]);
        // token 5 can be a date, or tenor
        Date zeroDate = Date();
        Period tenor = Period();
        if (tokens[5].size() < 6) { // e.g. 3M, 50Y, 1Y6M
            tenor = parsePeriod(tokens[5]);
        } else {
            zeroDate = parseDate(tokens[5]);
        }
        return boost::make_shared<ZeroQuote>(value, asof, datumName, quoteType, ccy, zeroDate, dc, tenor);
    }

    case MarketDatum::InstrumentType::DISCOUNT: {
        // DISCOUNT/RATE/EUR/EUR1D/1Y
        // DISCOUNT/RATE/EUR/EUR1D/2016-12-15
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        // token 4 can be a date, or tenor
        Date discountDate;
        if (tokens[4].size() < 6) { // e.g. 3M, 50Y, 1Y6M
            // DiscountQuote takes a date.
            Period p = parsePeriod(tokens[4]);
            // we can't assume any calendar here, so we do the minimal adjustment
            // Just weekends.
            discountDate = WeekendsOnly().adjust(asof + p);
        } else {
            discountDate = parseDate(tokens[4]);
        }
        return boost::make_shared<DiscountQuote>(value, asof, datumName, quoteType, ccy, discountDate);
    }

    case MarketDatum::InstrumentType::MM: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        Period fwdStart = parsePeriod(tokens[3]);
        Period term = parsePeriod(tokens[4]);
        return boost::make_shared<MoneyMarketQuote>(value, asof, datumName, quoteType, ccy, fwdStart, term);
    }

    case MarketDatum::InstrumentType::MM_FUTURE: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        const string& expiry = tokens[3];
        const string& contract = tokens[4];
        Period term = parsePeriod(tokens[5]);
        return boost::make_shared<MMFutureQuote>(value, asof, datumName, quoteType, ccy, expiry, contract, term);
    }

    case MarketDatum::InstrumentType::FRA: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        Period fwdStart = parsePeriod(tokens[3]);
        Period term = parsePeriod(tokens[4]);
        return boost::make_shared<FRAQuote>(value, asof, datumName, quoteType, ccy, fwdStart, term);
    }

    case MarketDatum::InstrumentType::IR_SWAP: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        Period fwdStart = parsePeriod(tokens[3]);
        Period tenor = parsePeriod(tokens[4]);
        Period term = parsePeriod(tokens[5]);
        return boost::make_shared<SwapQuote>(value, asof, datumName, quoteType, ccy, fwdStart, term, tenor);
    }

    case MarketDatum::InstrumentType::BASIS_SWAP: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        Period flatTerm = parsePeriod(tokens[2]);
        Period term = parsePeriod(tokens[3]);
        const string& ccy = tokens[4];
        Period maturity = parsePeriod(tokens[5]);
        return boost::make_shared<BasisSwapQuote>(value, asof, datumName, quoteType, flatTerm, term, ccy, maturity);
    }

    case MarketDatum::InstrumentType::CC_BASIS_SWAP: {
        QL_REQUIRE(tokens.size() == 7, "7 tokens expected in " << datumName);
        const string& flatCcy = tokens[2];
        Period flatTerm = parsePeriod(tokens[3]);
        const string& ccy = tokens[4];
        Period term = parsePeriod(tokens[5]);
        Period maturity = parsePeriod(tokens[6]);
        return boost::make_shared<CrossCcyBasisSwapQuote>(value, asof, datumName, quoteType, flatCcy, flatTerm, ccy,
                                                          term, maturity);
    }

    case MarketDatum::InstrumentType::CDS: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& underlyingName = tokens[2];
        const string& seniority = tokens[3];
        const string& ccy = tokens[4];
        Period term = parsePeriod(tokens[5]);
        return boost::make_shared<CdsSpreadQuote>(value, asof, datumName, underlyingName, seniority, ccy, term);
    }

    case MarketDatum::InstrumentType::HAZARD_RATE: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& underlyingName = tokens[2];
        const string& seniority = tokens[3];
        const string& ccy = tokens[4];
        Period term = parsePeriod(tokens[5]);
        return boost::make_shared<HazardRateQuote>(value, asof, datumName, underlyingName, seniority, ccy, term);
    }

    case MarketDatum::InstrumentType::RECOVERY_RATE: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& underlyingName = tokens[2];
        const string& seniority = tokens[3];
        const string& ccy = tokens[4];
        return boost::make_shared<RecoveryRateQuote>(value, asof, datumName, underlyingName, seniority, ccy);
    }

    case MarketDatum::InstrumentType::CAPFLOOR: {
        QL_REQUIRE(tokens.size() == 8 || tokens.size() == 4, "Either 4 or 8 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        if (tokens.size() == 8) {
            Period term = parsePeriod(tokens[3]);
            Period tenor = parsePeriod(tokens[4]);
            bool atm = parseBool(tokens[5].c_str());
            bool relative = parseBool(tokens[6].c_str());
            Real strike = parseReal(tokens[7]);
            return boost::make_shared<CapFloorQuote>(value, asof, datumName, quoteType, ccy, term, tenor, atm, relative,
                                                     strike);
        } else {
            Period indexTenor = parsePeriod(tokens[3]);
            return boost::make_shared<CapFloorShiftQuote>(value, asof, datumName, quoteType, ccy, indexTenor);
        }
    }

    case MarketDatum::InstrumentType::SWAPTION: {
        QL_REQUIRE(tokens.size() == 4 || tokens.size() == 6 || tokens.size() == 7, "4, 6 or 7 tokens expected in "
                                                                                       << datumName);
        const string& ccy = tokens[2];
        Period expiry = tokens.size() >= 6 ? parsePeriod(tokens[3]) : Period(0 * QuantLib::Days);
        Period term = tokens.size() >= 6 ? parsePeriod(tokens[4]) : parsePeriod(tokens[3]);
        if (tokens.size() >= 6) { // volatility
            const string& dimension = tokens[5];
            Real strike = 0.0;
            if (dimension == "ATM")
                QL_REQUIRE(tokens.size() == 6, "6 tokens expected in ATM quote " << datumName);
            else if (dimension == "Smile") {
                QL_REQUIRE(tokens.size() == 7, "7 tokens expected in Smile quote " << datumName);
                strike = parseReal(tokens[6]);
            } else
                QL_FAIL("Swaption vol quote dimension " << dimension << " not recognised");
            return boost::make_shared<SwaptionQuote>(value, asof, datumName, quoteType, ccy, expiry, term, dimension,
                                                     strike);
        } else { // SLN volatility shift
            return boost::make_shared<SwaptionShiftQuote>(value, asof, datumName, quoteType, ccy, term);
        }
    }

    case MarketDatum::InstrumentType::FX_SPOT: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        const string& unitCcy = tokens[2];
        const string& ccy = tokens[3];
        return boost::make_shared<FXSpotQuote>(value, asof, datumName, quoteType, unitCcy, ccy);
    }

    case MarketDatum::InstrumentType::FX_FWD: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& unitCcy = tokens[2];
        const string& ccy = tokens[3];
        Period term = parsePeriod(tokens[4]);
        return boost::make_shared<FXForwardQuote>(value, asof, datumName, quoteType, unitCcy, ccy, term);
    }

    case MarketDatum::InstrumentType::FX_OPTION: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& unitCcy = tokens[2];
        const string& ccy = tokens[3];
        Period expiry = parsePeriod(tokens[4]);
        const string& strike = tokens[5];
        return boost::make_shared<FXOptionQuote>(value, asof, datumName, quoteType, unitCcy, ccy, expiry, strike);
    }

    case MarketDatum::InstrumentType::ZC_INFLATIONSWAP: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        const string& index = tokens[2];
        Period term = parsePeriod(tokens[3]);
        return boost::make_shared<ZcInflationSwapQuote>(value, asof, datumName, index, term);
    }
        
    case MarketDatum::InstrumentType::YY_INFLATIONSWAP: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        const string& index = tokens[2];
        Period term = parsePeriod(tokens[3]);
        return boost::make_shared<YoYInflationSwapQuote>(value, asof, datumName, index, term);
    }
        
    case MarketDatum::InstrumentType::ZC_INFLATIONCAPFLOOR: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& index = tokens[2];
        Period term = parsePeriod(tokens[3]);
        QL_REQUIRE(tokens[4] == "C" || tokens[4] == "F", "excepted C or F for Cap or Floor at position 5 in "
                   << datumName);
        bool isCap = tokens[4] == "C";
        string strike = tokens[5];
        return boost::make_shared<ZcInflationCapFloorQuote>(value, asof, datumName, quoteType, index, term, isCap,
                                                            strike);
    }
    
    case MarketDatum::InstrumentType::SEASONALITY: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& index = tokens[3];
        const string& type = tokens[2];
        const string& month = tokens[4];
        return boost::make_shared<SeasonalityQuote>(value, asof, datumName, index, type, month);
    }

    default:
        QL_FAIL("Cannot convert \"" << datumName << "\" to MarketDatum");
    }
}
}
}
