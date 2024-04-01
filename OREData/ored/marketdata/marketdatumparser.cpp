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
#include <boost/range.hpp>
#include <map>
#include <ored/marketdata/expiry.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/marketdata/strike.hpp>
#include <ored/portfolio/creditdefaultswapdata.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using namespace std;
using QuantLib::Currency;
using QuantLib::Days;
using QuantLib::WeekendsOnly;

namespace ore {
namespace data {

static MarketDatum::InstrumentType parseInstrumentType(const string& s) {

    static map<string, MarketDatum::InstrumentType> b = {
        {"ZERO", MarketDatum::InstrumentType::ZERO},
        {"DISCOUNT", MarketDatum::InstrumentType::DISCOUNT},
        {"MM", MarketDatum::InstrumentType::MM},
        {"MM_FUTURE", MarketDatum::InstrumentType::MM_FUTURE},
        {"OI_FUTURE", MarketDatum::InstrumentType::OI_FUTURE},
        {"FRA", MarketDatum::InstrumentType::FRA},
        {"IMM_FRA", MarketDatum::InstrumentType::IMM_FRA},
        {"IR_SWAP", MarketDatum::InstrumentType::IR_SWAP},
        // {"IR_DATED_SWAP", MarketDatum::InstrumentType::IR_DATED_SWAP},
        {"BASIS_SWAP", MarketDatum::InstrumentType::BASIS_SWAP},
        {"CC_BASIS_SWAP", MarketDatum::InstrumentType::CC_BASIS_SWAP},
        {"CC_FIX_FLOAT_SWAP", MarketDatum::InstrumentType::CC_FIX_FLOAT_SWAP},
        {"BMA_SWAP", MarketDatum::InstrumentType::BMA_SWAP},
        {"CDS", MarketDatum::InstrumentType::CDS},
        {"CDS_INDEX", MarketDatum::InstrumentType::CDS_INDEX},
        {"FX", MarketDatum::InstrumentType::FX_SPOT},
        {"FX_SPOT", MarketDatum::InstrumentType::FX_SPOT},
        {"FXFWD", MarketDatum::InstrumentType::FX_FWD},
        {"FX_FWD", MarketDatum::InstrumentType::FX_FWD},
        {"HAZARD_RATE", MarketDatum::InstrumentType::HAZARD_RATE},
        {"RECOVERY_RATE", MarketDatum::InstrumentType::RECOVERY_RATE},
        {"SWAPTION", MarketDatum::InstrumentType::SWAPTION},
        {"CAPFLOOR", MarketDatum::InstrumentType::CAPFLOOR},
        {"FX_OPTION", MarketDatum::InstrumentType::FX_OPTION},
        {"EQUITY", MarketDatum::InstrumentType::EQUITY_SPOT},
        {"EQUITY_FWD", MarketDatum::InstrumentType::EQUITY_FWD},
        {"EQUITY_DIVIDEND", MarketDatum::InstrumentType::EQUITY_DIVIDEND},
        {"EQUITY_OPTION", MarketDatum::InstrumentType::EQUITY_OPTION},
        {"BOND", MarketDatum::InstrumentType::BOND},
        {"BOND_OPTION", MarketDatum::InstrumentType::BOND_OPTION},
        {"ZC_INFLATIONSWAP", MarketDatum::InstrumentType::ZC_INFLATIONSWAP},
        {"ZC_INFLATIONCAPFLOOR", MarketDatum::InstrumentType::ZC_INFLATIONCAPFLOOR},
        {"YY_INFLATIONSWAP", MarketDatum::InstrumentType::YY_INFLATIONSWAP},
        {"YY_INFLATIONCAPFLOOR", MarketDatum::InstrumentType::YY_INFLATIONCAPFLOOR},
        {"SEASONALITY", MarketDatum::InstrumentType::SEASONALITY},
        {"INDEX_CDS_OPTION", MarketDatum::InstrumentType::INDEX_CDS_OPTION},
        {"COMMODITY", MarketDatum::InstrumentType::COMMODITY_SPOT},
        {"COMMODITY_FWD", MarketDatum::InstrumentType::COMMODITY_FWD},
        {"CORRELATION", MarketDatum::InstrumentType::CORRELATION},
        {"COMMODITY_OPTION", MarketDatum::InstrumentType::COMMODITY_OPTION},
        {"CPR", MarketDatum::InstrumentType::CPR},
        {"RATING", MarketDatum::InstrumentType::RATING}};

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert \"" << s << "\" to InstrumentType");
    }
}

