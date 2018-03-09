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

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <map>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

using namespace std;
using QuantLib::WeekendsOnly;

namespace ore {
namespace data {

static MarketDatum::InstrumentType parseInstrumentType(const string& s) {

    static map<string, MarketDatum::InstrumentType> b = {
        {"ZERO", MarketDatum::InstrumentType::ZERO},
        {"DISCOUNT", MarketDatum::InstrumentType::DISCOUNT},
        {"MM", MarketDatum::InstrumentType::MM},
        {"MM_FUTURE", MarketDatum::InstrumentType::MM_FUTURE},
        {"FRA", MarketDatum::InstrumentType::FRA},
        {"IMM_FRA", MarketDatum::InstrumentType::IMM_FRA},
        {"IR_SWAP", MarketDatum::InstrumentType::IR_SWAP},
        {"BASIS_SWAP", MarketDatum::InstrumentType::BASIS_SWAP},
        {"CC_BASIS_SWAP", MarketDatum::InstrumentType::CC_BASIS_SWAP},
        {"CDS", MarketDatum::InstrumentType::CDS},
        {"CDS_INDEX", MarketDatum::InstrumentType::CDS_INDEX},
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
        {"EQUITY", MarketDatum::InstrumentType::EQUITY_SPOT},
        {"EQUITY_FWD", MarketDatum::InstrumentType::EQUITY_FWD},
        {"EQUITY_DIVIDEND", MarketDatum::InstrumentType::EQUITY_DIVIDEND},
        {"EQUITY_OPTION", MarketDatum::InstrumentType::EQUITY_OPTION},
        {"BOND", MarketDatum::InstrumentType::BOND},
        {"ZC_INFLATIONSWAP", MarketDatum::InstrumentType::ZC_INFLATIONSWAP},
        {"ZC_INFLATIONCAPFLOOR", MarketDatum::InstrumentType::ZC_INFLATIONCAPFLOOR},
        {"YY_INFLATIONSWAP", MarketDatum::InstrumentType::YY_INFLATIONSWAP},
        {"SEASONALITY", MarketDatum::InstrumentType::SEASONALITY},
        {"INDEX_CDS_OPTION", MarketDatum::InstrumentType::INDEX_CDS_OPTION}};

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
        {"BASE_CORRELATION", MarketDatum::QuoteType::BASE_CORRELATION},
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

// calls parseDateOrPeriod and returns a Date (either the supplied date or asof+period)
static Date getDateFromDateOrPeriod(const string& token, Date asof) {
    Period term;                                           // gets populated by parseDateOrPeriod
    Date expiryDate;                                       // gets populated by parseDateOrPeriod
    bool tmpIsDate;                                        // gets populated by parseDateOrPeriod
    parseDateOrPeriod(token, expiryDate, term, tmpIsDate); // checks if the market string contains a date or a period
    if (!tmpIsDate)
        expiryDate =
            WeekendsOnly().adjust(asof + term); // we have no calendar information here, so we use a generic calendar
    return expiryDate;
}

//! Function to parse a market datum
boost::shared_ptr<MarketDatum> parseMarketDatum(const Date& asof, const string& datumName, const Real& value) {

    vector<string> tokens;
    boost::split(tokens, datumName, boost::is_any_of("/"));
    QL_REQUIRE(tokens.size() > 2, "more than 2 tokens expected in " << datumName);

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
        Date date = Date();
        Period tenor = Period();
        bool isDate;
        parseDateOrPeriod(tokens[5], date, tenor, isDate);
        return boost::make_shared<ZeroQuote>(value, asof, datumName, quoteType, ccy, date, dc, tenor);
    }

    case MarketDatum::InstrumentType::DISCOUNT: {
        // DISCOUNT/RATE/EUR/EUR1D/1Y
        // DISCOUNT/RATE/EUR/EUR1D/2016-12-15
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        // token 4 can be a date, or tenor
        Date date = Date();
        Period tenor = Period();
        bool isDate;
        parseDateOrPeriod(tokens[4], date, tenor, isDate);
        if (!isDate) {
            // we can't assume any calendar here, so we do the minimal adjustment with a weekend only calendar
            QL_REQUIRE(tenor != Period(), "neither date nor tenor recognised");
            date = WeekendsOnly().adjust(asof + tenor);
        }
        return boost::make_shared<DiscountQuote>(value, asof, datumName, quoteType, ccy, date);
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

    case MarketDatum::InstrumentType::IMM_FRA: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        string imm1 = tokens[3];
        string imm2 = tokens[4];
        unsigned int m1 = parseInteger(imm1);
        unsigned int m2 = parseInteger(imm2);
        QL_REQUIRE(m2 > m1, "Second IMM date must be after the first in " << datumName);
        return boost::make_shared<ImmFraQuote>(value, asof, datumName, quoteType, ccy, m1, m2);
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
        QL_REQUIRE(tokens.size() == 3 || tokens.size() == 5, "3 or 5 tokens expected in " << datumName);
        const string& underlyingName = tokens[2]; // issuer name for CDS, security ID for bond specific RRs
        string seniority = "";
        string ccy = "";
        if (tokens.size() == 5) {
            // CDS
            seniority = tokens[3];
            ccy = tokens[4];
        }
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
        QL_REQUIRE(tokens.size() == 4 || tokens.size() == 6 || tokens.size() == 7,
                   "4, 6 or 7 tokens expected in " << datumName);
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
        QL_REQUIRE(tokens[4] == "C" || tokens[4] == "F",
                   "excepted C or F for Cap or Floor at position 5 in " << datumName);
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
    case MarketDatum::InstrumentType::EQUITY_SPOT: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::PRICE, "Invalid quote type for " << datumName);
        const string& equityName = tokens[2];
        const string& ccy = tokens[3];
        return boost::make_shared<EquitySpotQuote>(value, asof, datumName, quoteType, equityName, ccy);
    }