static MarketDatum::QuoteType parseQuoteType(const string& s) {
    static map<string, MarketDatum::QuoteType> b = {
        {"BASIS_SPREAD", MarketDatum::QuoteType::BASIS_SPREAD},
        {"CREDIT_SPREAD", MarketDatum::QuoteType::CREDIT_SPREAD},
        {"CONV_CREDIT_SPREAD", MarketDatum::QuoteType::CONV_CREDIT_SPREAD},
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
        {"NULL", MarketDatum::QuoteType::NONE},
        {"TRANSITION_PROBABILITY", MarketDatum::QuoteType::TRANSITION_PROBABILITY}};

    if (s == "RATE_GVOL")
        LOG("Use of deprecated quote type RATE_GVOL");

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert \"" << s << "\" to QuoteType");
    }
}

// calls parseDateOrPeriod and returns a Date (either the supplied date or asof+period)
Date getDateFromDateOrPeriod(const string& token, Date asof, QuantLib::Calendar cal, QuantLib::BusinessDayConvention bdc) {
    Period term;                                           // gets populated by parseDateOrPeriod
    Date expiryDate;                                       // gets populated by parseDateOrPeriod
    bool tmpIsDate;                                        // gets populated by parseDateOrPeriod
    parseDateOrPeriod(token, expiryDate, term, tmpIsDate); // checks if the market string contains a date or a period
    if (!tmpIsDate)
        expiryDate = cal.adjust(asof + term, bdc);
    return expiryDate;
}

static FXForwardQuote::FxFwdString parseFxString(const string& s) {
    static map<string, FXForwardQuote::FxFwdString> b = {
        {"ON", FXForwardQuote::FxFwdString::ON},
        {"TN", FXForwardQuote::FxFwdString::TN},
        {"SN", FXForwardQuote::FxFwdString::SN}
    };

    auto it = b.find(s);
    if (it != b.end()) {
        return it->second;
    } else {
        QL_FAIL("Cannot convert \"" << s << "\" to FxFwdString");
    }
}

boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString> parseFxPeriod(const string& s) {
    bool isPeriod = isdigit(s.front());
    if (isPeriod)
        return parsePeriod(s);
    else
        return parseFxString(s);
}

namespace {

struct FxTenorGetter : boost::static_visitor<Period> {
    FxTenorGetter() {}
    Period operator()(const Period& p) const { return p; }
    Period operator()(const FXForwardQuote::FxFwdString& p) const { 
        // These are all overnight rates
        return Period(1, Days);
    }
};

struct FxStartTenorGetter : boost::static_visitor<Period> {
    
    FxStartTenorGetter(const QuantLib::ext::shared_ptr<FXConvention>& fxConvention) :
        fxConvention(fxConvention) {}    
    
    Period operator()(const Period& p) const { 
        Integer days = 0;
        if (fxConvention)
            days = fxConvention->spotRelative() ? fxConvention->spotDays() : days; 
        return Period(days, Days);
    }

    Period operator()(const FXForwardQuote::FxFwdString& p) const {
        Integer days = 0;
        if (p == FXForwardQuote::FxFwdString::TN)
            days = 1;
        else if (p == FXForwardQuote::FxFwdString::SN && fxConvention) {
            days = fxConvention->spotDays();
        }
        return Period(days, Days);
    }

    QuantLib::ext::shared_ptr<FXConvention> fxConvention;
};

struct FxFwdStringCompare : boost::static_visitor<bool> {

    FxFwdStringCompare(FXForwardQuote::FxFwdString fxFwdString) : fxFwdString(fxFwdString) {}

    bool operator()(const Period& p) const { return false; }

    bool operator()(const FXForwardQuote::FxFwdString& p) const { 
        return p == fxFwdString;
    }

    FXForwardQuote::FxFwdString fxFwdString;
};

} // namespace

QuantLib::Period fxFwdQuoteTenor(const boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString>& term) {
    return boost::apply_visitor(FxTenorGetter(), term);
}

QuantLib::Period fxFwdQuoteStartTenor(const boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString>& term,
                                      const QuantLib::ext::shared_ptr<FXConvention>& fxConvention) {
    return boost::apply_visitor(FxStartTenorGetter(fxConvention), term);
}

bool matchFxFwdStringTerm(const boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString>& term,
                     const FXForwardQuote::FxFwdString& fxfwdString) {
    return boost::apply_visitor(FxFwdStringCompare(fxfwdString), term);
}