    case MarketDatum::InstrumentType::EQUITY_FWD: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::PRICE, "Invalid quote type for " << datumName);
        const string& equityName = tokens[2];
        const string& ccy = tokens[3];
        Date expiryDate = getDateFromDateOrPeriod(tokens[4], asof);
        return boost::make_shared<EquityForwardQuote>(value, asof, datumName, quoteType, equityName, ccy, expiryDate);
    }

    case MarketDatum::InstrumentType::EQUITY_DIVIDEND: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE, "Invalid quote type for " << datumName);
        const string& equityName = tokens[2];
        const string& ccy = tokens[3];
        Date tenorDate = getDateFromDateOrPeriod(tokens[4], asof);
        return boost::make_shared<EquityDividendYieldQuote>(value, asof, datumName, quoteType, equityName, ccy,
                                                            tenorDate);
    }

    case MarketDatum::InstrumentType::EQUITY_OPTION: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE_LNVOL, "Invalid quote type for " << datumName);
        const string& equityName = tokens[2];
        const string& ccy = tokens[3];
        string expiryString = tokens[4];
        const string& strike = tokens[5];
        // note how we only store the expiry string - to ensure we can support both Periods and Dates being specified in
        // the vol curve-config.
        return boost::make_shared<EquityOptionQuote>(value, asof, datumName, quoteType, equityName, ccy, expiryString,
                                                     strike);
    }

    case MarketDatum::InstrumentType::BOND: {
        QL_REQUIRE(tokens.size() == 3, "3 tokens expected in " << datumName);
        const string& securityID = tokens[2];
        return boost::make_shared<SecuritySpreadQuote>(value, asof, datumName, securityID);
    }

    case MarketDatum::InstrumentType::CDS_INDEX: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::BASE_CORRELATION, "Invalid quote type for " << datumName);
        const string& cdsIndexName = tokens[2];
        Period term = parsePeriod(tokens[3]);
        Real detachmentPoint = parseReal(tokens[4]);
        return boost::make_shared<BaseCorrelationQuote>(value, asof, datumName, quoteType, cdsIndexName, term,
                                                        detachmentPoint);
    }

    case MarketDatum::InstrumentType::INDEX_CDS_OPTION: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE_LNVOL, "Invalid quote type for " << datumName);
        const string& indexName = tokens[2];
        const string& expiry = tokens[3];
        return boost::make_shared<IndexCDSOptionQuote>(value, asof, datumName, indexName, expiry);
    }

    default:
        QL_FAIL("Cannot convert \"" << datumName << "\" to MarketDatum");
    } // switch instrument type
} // parseMarketDatum
} // namespace data
} // namespace ore