//! Function to parse a market datum
QuantLib::ext::shared_ptr<MarketDatum> parseMarketDatum(const Date& asof, const string& datumName, const Real& value) {

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
        return QuantLib::ext::make_shared<ZeroQuote>(value, asof, datumName, quoteType, ccy, date, dc, tenor);
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
        return QuantLib::ext::make_shared<DiscountQuote>(value, asof, datumName, quoteType, ccy, date, tenor);
    }

    case MarketDatum::InstrumentType::MM: {
        QL_REQUIRE(tokens.size() == 5 || tokens.size() == 6, "5 or 6 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        Size offset = 0;
        string indexName;
        if (tokens.size() == 6) {
            indexName = tokens[3];
            offset = 1;
        }
        Period fwdStart = parsePeriod(tokens[3 + offset]);
        Period term = parsePeriod(tokens[4 + offset]);
        return QuantLib::ext::make_shared<MoneyMarketQuote>(value, asof, datumName, quoteType, ccy, fwdStart, term, indexName);
    }

    case MarketDatum::InstrumentType::MM_FUTURE: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        const string& expiry = tokens[3];
        const string& contract = tokens[4];
        Period term = parsePeriod(tokens[5]);
        return QuantLib::ext::make_shared<MMFutureQuote>(value, asof, datumName, quoteType, ccy, expiry, contract, term);
    }

    case MarketDatum::InstrumentType::OI_FUTURE: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        const string& expiry = tokens[3];
        const string& contract = tokens[4];
        Period term = parsePeriod(tokens[5]);
        return QuantLib::ext::make_shared<OIFutureQuote>(value, asof, datumName, quoteType, ccy, expiry, contract, term);
    }

    case MarketDatum::InstrumentType::FRA: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        Period fwdStart = parsePeriod(tokens[3]);
        Period term = parsePeriod(tokens[4]);
        return QuantLib::ext::make_shared<FRAQuote>(value, asof, datumName, quoteType, ccy, fwdStart, term);
    }

    case MarketDatum::InstrumentType::IMM_FRA: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        string imm1 = tokens[3];
        string imm2 = tokens[4];
        unsigned int m1 = parseInteger(imm1);
        unsigned int m2 = parseInteger(imm2);
        QL_REQUIRE(m2 > m1, "Second IMM date must be after the first in " << datumName);
        return QuantLib::ext::make_shared<ImmFraQuote>(value, asof, datumName, quoteType, ccy, m1, m2);
    }

    case MarketDatum::InstrumentType::IR_SWAP: {
        QL_REQUIRE(tokens.size() == 6 || tokens.size() == 7, "6 or 7 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        Size offset = 0;
        string indexName;
        if (tokens.size() == 7) {
            indexName = tokens[3];
            offset = 1;
        }
        Period tenor = parsePeriod(tokens[4 + offset]);
        boost::variant<QuantLib::Date, QuantLib::Period> start = parseDateOrPeriod(tokens[3 + offset]);
        boost::variant<QuantLib::Date, QuantLib::Period> end = parseDateOrPeriod(tokens[5 + offset]);

        if (start.type() == typeid(QuantLib::Period) && end.type() == typeid(QuantLib::Period)) {
            Period fwdStart = QuantLib::ext::get<QuantLib::Period>(start);
            Period term = QuantLib::ext::get<QuantLib::Period>(end);
            return QuantLib::ext::make_shared<SwapQuote>(value, asof, datumName, quoteType, ccy, fwdStart, term, tenor,
                                                 indexName);
        } else if (start.type() == typeid(QuantLib::Date) && end.type() == typeid(QuantLib::Date)) {
            Date startDate = QuantLib::ext::get<QuantLib::Date>(start);
            Date maturityDate = QuantLib::ext::get<QuantLib::Date>(end);
            return QuantLib::ext::make_shared<SwapQuote>(value, asof, datumName, quoteType, ccy, startDate, maturityDate, tenor,
                                                 indexName);
        } else {
            QL_FAIL("Expect swap quote with start/end as either periods or dates");
        }
    }

    case MarketDatum::InstrumentType::BASIS_SWAP: {
        // An optional identifier as a penultimate token supports the following two versions:
        // BASIS_SWAP/BASIS_SPREAD/3M/1D/USD/5Y
        // BASIS_SWAP/BASIS_SPREAD/3M/1D/USD/foobar/5Y
        QL_REQUIRE(tokens.size() == 6 || tokens.size() == 7, "Either 6 or 7 tokens expected in " << datumName);
        Period flatTerm = parsePeriod(tokens[2]);
        Period term = parsePeriod(tokens[3]);
        const string& ccy = tokens[4];
        Period maturity;
        if (tokens.size() == 7) {
            maturity = parsePeriod(tokens[6]);
        } else {
            maturity = parsePeriod(tokens[5]);
        }
        return QuantLib::ext::make_shared<BasisSwapQuote>(value, asof, datumName, quoteType, flatTerm, term, ccy, maturity);
    }

    case MarketDatum::InstrumentType::BMA_SWAP: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        Period term = parsePeriod(tokens[3]);
        Period maturity = parsePeriod(tokens[4]);
        return QuantLib::ext::make_shared<BMASwapQuote>(value, asof, datumName, quoteType, term, ccy, maturity);
    }

    case MarketDatum::InstrumentType::CC_BASIS_SWAP: {
        QL_REQUIRE(tokens.size() == 7, "7 tokens expected in " << datumName);
        const string& flatCcy = tokens[2];
        Period flatTerm = parsePeriod(tokens[3]);
        const string& ccy = tokens[4];
        Period term = parsePeriod(tokens[5]);
        Period maturity = parsePeriod(tokens[6]);
        return QuantLib::ext::make_shared<CrossCcyBasisSwapQuote>(value, asof, datumName, quoteType, flatCcy, flatTerm, ccy,
                                                          term, maturity);
    }

    case MarketDatum::InstrumentType::CC_FIX_FLOAT_SWAP: {
        // CC_FIX_FLOAT_SWAP/RATE/USD/3M/TRY/1Y/5Y
        QL_REQUIRE(tokens.size() == 7, "7 tokens expected in " << datumName);
        Period floatTenor = parsePeriod(tokens[3]);
        Period fixedTenor = parsePeriod(tokens[5]);
        Period maturity = parsePeriod(tokens[6]);
        return QuantLib::ext::make_shared<CrossCcyFixFloatSwapQuote>(value, asof, datumName, quoteType, tokens[2], floatTenor,
                                                             tokens[4], fixedTenor, maturity);
    }

    case MarketDatum::InstrumentType::CDS: {
        // CDS/[CONV_]CREDIT_SPREAD/Name/Seniority/ccy/term
        // CDS/[CONV_]CREDIT_SPREAD/Name/Seniority/ccy/term/runningSpread
        // CDS/[CONV_]CREDIT_SPREAD/Name/Seniority/ccy/doc/term
        // CDS/[CONV_]CREDIT_SPREAD/Name/Seniority/ccy/doc/term/runningSpread
        // CDS/PRICE/Name/Seniority/ccy/term
        // CDS/PRICE/Name/Seniority/ccy/term/runningSpread
        // CDS/PRICE/Name/Seniority/ccy/doc/term
        // CDS/PRICE/Name/Seniority/ccy/doc/term/runningSpread
        QL_REQUIRE(tokens.size() == 6 || tokens.size() == 7 || tokens.size() == 8,
            "6, 7 or 8 tokens expected in " << datumName);
        const string& underlyingName = tokens[2];
        const string& seniority = tokens[3];
        const string& ccy = tokens[4];

        string docClause;
        Period term;
        Real runningSpread = Null<Real>();
        if (tokens.size() == 6) {
            term = parsePeriod(tokens[5]);
        } else if (tokens.size() == 8) {
            docClause = tokens[5];
            term = parsePeriod(tokens[6]);
            runningSpread = parseReal(tokens[7]) / 10000;
        } else {
            // 7 tokens => [5]/[6] = doc/term or term/runningSpread
            CdsDocClause cdsDocClause;
            if (tryParse<CdsDocClause>(tokens[5], cdsDocClause, &parseCdsDocClause)) {
                docClause = tokens[5];
                term = parsePeriod(tokens[6]);
            } else {
                term = parsePeriod(tokens[5]);
                runningSpread = parseReal(tokens[6]) / 10000;
            }
        }

        return QuantLib::ext::make_shared<CdsQuote>(value, asof, datumName, quoteType, underlyingName,
            seniority, ccy, term, docClause, runningSpread);
    }

    case MarketDatum::InstrumentType::HAZARD_RATE: {
        QL_REQUIRE(tokens.size() == 6 || tokens.size() == 7, "6 or 7 tokens expected in " << datumName);
        const string& underlyingName = tokens[2];
        const string& seniority = tokens[3];
        const string& ccy = tokens[4];
        string docClause = tokens.size() == 7 ? tokens[5] : "";
        Period term = parsePeriod(tokens.back());
        return QuantLib::ext::make_shared<HazardRateQuote>(value, asof, datumName, underlyingName, seniority, ccy, term,
                                                   docClause);
    }

    case MarketDatum::InstrumentType::RECOVERY_RATE: {
        QL_REQUIRE(tokens.size() == 3 || tokens.size() == 5 || tokens.size() == 6,
                   "3, 5 or 6 tokens expected in " << datumName);
        const string& underlyingName = tokens[2]; // issuer name for CDS, security ID for bond specific RRs
        string seniority = "";
        string ccy = "";
        string docClause = "";
        if (tokens.size() >= 5) {
            // CDS
            seniority = tokens[3];
            ccy = tokens[4];
            if (tokens.size() == 6)
                docClause = tokens[5];
        }
        return QuantLib::ext::make_shared<RecoveryRateQuote>(value, asof, datumName, underlyingName, seniority, ccy, docClause);
    }

    case MarketDatum::InstrumentType::CAPFLOOR: {
        QL_REQUIRE((tokens.size() >= 8 && tokens.size() <= 10) || tokens.size() == 4 || tokens.size() == 5,
                   "Either 4, 5 or 8, 9, 10 tokens expected in " << datumName);
        const string& ccy = tokens[2];
	    Size offset = 0;
	    std::string indexName;
        Size hasCapFloorToken = (tokens.back() == "C" || tokens.back() == "F") ? 1 : 0;
        QL_REQUIRE(quoteType != MarketDatum::QuoteType::PRICE || hasCapFloorToken == 1,
            "CAPFLOOR PRICE quotes must specify whether the datum represents a cap or a floor with a \"C\" or"
            " \"F\" as the final token.");

        if (tokens.size() == 9 + hasCapFloorToken || tokens.size() == 5 + hasCapFloorToken) {
            // has an index name token... all later tokens are offset by 1
            offset = 1;
            indexName = tokens[3];
        }
        if (tokens.size() == 8 + hasCapFloorToken || tokens.size() == 9 + hasCapFloorToken) {
            Period term = parsePeriod(tokens[3 + offset]);
            Period tenor = parsePeriod(tokens[4 + offset]);
            bool atm = parseBool(tokens[5 + offset].c_str());
            bool relative = parseBool(tokens[6 + offset].c_str());
            Real strike = parseReal(tokens[7 + offset]);
            bool isCap = !(hasCapFloorToken==1 && tokens.back() == "F"); // assume cap if omitted
            return QuantLib::ext::make_shared<CapFloorQuote>(value, asof, datumName, quoteType, ccy, term, tenor, atm, relative,
                                                        strike, indexName, isCap);
        } else {
            // not enough tokens, must be a shift quote
            Period indexTenor = parsePeriod(tokens[3 + offset]);
            return QuantLib::ext::make_shared<CapFloorShiftQuote>(value, asof, datumName, quoteType, ccy, indexTenor,
                                                            indexName);
        }
    }

    case MarketDatum::InstrumentType::SWAPTION: {
        QL_REQUIRE(tokens.size() >= 4 && tokens.size() <= 9, "4...9 tokens expected in " << datumName);
        const string& ccy = tokens[2];
        Size offset = isOnePeriod(tokens[3]) ? 0 : 1;
        std::string quoteTag;
        Size hasPayReceiveToken = (tokens.back() == "P" || tokens.back() == "R") ? 1 : 0;
        QL_REQUIRE(quoteType != MarketDatum::QuoteType::PRICE || hasPayReceiveToken == 1,
            "SWAPTION PRICE quotes must specify whether the datum represents a payer or a receiver"
            " swaption with a \"P\" or \"R\" as the final token.");

        if (offset == 1)
            quoteTag = tokens[3];
        if (tokens.size() >= 6 + offset + hasPayReceiveToken) { // volatility
            Period expiry = parsePeriod(tokens[3 + offset]);
            Period term = parsePeriod(tokens[4 + offset]);
            const string& dimension = tokens[5 + offset];
            Real strike = 0.0;
            if (dimension == "ATM")
                QL_REQUIRE(tokens.size() == 6 + offset + hasPayReceiveToken, 6 + offset + hasPayReceiveToken
                    << " tokens expected in ATM quote " << datumName);
            else if (dimension == "Smile") {
                QL_REQUIRE(tokens.size() == 7 + offset + hasPayReceiveToken, 7 + offset + hasPayReceiveToken
                    << " tokens expected in Smile quote " << datumName);
                strike = parseReal(tokens[6 + offset]);
            } else
                QL_FAIL("Swaption vol quote dimension " << dimension << " not recognised");
            bool isPayer = !(hasPayReceiveToken == 1 && tokens.back() == "R"); // assume payer if omitted
            return QuantLib::ext::make_shared<SwaptionQuote>(value, asof, datumName, quoteType, ccy, expiry, term, dimension,
                                                     strike, quoteTag, isPayer);
        } else { // SLN volatility shift
            return QuantLib::ext::make_shared<SwaptionShiftQuote>(value, asof, datumName, quoteType, ccy,
                                                          parsePeriod(tokens[3 + offset]), quoteTag);
        }
    }

    case MarketDatum::InstrumentType::BOND_OPTION: {
        QL_REQUIRE(tokens.size() == 4 || tokens.size() == 6, "4 or 6 tokens expected in " << datumName);
        const string& qualifier = tokens[2];
        Period expiry = tokens.size() == 6 ? parsePeriod(tokens[3]) : Period(0 * QuantLib::Days);
        Period term = tokens.size() == 6 ? parsePeriod(tokens[4]) : parsePeriod(tokens[3]);
        if (tokens.size() == 6) { // volatility
            QL_REQUIRE(tokens[5] == "ATM", "only ATM allowed for bond option quotes");
            return QuantLib::ext::make_shared<BondOptionQuote>(value, asof, datumName, quoteType, qualifier, expiry, term);
        } else { // SLN volatility shift
            return QuantLib::ext::make_shared<BondOptionShiftQuote>(value, asof, datumName, quoteType, qualifier, term);
        }
    }

    case MarketDatum::InstrumentType::FX_SPOT: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        const string& unitCcy = tokens[2];
        const string& ccy = tokens[3];
        return QuantLib::ext::make_shared<FXSpotQuote>(value, asof, datumName, quoteType, unitCcy, ccy);
    }

    case MarketDatum::InstrumentType::FX_FWD: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& unitCcy = tokens[2];
        const string& ccy = tokens[3];
        boost::variant<QuantLib::Period, FXForwardQuote::FxFwdString> term = parseFxPeriod(tokens[4]);
        return QuantLib::ext::make_shared<FXForwardQuote>(value, asof, datumName, quoteType, unitCcy, ccy, term);
    }

    case MarketDatum::InstrumentType::FX_OPTION: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& unitCcy = tokens[2];
        const string& ccy = tokens[3];
        Period expiry = parsePeriod(tokens[4]);
        const string& strike = tokens[5];
        return QuantLib::ext::make_shared<FXOptionQuote>(value, asof, datumName, quoteType, unitCcy, ccy, expiry, strike);
    }

    case MarketDatum::InstrumentType::ZC_INFLATIONSWAP: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        const string& index = tokens[2];
        Period term = parsePeriod(tokens[3]);
        return QuantLib::ext::make_shared<ZcInflationSwapQuote>(value, asof, datumName, index, term);
    }

    case MarketDatum::InstrumentType::YY_INFLATIONSWAP: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        const string& index = tokens[2];
        Period term = parsePeriod(tokens[3]);
        return QuantLib::ext::make_shared<YoYInflationSwapQuote>(value, asof, datumName, index, term);
    }

    case MarketDatum::InstrumentType::ZC_INFLATIONCAPFLOOR: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& index = tokens[2];
        Period term = parsePeriod(tokens[3]);
        QL_REQUIRE(tokens[4] == "C" || tokens[4] == "F",
                   "expected C or F for Cap or Floor at position 5 in " << datumName);
        bool isCap = tokens[4] == "C";
        string strike = tokens[5];
        return QuantLib::ext::make_shared<ZcInflationCapFloorQuote>(value, asof, datumName, quoteType, index, term, isCap,
                                                            strike);
    }

    case MarketDatum::InstrumentType::YY_INFLATIONCAPFLOOR: {
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        const string& index = tokens[2];
        Period term = parsePeriod(tokens[3]);
        QL_REQUIRE(tokens[4] == "C" || tokens[4] == "F",
                   "expected C or F for Cap or Floor at position 5 in " << datumName);
        bool isCap = tokens[4] == "C";
        string strike = tokens[5];
        return QuantLib::ext::make_shared<YyInflationCapFloorQuote>(value, asof, datumName, quoteType, index, term, isCap,
                                                            strike);
    }

    case MarketDatum::InstrumentType::SEASONALITY: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& index = tokens[3];
        const string& type = tokens[2];
        const string& month = tokens[4];
        return QuantLib::ext::make_shared<SeasonalityQuote>(value, asof, datumName, index, type, month);
    }
    case MarketDatum::InstrumentType::EQUITY_SPOT: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::PRICE, "Invalid quote type for " << datumName);
        const string& equityName = tokens[2];
        const string& ccy = tokens[3];
        return QuantLib::ext::make_shared<EquitySpotQuote>(value, asof, datumName, quoteType, equityName, ccy);
    }

    case MarketDatum::InstrumentType::EQUITY_FWD: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::PRICE, "Invalid quote type for " << datumName);
        const string& equityName = tokens[2];
        const string& ccy = tokens[3];
        Date expiryDate = getDateFromDateOrPeriod(tokens[4], asof);
        return QuantLib::ext::make_shared<EquityForwardQuote>(value, asof, datumName, quoteType, equityName, ccy, expiryDate);
    }

    case MarketDatum::InstrumentType::EQUITY_DIVIDEND: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE, "Invalid quote type for " << datumName);
        const string& equityName = tokens[2];
        const string& ccy = tokens[3];
        Date tenorDate = getDateFromDateOrPeriod(tokens[4], asof);
        return QuantLib::ext::make_shared<EquityDividendYieldQuote>(value, asof, datumName, quoteType, equityName, ccy,
                                                            tenorDate);
    }

    case MarketDatum::InstrumentType::EQUITY_OPTION: {
        QL_REQUIRE(tokens.size() >= 6 || tokens.size() <= 9, "6 - 9 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE_LNVOL || quoteType == MarketDatum::QuoteType::PRICE,
                   "Invalid quote type for " << datumName);
        const string& equityName = tokens[2];
        const string& ccy = tokens[3];
        string expiryString = tokens[4];
        // do we have a call or put flag as the last token?
        bool hasCallPutToken = tokens.back() == "C" || tokens.back() == "P";
        // the following tokens represent the strike, except the last one, if we have a call/put token
        string strikeStr;
        for(Size i=5; i < tokens.size() - (hasCallPutToken ? 1 : 0); ++i) {
            strikeStr += (i > 5 ? "/" : "") + tokens[i];
        }
        QuantLib::ext::shared_ptr<BaseStrike> strike;
        // we support ATM, ATMF as aliases for ATM/AtmSpot, ATM/AtmFwd, plus absolute strikes and MNY/[Spot/Fwd]/1.2
        if(strikeStr == "ATM")
            strike = QuantLib::ext::make_shared<AtmStrike>(QuantLib::DeltaVolQuote::AtmType::AtmSpot);
        else if(strikeStr == "ATMF")
            strike = QuantLib::ext::make_shared<AtmStrike>(QuantLib::DeltaVolQuote::AtmType::AtmFwd);
        else
            strike = parseBaseStrike(strikeStr);
        bool isCall = true;
        if (hasCallPutToken) {
            QL_REQUIRE(tokens.back() == "C" || tokens.back() == "P",
                       "expected C or P for Call or Put at position " << tokens.size() << " in " << datumName);
            isCall = tokens.back() == "C";
        }

        // note how we only store the expiry string - to ensure we can support both Periods and Dates being specified in
        // the vol curve-config.
        return QuantLib::ext::make_shared<EquityOptionQuote>(value, asof, datumName, quoteType, equityName, ccy, expiryString,
                                                     strike, isCall);
    }

    case MarketDatum::InstrumentType::BOND: {
        QL_REQUIRE(tokens.size() == 3, "3 tokens expected in " << datumName);
        const string& securityID = tokens[2];
        if (quoteType == MarketDatum::QuoteType::YIELD_SPREAD)
            return QuantLib::ext::make_shared<SecuritySpreadQuote>(value, asof, datumName, securityID);
        else if (quoteType == MarketDatum::QuoteType::PRICE)
            return QuantLib::ext::make_shared<BondPriceQuote>(value, asof, datumName, securityID);
    }

    case MarketDatum::InstrumentType::CDS_INDEX: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::BASE_CORRELATION, "Invalid quote type for " << datumName);
        const string& cdsIndexName = tokens[2];
        Period term = parsePeriod(tokens[3]);
        Real detachmentPoint = parseReal(tokens[4]);
        return QuantLib::ext::make_shared<BaseCorrelationQuote>(value, asof, datumName, quoteType, cdsIndexName, term,
                                                        detachmentPoint);
    }

    case MarketDatum::InstrumentType::INDEX_CDS_OPTION: {

        // Expects the following form. The strike is optional. The index term is optional for backwards compatibility.
        // INDEX_CDS_OPTION/RATE_LNVOL/<INDEX_NAME>[/<INDEX_TERM>]/<EXPIRY>[/<STRIKE>]
        QL_REQUIRE(tokens.size() >= 4 || tokens.size() <= 6, "4, 5 or 6 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE_LNVOL, "Invalid quote type for " << datumName);

        QuantLib::ext::shared_ptr<Expiry> expiry;
        QuantLib::ext::shared_ptr<BaseStrike> strike;
        string indexTerm;
        if (tokens.size() == 6) {
            // We have been given an index term, an expiry and a strike.
            indexTerm = tokens[3];
            expiry = parseExpiry(tokens[4]);
            strike = parseBaseStrike(tokens[5]);
        } else if (tokens.size() == 5) {
            // We have been given either 1) an index term and an expiry or 2) an expiry and a strike.
            // If the last token is a number, we have 2) an expiry and a strike.
            Real tmp;
            if (tryParseReal(tokens[4], tmp)) {
                expiry = parseExpiry(tokens[3]);
                strike = parseBaseStrike(tokens[4]);
            } else {
                indexTerm = tokens[3];
                expiry = parseExpiry(tokens[4]);
            }
        } else {
            // We have just been given the expiry.
            expiry = parseExpiry(tokens[3]);
        }

        return QuantLib::ext::make_shared<IndexCDSOptionQuote>(value, asof, datumName, tokens[2], expiry, indexTerm, strike);
    }

    case MarketDatum::InstrumentType::COMMODITY_SPOT: {
        QL_REQUIRE(tokens.size() == 4, "4 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::PRICE, "Invalid quote type for " << datumName);

        return QuantLib::ext::make_shared<CommoditySpotQuote>(value, asof, datumName, quoteType, tokens[2], tokens[3]);
    }

    case MarketDatum::InstrumentType::COMMODITY_FWD: {
        // Expects the following form:
        // COMMODITY_FWD/PRICE/<COMDTY_NAME>/<CCY>/<DATE/TENOR>
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::PRICE, "Invalid quote type for " << datumName);

        // The last token can be a string defining a special tenor i.e. ON, TN or SN
        if (tokens[4] == "ON") {
            return QuantLib::ext::make_shared<CommodityForwardQuote>(value, asof, datumName, quoteType, tokens[2], tokens[3],
                                                             1 * Days, 0 * Days);
        } else if (tokens[4] == "TN") {
            return QuantLib::ext::make_shared<CommodityForwardQuote>(value, asof, datumName, quoteType, tokens[2], tokens[3],
                                                             1 * Days, 1 * Days);
        } else if (tokens[4] == "SN") {
            return QuantLib::ext::make_shared<CommodityForwardQuote>(value, asof, datumName, quoteType, tokens[2], tokens[3],
                                                             1 * Days);
        }

        // The last token can be a date or a standard tenor
        Date date;
        Period tenor;
        bool isDate;
        parseDateOrPeriod(tokens[4], date, tenor, isDate);

        if (isDate) {
            return QuantLib::ext::make_shared<CommodityForwardQuote>(value, asof, datumName, quoteType, tokens[2], tokens[3],
                                                             date);
        } else {
            return QuantLib::ext::make_shared<CommodityForwardQuote>(value, asof, datumName, quoteType, tokens[2], tokens[3],
                                                             tenor);
        }
    }

    case MarketDatum::InstrumentType::COMMODITY_OPTION: {
        // Expects one of the following forms:
        // COMMODITY_OPTION/<QT>/<COMDTY_NAME>/<CCY>/<EXPIRY>/<STRIKE>
        // COMMODITY_OPTION/<QT>/<COMDTY_NAME>/<CCY>/<EXPIRY>/<STRIKE>/<OT>
        // where QT = RATE_LNVOL or PRICE and OT = C (for Call) or P (for Put)
        using QT = MarketDatum::QuoteType;
        QL_REQUIRE(tokens.size() >= 6, "At least 6 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == QT::RATE_LNVOL || quoteType == QT::PRICE,
                   "Quote type for " << datumName << " should be 'RATE_LNVOL' or 'PRICE'");

        QuantLib::ext::shared_ptr<Expiry> expiry = parseExpiry(tokens[4]);

        // If the last token is C or P, process it and update bounds of strike portion.
        auto itStkBeg = tokens.begin() + 5;
        auto itStkEnd = tokens.end();
        Option::Type optionType = Option::Call;
        if (tokens.back() == "C" || tokens.back() == "P") {
            if (tokens.back() == "P")
                optionType = Option::Put;
            itStkEnd = prev(itStkEnd);
        }

        // Parse the strike
        QL_REQUIRE(itStkBeg != itStkEnd, "");
        string strStrike = boost::algorithm::join(boost::make_iterator_range(itStkBeg, itStkEnd), "/");
        QuantLib::ext::shared_ptr<BaseStrike> strike = parseBaseStrike(strStrike);

        return QuantLib::ext::make_shared<CommodityOptionQuote>(value, asof, datumName, quoteType, tokens[2],
            tokens[3], expiry, strike, optionType);
    }

    case MarketDatum::InstrumentType::CORRELATION: {
        // Expects the following form:
        // CORRELATION/RATE/<INDEX1>/<INDEX2>/<TENOR>/<STRIKE>
        QL_REQUIRE(tokens.size() == 6, "6 tokens expected in " << datumName);
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE || quoteType == MarketDatum::QuoteType::PRICE,
                   "Quote type for " << datumName << " should be 'CORRELATION' or 'PRICE'");

        return QuantLib::ext::make_shared<CorrelationQuote>(value, asof, datumName, quoteType, tokens[2], tokens[3], tokens[4],
                                                    tokens[5]);
    }

    case MarketDatum::InstrumentType::CPR: {
        QL_REQUIRE(tokens.size() == 3, "3 tokens expected in " << datumName);
        const string& securityID = tokens[2];
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::RATE, "Invalid quote type for " << datumName);
        return QuantLib::ext::make_shared<CPRQuote>(value, asof, datumName, securityID);
    }

    case MarketDatum::InstrumentType::RATING: {
        QL_REQUIRE(tokens.size() == 5, "5 tokens expected in " << datumName);
        const string& name = tokens[2];
        const string& fromRating = tokens[3];
        const string& toRating = tokens[4];
        QL_REQUIRE(quoteType == MarketDatum::QuoteType::TRANSITION_PROBABILITY, "Invalid quote type for " << datumName);
        return QuantLib::ext::make_shared<TransitionProbabilityQuote>(value, asof, datumName, name, fromRating, toRating);
    }

    default:
        QL_FAIL("Cannot convert \"" << datumName << "\" to MarketDatum");
    } // switch instrument type
} // parseMarketDatum
} // namespace data
} // namespace ore
