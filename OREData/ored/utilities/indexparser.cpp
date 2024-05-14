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

/*! \file ored/utilities/indexparser.cpp
    \brief
    \ingroup utilities
*/

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <boost/regex.hpp>
#include <map>
#include <ored/configuration/conventions.hpp>
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/indexnametranslator.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>
#include <ql/errors.hpp>
#include <ql/indexes/all.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/all.hpp>
#include <qle/indexes/behicp.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/indexes/cacpi.hpp>
#include <qle/indexes/commoditybasisfutureindex.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/indexes/decpi.hpp>
#include <qle/indexes/dkcpi.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/escpi.hpp>
#include <qle/indexes/frcpi.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/indexes/genericiborindex.hpp>
#include <qle/indexes/genericindex.hpp>
#include <qle/indexes/ibor/ambor.hpp>
#include <qle/indexes/ibor/ameribor.hpp>
#include <qle/indexes/ibor/boebaserate.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>
#include <qle/indexes/ibor/chfsaron.hpp>
#include <qle/indexes/ibor/chftois.hpp>
#include <qle/indexes/ibor/clpcamara.hpp>
#include <qle/indexes/ibor/cnhhibor.hpp>
#include <qle/indexes/ibor/cnhshibor.hpp>
#include <qle/indexes/ibor/cnyrepofix.hpp>
#include <qle/indexes/ibor/copibr.hpp>
#include <qle/indexes/ibor/corra.hpp>
#include <qle/indexes/ibor/czkpribor.hpp>
#include <qle/indexes/ibor/demlibor.hpp>
#include <qle/indexes/ibor/dkkcibor.hpp>
#include <qle/indexes/ibor/dkkcita.hpp>
#include <qle/indexes/ibor/dkkois.hpp>
#include <qle/indexes/ibor/hkdhibor.hpp>
#include <qle/indexes/ibor/hkdhonia.hpp>
#include <qle/indexes/ibor/hufbubor.hpp>
#include <qle/indexes/ibor/idridrfix.hpp>
#include <qle/indexes/ibor/idrjibor.hpp>
#include <qle/indexes/ibor/ilstelbor.hpp>
#include <qle/indexes/ibor/inrmiborois.hpp>
#include <qle/indexes/ibor/inrmifor.hpp>
#include <qle/indexes/ibor/jpyeytibor.hpp>
#include <qle/indexes/ibor/krwcd.hpp>
#include <qle/indexes/ibor/krwkoribor.hpp>
#include <qle/indexes/ibor/mxntiie.hpp>
#include <qle/indexes/ibor/myrklibor.hpp>
#include <qle/indexes/ibor/noknibor.hpp>
#include <qle/indexes/ibor/nowa.hpp>
#include <qle/indexes/ibor/nzdbkbm.hpp>
#include <qle/indexes/ibor/phpphiref.hpp>
#include <qle/indexes/ibor/plnpolonia.hpp>
#include <qle/indexes/ibor/primeindex.hpp>
#include <qle/indexes/ibor/rubkeyrate.hpp>
#include <qle/indexes/ibor/saibor.hpp>
#include <qle/indexes/ibor/seksior.hpp>
#include <qle/indexes/ibor/sekstibor.hpp>
#include <qle/indexes/ibor/sekstina.hpp>
#include <qle/indexes/ibor/sgdsibor.hpp>
#include <qle/indexes/ibor/sgdsor.hpp>
#include <qle/indexes/ibor/skkbribor.hpp>
#include <qle/indexes/ibor/sofr.hpp>
#include <qle/indexes/ibor/sonia.hpp>
#include <qle/indexes/ibor/sora.hpp>
#include <qle/indexes/ibor/thbbibor.hpp>
#include <qle/indexes/ibor/thor.hpp>
#include <qle/indexes/ibor/tonar.hpp>
#include <qle/indexes/ibor/twdtaibor.hpp>
#include <qle/indexes/offpeakpowerindex.hpp>
#include <qle/indexes/secpi.hpp>
#include <qle/termstructures/commoditybasispricecurve.hpp>
#include <qle/termstructures/spreadedpricetermstructure.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using ore::data::Convention;

namespace ore {
namespace data {

// Helper base class to build an IborIndex with a specific period and term structure given an instance of the same
// IborIndex
class IborIndexParser {
public:
    virtual ~IborIndexParser() {}
    virtual QuantLib::ext::shared_ptr<IborIndex> build(Period p, const Handle<YieldTermStructure>& h) const = 0;
    virtual string family() const = 0;
};

// General case
template <class T> class IborIndexParserWithPeriod : public IborIndexParser {
public:
    QuantLib::ext::shared_ptr<IborIndex> build(Period p, const Handle<YieldTermStructure>& h) const override {
        return QuantLib::ext::make_shared<T>(p, h);
    }
    string family() const override { return T(3 * Months).familyName(); }
};

// MXN TIIE
// If tenor equates to 28 Days, i.e. tenor is 4W or 28D, ensure that the index is created
// with a tenor of 4W under the hood. Things work better this way especially cap floor stripping.
// We do the same with 91D -> 3M (a la KRW CD below)
template <> class IborIndexParserWithPeriod<MXNTiie> : public IborIndexParser {
public:
    QuantLib::ext::shared_ptr<IborIndex> build(Period p, const Handle<YieldTermStructure>& h) const override {
        if (p.units() == Days && p.length() == 28) {
            return QuantLib::ext::make_shared<MXNTiie>(4 * Weeks, h);
        } else if (p.units() == Days && p.length() == 91) {
            return QuantLib::ext::make_shared<MXNTiie>(3 * Months, h);
        } else if (p.units() == Days && (p.length() >= 180 || p.length() <= 183)) {
            return QuantLib::ext::make_shared<MXNTiie>(6 * Months, h);
        } else {
            return QuantLib::ext::make_shared<MXNTiie>(p, h);
        }
    }

    string family() const override { return MXNTiie(4 * Weeks).familyName(); }
};

// KRW CD
// If tenor equates to 91 Days, ensure that the index is created with a tenor of 3M under the hood.
template <> class IborIndexParserWithPeriod<KRWCd> : public IborIndexParser {
public:
    QuantLib::ext::shared_ptr<IborIndex> build(Period p, const Handle<YieldTermStructure>& h) const override {
        if (p.units() == Days && p.length() == 91) {
            return QuantLib::ext::make_shared<KRWCd>(3 * Months, h);
        } else {
            return QuantLib::ext::make_shared<KRWCd>(p, h);
        }
    }

    string family() const override { return KRWCd(3 * Months).familyName(); }
};

// CNY REPOFIX
// If tenor equates to 7 Days, i.e. tenor is 1W or 7D, ensure that the index is created
// with a tenor of 1W under the hood. Similarly for 14 days, i.e. 2W.
template <> class IborIndexParserWithPeriod<CNYRepoFix> : public IborIndexParser {
public:
    QuantLib::ext::shared_ptr<IborIndex> build(Period p, const Handle<YieldTermStructure>& h) const override {
        if (p.units() == Days && p.length() == 7) {
            return QuantLib::ext::make_shared<CNYRepoFix>(1 * Weeks, h);
        } else if (p.units() == Days && p.length() == 14) {
            return QuantLib::ext::make_shared<CNYRepoFix>(2 * Weeks, h);
        } else {
            return QuantLib::ext::make_shared<CNYRepoFix>(p, h);
        }
    }

    string family() const override { return CNYRepoFix(1 * Weeks).familyName(); }
};

// Helper function to check that index name to index object is a one-to-one mapping
void checkOneToOne(const map<string, QuantLib::ext::shared_ptr<OvernightIndex>>& onIndices,
                   const map<string, QuantLib::ext::shared_ptr<IborIndexParser>>& iborIndices) {

    // Should not attempt to add the same family name to the set if the provided mappings are one to one
    set<string> familyNames;

    for (const auto& kv : onIndices) {
        auto p = familyNames.insert(kv.second->familyName());
        QL_REQUIRE(p.second, "Duplicate mapping for overnight index family " << *p.first << " not allowed");
    }

    for (const auto& kv : iborIndices) {
        auto p = familyNames.insert(kv.second->family());
        QL_REQUIRE(p.second, "Duplicate mapping for ibor index family " << *p.first << " not allowed");
    }
}

QuantLib::ext::shared_ptr<FxIndex> parseFxIndex(const string& s, const Handle<Quote>& fxSpot,
                                        const Handle<YieldTermStructure>& sourceYts,
                                        const Handle<YieldTermStructure>& targetYts, const bool useConventions) {
    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 4, "four tokens required in " << s << ": FX-TAG-CCY1-CCY2");
    QL_REQUIRE(tokens[0] == "FX", "expected first token to be FX");
    Natural fixingDays = 0;
    Calendar fixingCalendar = NullCalendar();
    BusinessDayConvention bdc;
    if (useConventions)
        std::tie(fixingDays, fixingCalendar, bdc) = getFxIndexConventions(s);
    auto index = QuantLib::ext::make_shared<FxIndex>(tokens[1], fixingDays, parseCurrency(tokens[2]), parseCurrency(tokens[3]),
                                             fixingCalendar, fxSpot, sourceYts, targetYts);

    IndexNameTranslator::instance().add(index->name(), s);
    return index;
}

QuantLib::ext::shared_ptr<QuantExt::EquityIndex2> parseEquityIndex(const string& s) {
    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2, "two tokens required in " << s << ": EQ-NAME");
    QL_REQUIRE(tokens[0] == "EQ", "expected first token to be EQ");
    auto index = QuantLib::ext::make_shared<QuantExt::EquityIndex2>(tokens[1], NullCalendar(), Currency());
    IndexNameTranslator::instance().add(index->name(), s);
    return index;
}

QuantLib::ext::shared_ptr<QuantLib::Index> parseGenericIndex(const string& s) {
    QL_REQUIRE(boost::starts_with(s, "GENERIC-"), "generic index expected to be of the form GENERIC-*");
    auto index = QuantLib::ext::make_shared<GenericIndex>(s);
    IndexNameTranslator::instance().add(index->name(), s);
    return index;
}

bool tryParseIborIndex(const string& s, QuantLib::ext::shared_ptr<IborIndex>& index) {
    try {
        index = parseIborIndex(s, Handle<YieldTermStructure>());
    } catch (const std::exception& e) {
	DLOG("tryParseIborIndex(" << s << ") failed: " << e.what());
        return false;
    }
    return true;
}

QuantLib::ext::shared_ptr<IborIndex> parseIborIndex(const string& s, const Handle<YieldTermStructure>& h) {
    string dummy;
    return parseIborIndex(s, dummy, h);
}

QuantLib::ext::shared_ptr<IborIndex> parseIborIndex(const string& s, string& tenor, const Handle<YieldTermStructure>& h) {

    // Check the index string is of the required form before doing anything
    vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2 || tokens.size() == 3,
               "Two or three tokens required in " << s << ": CCY-INDEX or CCY-INDEX-TERM");

    // Variables used below
    string indexStem = tokens[0] + "-" + tokens[1];
    if (tokens.size() == 3) {
        tenor = tokens[2];
    } else {
        tenor = "";
    }

    const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> c;
    if (conventions->has(s, Convention::Type::IborIndex) || conventions->has(s, Convention::Type::OvernightIndex))
        c = conventions->get(s); 

    // if we have a convention given, set up the index using this convention, this overrides the parsing from
    // hardcoded strings below if there is an overlap
    if (c) {
        QL_REQUIRE(c->id() == s, "ibor index convention id ('"
                                     << c->id() << "') not matching ibor index string to parse ('" << s << "'");
        Currency ccy = parseCurrency(tokens[0]);
        if (auto conv = QuantLib::ext::dynamic_pointer_cast<OvernightIndexConvention>(c)) {
            QL_REQUIRE(tenor.empty(), "no tenor allowed for convention based overnight index ('" << s << "')");
            auto res = QuantLib::ext::make_shared<OvernightIndex>(tokens[0] + "-" + tokens[1], conv->settlementDays(), ccy,
                                                      parseCalendar(conv->fixingCalendar()),
                                                      parseDayCounter(conv->dayCounter()), h);
	    IndexNameTranslator::instance().add(res->name(), s);
	    return res;
        } else if (auto conv = QuantLib::ext::dynamic_pointer_cast<IborIndexConvention>(c)) {
            QL_REQUIRE(!tenor.empty(), "no tenor given for convention based Ibor index ('" << s << "'");
            auto res = QuantLib::ext::make_shared<IborIndex>(tokens[0] + "-" + tokens[1], parsePeriod(tenor),
                                                 conv->settlementDays(), ccy, parseCalendar(conv->fixingCalendar()),
                                                 parseBusinessDayConvention(conv->businessDayConvention()),
                                                 conv->endOfMonth(), parseDayCounter(conv->dayCounter()), h);
	    IndexNameTranslator::instance().add(res->name(), s);
	    return res;
        } else {
            QL_FAIL("invalid convention passed to parseIborIndex(): expected OvernightIndexConvention or "
                    "IborIndexConvention");
        }
    }

    // if we do not have a convention, look up the index in the hardcoded maps below

    // Map from our _unique internal name_ to an overnight index
    static map<string, QuantLib::ext::shared_ptr<OvernightIndex>> onIndices = {
        {"EUR-EONIA", QuantLib::ext::make_shared<Eonia>()},
        {"EUR-ESTER", QuantLib::ext::make_shared<Estr>()},
        {"GBP-SONIA", QuantLib::ext::make_shared<Sonia>()},
        {"JPY-TONAR", QuantLib::ext::make_shared<Tonar>()},
        {"SGD-SORA", QuantLib::ext::make_shared<Sora>()},
        {"CHF-TOIS", QuantLib::ext::make_shared<CHFTois>()},
        {"CHF-SARON", QuantLib::ext::make_shared<CHFSaron>()},
        {"USD-FedFunds", QuantLib::ext::make_shared<FedFunds>()},
        {"USD-SOFR", QuantLib::ext::make_shared<Sofr>()},
        {"USD-Prime", QuantLib::ext::make_shared<PrimeIndex>()},
        {"USD-AMERIBOR", QuantLib::ext::make_shared<USDAmeribor>()},
        {"AUD-AONIA", QuantLib::ext::make_shared<Aonia>()},
        {"CAD-CORRA", QuantLib::ext::make_shared<CORRA>()},
        {"DKK-DKKOIS", QuantLib::ext::make_shared<DKKOis>()},
        {"SEK-SIOR", QuantLib::ext::make_shared<SEKSior>()},
        {"COP-IBR", QuantLib::ext::make_shared<COPIbr>()},
        {"BRL-CDI", QuantLib::ext::make_shared<BRLCdi>()},
        {"NOK-NOWA", QuantLib::ext::make_shared<Nowa>()},
        {"CLP-CAMARA", QuantLib::ext::make_shared<CLPCamara>()},
        {"NZD-OCR", QuantLib::ext::make_shared<Nzocr>()},
        {"PLN-POLONIA", QuantLib::ext::make_shared<PLNPolonia>()},
        {"INR-MIBOROIS", QuantLib::ext::make_shared<INRMiborOis>()},
        {"GBP-BoEBase", QuantLib::ext::make_shared<BOEBaseRateIndex>()},
        {"HKD-HONIA", QuantLib::ext::make_shared<HKDHonia>()},
        {"SEK-STINA", QuantLib::ext::make_shared<SEKStina>()},
        {"DKK-CITA", QuantLib::ext::make_shared<DKKCita>()},
        {"THB-THOR", QuantLib::ext::make_shared<THBThor>()}};

    // Map from our _unique internal name_ to an ibor index (the period does not matter here)
    static map<string, QuantLib::ext::shared_ptr<IborIndexParser>> iborIndices = {
        {"AUD-BBSW", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Bbsw>>()},
        {"AUD-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<AUDLibor>>()},
        {"EUR-EURIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Euribor>>()},
        {"EUR-EURIBOR365", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Euribor365>>()},
        {"CAD-CDOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Cdor>>()},
        {"CNY-SHIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Shibor>>()},
        {"CZK-PRIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<CZKPribor>>()},
        {"EUR-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<EURLibor>>()},
        {"USD-AMBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<USDAmbor>>()},
        {"USD-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<USDLibor>>()},
        {"GBP-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<GBPLibor>>()},
        {"JPY-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<JPYLibor>>()},
        {"JPY-TIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Tibor>>()},
        {"JPY-EYTIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<JPYEYTIBOR>>()},
        {"CAD-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<CADLibor>>()},
        {"CHF-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<CHFLibor>>()},
        {"SEK-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<SEKLibor>>()},
        {"SEK-STIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<SEKStibor>>()},
        {"NOK-NIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<NOKNibor>>()},
        {"HKD-HIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<HKDHibor>>()},
        {"CNH-HIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<CNHHibor>>()},
        {"CNH-SHIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<CNHShibor>>()},
        {"SAR-SAIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<SAibor>>()},
        {"SGD-SIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<SGDSibor>>()},
        {"SGD-SOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<SGDSor>>()},
        {"DKK-CIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<DKKCibor>>()},
        {"DKK-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<DKKLibor>>()},
        {"HUF-BUBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<HUFBubor>>()},
        {"IDR-IDRFIX", QuantLib::ext::make_shared<IborIndexParserWithPeriod<IDRIdrfix>>()},
        {"IDR-JIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<IDRJibor>>()},
        {"ILS-TELBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<ILSTelbor>>()},
        {"INR-MIFOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<INRMifor>>()},
        {"MXN-TIIE", QuantLib::ext::make_shared<IborIndexParserWithPeriod<MXNTiie>>()},
        {"PLN-WIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Wibor>>()},
        {"SKK-BRIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<SKKBribor>>()},
        {"NZD-BKBM", QuantLib::ext::make_shared<IborIndexParserWithPeriod<NZDBKBM>>()},
        {"TRY-TRLIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<TRLibor>>()},
        {"TWD-TAIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<TWDTaibor>>()},
        {"MYR-KLIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<MYRKlibor>>()},
        {"KRW-CD", QuantLib::ext::make_shared<IborIndexParserWithPeriod<KRWCd>>()},
        {"KRW-KORIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<KRWKoribor>>()},
        {"ZAR-JIBAR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Jibar>>()},
        {"RUB-MOSPRIME", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Mosprime>>()},
        {"RUB-KEYRATE", QuantLib::ext::make_shared<IborIndexParserWithPeriod<RUBKeyRate>>()},
        {"THB-BIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<THBBibor>>()},
        {"THB-THBFIX", QuantLib::ext::make_shared<IborIndexParserWithPeriod<THBFIX>>()},
        {"PHP-PHIREF", QuantLib::ext::make_shared<IborIndexParserWithPeriod<PHPPhiref>>()},
        {"RON-ROBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<Robor>>()},
        {"DEM-LIBOR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<DEMLibor>>()},
        {"CNY-REPOFIX", QuantLib::ext::make_shared<IborIndexParserWithPeriod<CNYRepoFix>>()},
        {"USD-SOFR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<QuantExt::SofrTerm>>()},
        {"GBP-SONIA", QuantLib::ext::make_shared<IborIndexParserWithPeriod<QuantExt::SoniaTerm>>()},
        {"JPY-TONAR", QuantLib::ext::make_shared<IborIndexParserWithPeriod<QuantExt::TonarTerm>>()},
        {"CAD-CORRA", QuantLib::ext::make_shared<IborIndexParserWithPeriod<QuantExt::CORRATerm>>()}};

    // Check (once) that we have a one-to-one mapping
    static bool checked = false;
    if (!checked) {
        checkOneToOne(onIndices, iborIndices);
        checked = true;
    }

    // Simple single case for USD-SIFMA (i.e. BMA)
    if (indexStem == "USD-SIFMA") {
        QL_REQUIRE(tenor.empty(), "A tenor is not allowed with USD-SIFMA as it is implied");
        auto res = QuantLib::ext::make_shared<BMAIndexWrapper>(QuantLib::ext::make_shared<BMAIndex>(h));
	IndexNameTranslator::instance().add(res->name(), s);
	return res;
    }

    // Ibor indices with a tenor, this includes OIS term rates like USD-SOFR-3M
    auto it = iborIndices.find(indexStem);
    if (it != iborIndices.end() && !tenor.empty()) {
        Period p = parsePeriod(tenor);
        auto res = it->second->build(p, h);
        IndexNameTranslator::instance().add(res->name(), s);
        return res;
    }

    // Overnight indices
    auto onIt = onIndices.find(indexStem);
    if (onIt != onIndices.end()) {
        QL_REQUIRE(tenor.empty(),
                   "A tenor is not allowed with the overnight index " << indexStem << " as it is implied");
        auto res = onIt->second->clone(h);
	IndexNameTranslator::instance().add(res->name(), s);
	return res;
    }

    // GENERIC indices
    if (tokens[1] == "GENERIC") {
        Period p = parsePeriod(tenor);
        auto ccy = parseCurrency(tokens[0]);
        auto res = QuantLib::ext::make_shared<GenericIborIndex>(p, ccy, h);
        IndexNameTranslator::instance().add(res->name(), s);
        return res;
    }

    QL_FAIL("parseIborIndex \"" << s << "\" not recognized");
}

bool isGenericIborIndex(const string& indexName) { return indexName.find("-GENERIC-") != string::npos; }

pair<bool, QuantLib::ext::shared_ptr<ZeroInflationIndex>> isInflationIndex(const string& indexName) {

    QuantLib::ext::shared_ptr<ZeroInflationIndex> index;
    try {
        // Currently, only way to have an inflation index is to have a ZeroInflationIndex
        index = parseZeroInflationIndex(indexName, Handle<ZeroInflationTermStructure>());
    } catch (...) {
        return make_pair(false, nullptr);
    }
    return make_pair(true, index);
}

bool isEquityIndex(const string& indexName) {
    try {
        parseEquityIndex(indexName);
    } catch (...) {
        return false;
    }
    return true;
}

bool isCommodityIndex(const string& indexName) {
    try {
        parseCommodityIndex(indexName);
    } catch (...) {
        return false;
    }
    return true;
}

bool isGenericIndex(const string& indexName) {
    try {
        parseGenericIndex(indexName);
    }
    catch (...) {
        return false;
    }
    return true;
}

// Swap Index Parser base
class SwapIndexParser {
public:
    virtual ~SwapIndexParser() {}
    virtual QuantLib::ext::shared_ptr<SwapIndex> build(Period p, const Handle<YieldTermStructure>& f,
                                               const Handle<YieldTermStructure>& d) const = 0;
};

// We build with both a forwarding and discounting curve
template <class T> class SwapIndexParserDualCurve : public SwapIndexParser {
public:
    QuantLib::ext::shared_ptr<SwapIndex> build(Period p, const Handle<YieldTermStructure>& f,
                                       const Handle<YieldTermStructure>& d) const override {
        return QuantLib::ext::make_shared<T>(p, f, d);
    }
};

QuantLib::ext::shared_ptr<SwapIndex> parseSwapIndex(const string& s, const Handle<YieldTermStructure>& f,
                                            const Handle<YieldTermStructure>& d) {

    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 3 || tokens.size() == 4,
               "three or four tokens required in " << s << ": CCY-CMS-TENOR or CCY-CMS-TAG-TENOR");
    QL_REQUIRE(tokens[0].size() == 3, "invalid currency code in " << s);
    QL_REQUIRE(tokens[1] == "CMS", "expected CMS as middle token in " << s);
    Period p = parsePeriod(tokens.back());
    // use default family name, if none is given
    string familyName = tokens.size() == 4 ? tokens[0] + "-CMS-" + tokens[2] : tokens[0] + "LiborSwapIsdaFix";
    Currency ccy = parseCurrency(tokens[0]);

    const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<SwapIndexConvention> swapIndexConvention;
    QuantLib::ext::shared_ptr<IRSwapConvention> irSwapConvention;
    QuantLib::ext::shared_ptr<OisConvention> oisCompConvention;
    QuantLib::ext::shared_ptr<AverageOisConvention> oisAvgConvention;
    if (conventions && conventions->has(s, Convention::Type::SwapIndex)) {
        swapIndexConvention = QuantLib::ext::dynamic_pointer_cast<SwapIndexConvention>(conventions->get(s));
        QL_REQUIRE(swapIndexConvention, "internal error: could not cast to SwapIndexConvention");
        QL_REQUIRE(conventions->has(swapIndexConvention->conventions(), Convention::Type::Swap) ||
                       conventions->has(swapIndexConvention->conventions(), Convention::Type::OIS) ||
                       conventions->has(swapIndexConvention->conventions(), Convention::Type::AverageOIS),
                   "do not have swap or ois conventions for '"
                       << swapIndexConvention->conventions() << "', required from swap index convention '" << s << "'");
        irSwapConvention =
            QuantLib::ext::dynamic_pointer_cast<IRSwapConvention>(conventions->get(swapIndexConvention->conventions()));
        oisCompConvention =
            QuantLib::ext::dynamic_pointer_cast<OisConvention>(conventions->get(swapIndexConvention->conventions()));
        oisAvgConvention =
            QuantLib::ext::dynamic_pointer_cast<AverageOisConvention>(conventions->get(swapIndexConvention->conventions()));
        QL_REQUIRE(irSwapConvention || oisCompConvention || oisAvgConvention,
                   "internal error: could not cast to IRSwapConvention, OisConvention, AverageOisConvention");
    } else {
        // set default conventions using a generic ibor index
        irSwapConvention = QuantLib::ext::make_shared<IRSwapConvention>("dummy_swap_conv_" + tokens[0], tokens[0], "Annual",
                                                                "MF", "A365", tokens[0] + "-GENERIC-3M");
        swapIndexConvention = QuantLib::ext::make_shared<SwapIndexConvention>("dummy_swapindex_conv_" + tokens[0],
                                                                      "dummy_swap_conv_" + tokens[0], "");
    }

    QuantLib::ext::shared_ptr<SwapIndex> index;
    if (irSwapConvention) {
        Calendar fixingCalendar = swapIndexConvention->fixingCalendar().empty()
                                      ? irSwapConvention->fixedCalendar()
                                      : parseCalendar(swapIndexConvention->fixingCalendar());
        index = QuantLib::ext::make_shared<SwapIndex>(familyName, p, irSwapConvention->index()->fixingDays(), ccy,
                                              fixingCalendar, Period(irSwapConvention->fixedFrequency()),
                                              irSwapConvention->fixedConvention(), irSwapConvention->fixedDayCounter(),
                                              irSwapConvention->index()->clone(f), d);
    } else if (oisCompConvention) {
        Calendar fixingCalendar = swapIndexConvention->fixingCalendar().empty()
                                      ? oisCompConvention->index()->fixingCalendar()
                                      : parseCalendar(swapIndexConvention->fixingCalendar());
        index = QuantLib::ext::make_shared<OvernightIndexedSwapIndex>(
            familyName, p, oisCompConvention->spotLag(), ccy,
            QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(oisCompConvention->index()->clone(f)), true,
            RateAveraging::Compound, Period(oisCompConvention->fixedFrequency()), d);
    } else if (oisAvgConvention) {
        Calendar fixingCalendar = swapIndexConvention->fixingCalendar().empty()
                                      ? oisAvgConvention->index()->fixingCalendar()
                                      : parseCalendar(swapIndexConvention->fixingCalendar());
        auto index = QuantLib::ext::make_shared<OvernightIndexedSwapIndex>(
            familyName, p, oisAvgConvention->spotLag(), ccy,
            QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(oisAvgConvention->index()->clone(f)), true,
            RateAveraging::Simple, Period(oisAvgConvention->fixedFrequency()), d);
        IndexNameTranslator::instance().add(index->name(), s);
        return index;
    } else {
        QL_FAIL("internal error: expected irSwapConvention, oisConvention, averageOisConvention to be not null");
    }
    IndexNameTranslator::instance().add(index->name(), s);
    return index;
}

// Zero Inflation Index Parser
class ZeroInflationIndexParserBase {
public:
    virtual ~ZeroInflationIndexParserBase() {}
    virtual QuantLib::ext::shared_ptr<ZeroInflationIndex> build(const Handle<ZeroInflationTermStructure>& h) const = 0;
    virtual QL_DEPRECATED QuantLib::ext::shared_ptr<ZeroInflationIndex>
    build(bool isInterpolated, const Handle<ZeroInflationTermStructure>& h) const = 0;
};

template <class T> class ZeroInflationIndexParser : public ZeroInflationIndexParserBase {
public:
    QuantLib::ext::shared_ptr<ZeroInflationIndex> build(const Handle<ZeroInflationTermStructure>& h) const override {
        return QuantLib::ext::make_shared<T>(h);
    }

    QL_DEPRECATED QuantLib::ext::shared_ptr<ZeroInflationIndex> build(bool isInterpolated,
                                                const Handle<ZeroInflationTermStructure>& h) const override {
        return QuantLib::ext::make_shared<T>(isInterpolated, h);
    }
};

template <class T> class ZeroInflationIndexParserWithFrequency : public ZeroInflationIndexParserBase {
public:
    ZeroInflationIndexParserWithFrequency(Frequency frequency) : frequency_(frequency) {}
    
    QuantLib::ext::shared_ptr<ZeroInflationIndex> build(const Handle<ZeroInflationTermStructure>& h) const override {
        return QuantLib::ext::make_shared<T>(frequency_, false, h);
    }
    
    QuantLib::ext::shared_ptr<ZeroInflationIndex> build(bool isInterpolated,
                                                const Handle<ZeroInflationTermStructure>& h) const override {
        return QuantLib::ext::make_shared<T>(frequency_, false, isInterpolated, h);
    }

private:
    Frequency frequency_;
};

QuantLib::ext::shared_ptr<ZeroInflationIndex> parseZeroInflationIndex(const string& s,
                                                              const Handle<ZeroInflationTermStructure>& h) {
    const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();

    // If conventions are non-null and we have provided a convention of type InflationIndex with a name equal to the
    // string s, we use that convention to construct the inflation index.
    if (conventions) {
        pair<bool, QuantLib::ext::shared_ptr<Convention>> p = conventions->get(s, Convention::Type::ZeroInflationIndex);
        if (p.first) {
            auto c = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndexConvention>(p.second);
            auto index = QuantLib::ext::make_shared<ZeroInflationIndex>(s, c->region(), c->revised(),
                                                                c->frequency(), c->availabilityLag(), c->currency(), h);
            IndexNameTranslator::instance().add(index->name(), s);
            return index;
        }
    }

    static map<string, QuantLib::ext::shared_ptr<ZeroInflationIndexParserBase>> m = {
        {"AUCPI", QuantLib::ext::make_shared<ZeroInflationIndexParserWithFrequency<AUCPI>>(Quarterly)},
        {"AU CPI", QuantLib::ext::make_shared<ZeroInflationIndexParserWithFrequency<AUCPI>>(Quarterly)},
        {"BEHICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<BEHICP>>()},
        {"BE HICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<BEHICP>>()},
        {"EUHICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<EUHICP>>()},
        {"EU HICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<EUHICP>>()},
        {"EUHICPXT", QuantLib::ext::make_shared<ZeroInflationIndexParser<EUHICPXT>>()},
        {"EU HICPXT", QuantLib::ext::make_shared<ZeroInflationIndexParser<EUHICPXT>>()},
        {"FRHICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<FRHICP>>()},
        {"FR HICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<FRHICP>>()},
        {"FRCPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<FRCPI>>()},
        {"FR CPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<FRCPI>>()},
        {"UKRPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<UKRPI>>()},
        {"UK RPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<UKRPI>>()},
        {"USCPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<USCPI>>()},
        {"US CPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<USCPI>>()},
        {"ZACPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<ZACPI>>()},
        {"ZA CPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<ZACPI>>()},
        {"SECPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<SECPI>>()},
        {"DKCPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<DKCPI>>()},
        {"CACPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<CACPI>>()},
        {"ESCPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<ESCPI>>()},
        {"DECPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<DECPI>>()},
        {"DE CPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<DECPI>>()}};

    auto it = m.find(s);
    if (it != m.end()) {
        auto index = it->second->build(h);
        IndexNameTranslator::instance().add(index->name(), s);
        return index;
    } else {
        QL_FAIL("parseZeroInflationIndex: \"" << s << "\" not recognized");
    }
}


QuantLib::ext::shared_ptr<ZeroInflationIndex> parseZeroInflationIndex(const string& s,
    bool isInterpolated,
    const Handle<ZeroInflationTermStructure>& h) {
    
    const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();

    // If conventions are non-null and we have provided a convention of type InflationIndex with a name equal to the 
    // string s, we use that convention to construct the inflation index.
    if (conventions) {
        pair<bool, QuantLib::ext::shared_ptr<Convention>> p = conventions->get(s, Convention::Type::ZeroInflationIndex);
        if (p.first) {
            auto c = QuantLib::ext::dynamic_pointer_cast<ZeroInflationIndexConvention>(p.second);
            auto index = QuantLib::ext::make_shared<ZeroInflationIndex>(s, c->region(), c->revised(), isInterpolated,
                c->frequency(), c->availabilityLag(), c->currency(), h);
            IndexNameTranslator::instance().add(index->name(), s);
            return index;
        }
    }

    static map<string, QuantLib::ext::shared_ptr<ZeroInflationIndexParserBase>> m = {
        {"AUCPI", QuantLib::ext::make_shared<ZeroInflationIndexParserWithFrequency<AUCPI>>(Quarterly)},
        {"AU CPI", QuantLib::ext::make_shared<ZeroInflationIndexParserWithFrequency<AUCPI>>(Quarterly)},
        {"BEHICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<BEHICP>>()},
        {"BE HICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<BEHICP>>()},
        {"EUHICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<EUHICP>>()},
        {"EU HICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<EUHICP>>()},
        {"EUHICPXT", QuantLib::ext::make_shared<ZeroInflationIndexParser<EUHICPXT>>()},
        {"EU HICPXT", QuantLib::ext::make_shared<ZeroInflationIndexParser<EUHICPXT>>()},
        {"FRHICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<FRHICP>>()},
        {"FR HICP", QuantLib::ext::make_shared<ZeroInflationIndexParser<FRHICP>>()},
        {"FRCPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<FRCPI>>()},
        {"FR CPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<FRCPI>>()},
        {"UKRPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<UKRPI>>()},
        {"UK RPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<UKRPI>>()},
        {"USCPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<USCPI>>()},
        {"US CPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<USCPI>>()},
        {"ZACPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<ZACPI>>()},
        {"ZA CPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<ZACPI>>()},
        {"SECPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<SECPI>>()},
        {"DKCPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<DKCPI>>()},
        {"CACPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<CACPI>>()},
        {"ESCPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<ESCPI>>()},
        {"DECPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<DECPI>>()},
        {"DE CPI", QuantLib::ext::make_shared<ZeroInflationIndexParser<DECPI>>()}
    };

    auto it = m.find(s);
    if (it != m.end()) {
        QL_DEPRECATED_DISABLE_WARNING
        auto index = it->second->build(isInterpolated, h);
        QL_DEPRECATED_ENABLE_WARNING
        IndexNameTranslator::instance().add(index->name(), s);
        return index;
    } else {
        QL_FAIL("parseZeroInflationIndex: \"" << s << "\" not recognized");
    }
}

QuantLib::ext::shared_ptr<BondIndex> parseBondIndex(const string& name) {

    // Make sure the prefix is correct
    string prefix = name.substr(0, 5);
    QL_REQUIRE(prefix == "BOND-", "A bond index string must start with 'BOND-' but got " << prefix);

    // Now take the remainder of the string
    // for spot indices, this should just be the bond name
    // for future indices, this is of the form NAME-YYYY-MM or NAME-YYYY-MM-DD where NAME is the commodity name
    // (possibly containing hyphens) and YYYY-MM(-DD) is the expiry date of the futures contract
    Date expiry;
    string nameWoPrefix = name.substr(5);
    string bondName = nameWoPrefix;

    // Check for form NAME-YYYY-MM-DD
    if (nameWoPrefix.size() > 10) {
        string test = nameWoPrefix.substr(nameWoPrefix.size() - 10);
        if (boost::regex_match(test, boost::regex("\\d{4}-\\d{2}-\\d{2}"))) {
            expiry = parseDate(test);
            bondName = nameWoPrefix.substr(0, nameWoPrefix.size() - test.size() - 1);
        }
    }

    // Check for form NAME-YYYY-MM if NAME-YYYY-MM-DD failed
    if (expiry == Date() && nameWoPrefix.size() > 7) {
        string test = nameWoPrefix.substr(nameWoPrefix.size() - 7);
        if (boost::regex_match(test, boost::regex("\\d{4}-\\d{2}"))) {
            expiry = parseDate(test + "-01");
            bondName = nameWoPrefix.substr(0, nameWoPrefix.size() - test.size() - 1);
        }
    }

    // Create and return the required future index
    QuantLib::ext::shared_ptr<BondIndex> index;
    if (expiry != Date()) {
        index= QuantLib::ext::make_shared<BondFuturesIndex>(expiry, bondName);
    } else {
        index= QuantLib::ext::make_shared<BondIndex>(bondName);
    }
    IndexNameTranslator::instance().add(index->name(), name);
    return index;
}

QuantLib::ext::shared_ptr<ConstantMaturityBondIndex> parseConstantMaturityBondIndex(const string& name) {
    // Expected bondId structure with at least three tokens, separated by "-", of the form CMB-FAMILY-TERM, for example:
    // CMB-US-CMT-5Y, CMB-US-TIPS-10Y, CMB-UK-GILT-5Y, CMB-DE-BUND-10Y
    // with two tokens in the middle to define the family 
    std::vector<string> tokens;
    split(tokens, name, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() >= 3, "Generic Bond ID with at least two tokens separated by - expected, found " << name);

    // Make sure the prefix is correct
    std::string prefix = tokens[0];
    QL_REQUIRE(prefix == "CMB", "A constant maturity bond yield index string must start with 'CMB' but got " << prefix);

    std::string securityFamily = tokens[1];
    for (Size i = 2; i < tokens.size() - 1; ++i)
        securityFamily = securityFamily + "-" + tokens[i];
    Period underlyingPeriod = parsePeriod(tokens.back());
    QuantLib::ext::shared_ptr<ConstantMaturityBondIndex> i;
    try {
        i = QuantLib::ext::make_shared<ConstantMaturityBondIndex>(prefix + "-" + securityFamily, underlyingPeriod); 
    } catch(std::exception& e) {
        ALOG("error creating CMB index: " << e.what());
    }
    IndexNameTranslator::instance().add(i->name(), name);
    return i;
}

QuantLib::ext::shared_ptr<QuantExt::CommodityIndex> parseCommodityIndex(const string& name, bool hasPrefix,
                                                                const Handle<PriceTermStructure>& ts,
                                                                const Calendar& cal, const bool enforceFutureIndex) {

    // Whether we check for "COMM-" prefix depends on hasPrefix.
    string commName = name;
    if (hasPrefix) {
        // Make sure the prefix is correct
        string prefix = name.substr(0, 5);
        QL_REQUIRE(prefix == "COMM-", "A commodity index string must start with 'COMM-' but got " << prefix);
        commName = name.substr(5);
    }

    // Now take the remainder of the string
    // for spot indices, this should just be the commodity name (possibly containing hyphens)
    // for future indices, this is of the form NAME-YYYY-MM or NAME-YYYY-MM-DD where NAME is the commodity name
    // (possibly containing hyphens) and YYYY-MM(-DD) is the expiry date of the futures contract
    Date expiry;

    // Check for form NAME-YYYY-MM-DD
    if (commName.size() > 10) {
        string test = commName.substr(commName.size() - 10);
        if (boost::regex_match(test, boost::regex("\\d{4}-\\d{2}-\\d{2}"))) {
            expiry = parseDate(test);
            commName = commName.substr(0, commName.size() - test.size() - 1);
        }
    }

    // Check for form NAME-YYYY-MM if NAME-YYYY-MM-DD failed
    if (expiry == Date() && commName.size() > 7) {
        string test = commName.substr(commName.size() - 7);
        if (boost::regex_match(test, boost::regex("\\d{4}-\\d{2}"))) {
            expiry = parseDate(test + "-01");
            commName = commName.substr(0, commName.size() - test.size() - 1);
        }
    }

    // Name to use when creating the index. This may be updated if we have a commodity future convention and IndexName 
    // is provided by the convention.
    string indexName = commName;

    // Do we have a commodity future convention for the commodity.
    QuantLib::ext::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();
    pair<bool, QuantLib::ext::shared_ptr<Convention>> p = conventions->get(commName, Convention::Type::CommodityFuture);
    QuantLib::ext::shared_ptr<CommodityFutureConvention> convention;
    if (p.first) {

        convention = QuantLib::ext::dynamic_pointer_cast<CommodityFutureConvention>(p.second);

        if (!convention->indexName().empty())
            indexName = convention->indexName();

        // If we have provided OffPeakPowerIndexData, we use that to construct the off peak power commodity index.
        if (convention->offPeakPowerIndexData()) {

            const auto& oppIdxData = *convention->offPeakPowerIndexData();

            if (expiry == Date()) {
                // If expiry is still not set use any date (off peak index is calendar daily)
                expiry = Settings::instance().evaluationDate();
            }
            string suffix = "-" + to_string(expiry);

            auto offPeakIndex = QuantLib::ext::dynamic_pointer_cast<CommodityFuturesIndex>(parseCommodityIndex(
                oppIdxData.offPeakIndex() + suffix, false));
            auto peakIndex = QuantLib::ext::dynamic_pointer_cast<CommodityFuturesIndex>(parseCommodityIndex(
                oppIdxData.peakIndex() + suffix, false));

            auto index = QuantLib::ext::make_shared<OffPeakPowerIndex>(indexName, expiry, offPeakIndex, peakIndex,
                oppIdxData.offPeakHours(), oppIdxData.peakCalendar(), ts);
            IndexNameTranslator::instance().add(index->name(), hasPrefix ? name : "COMM-" + name);
            DLOG("parseCommodityIndex(" << name << ") -> " << index->name() << " with expiry " << index->expiryDate());
            return index;
        }
    }

    // Create and return the required future index
    QuantLib::ext::shared_ptr<CommodityIndex> index;
    if (expiry != Date() || (convention && enforceFutureIndex)) {

        // If expiry is empty, just use any valid expiry.
        if (expiry == Date()) {
            ConventionsBasedFutureExpiry feCalc(*convention);
            expiry = feCalc.nextExpiry();
        }

        bool keepDays = convention && convention->contractFrequency() == Daily;

        Calendar cdr = cal;
        if (convention && cdr == NullCalendar()) {
            cdr = convention->calendar();
        }

        auto basisCurve = ts.empty() ? nullptr :
            QuantLib::ext::dynamic_pointer_cast<CommodityBasisPriceTermStructure>(*ts);

        if (basisCurve) {
            index = QuantLib::ext::make_shared<CommodityBasisFutureIndex>(indexName, expiry, cdr, basisCurve);       
        } else {
            index = QuantLib::ext::make_shared<CommodityFuturesIndex>(indexName, expiry, cdr, keepDays, ts);
        }

        

    } else {
        index = QuantLib::ext::make_shared<CommoditySpotIndex>(commName, cal, ts);
    }
    IndexNameTranslator::instance().add(index->name(), index->name());
    DLOG("parseCommodityIndex(" << name << ") -> " << index->name() << " with expiry " << index->expiryDate());
    return index;
}

QuantLib::ext::shared_ptr<Index> parseIndex(const string& s) {
    QuantLib::ext::shared_ptr<QuantLib::Index> ret_idx;
    try {
        ret_idx = parseEquityIndex(s);
    } catch (...) {
    }
    if (!ret_idx) {
        try {
            ret_idx = parseBondIndex(s);
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseCommodityIndex(s, true, QuantLib::Handle<QuantExt::PriceTermStructure>(), QuantLib::NullCalendar(), false);
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseFxIndex(s);
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseGenericIndex(s);
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseConstantMaturityBondIndex(s);
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseIborIndex(s);
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseSwapIndex(s, Handle<YieldTermStructure>(), Handle<YieldTermStructure>());
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseZeroInflationIndex(s, Handle<ZeroInflationTermStructure>());
        } catch (...) {
        }
    }
    QL_REQUIRE(ret_idx, "parseIndex \"" << s << "\" not recognized");
    return ret_idx;
}

bool isOvernightIndex(const string& indexName) {
    
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(indexName, index)) {
        auto onIndex = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index);
        if (onIndex)
            return true;
    }

    return false;
}

bool isBmaIndex(const string& indexName) {
    QuantLib::ext::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(indexName, index)) {
        // in ore a bma index is parsed to a BMAIndexWrapper instance!
        return QuantLib::ext::dynamic_pointer_cast<BMAIndexWrapper>(index) != nullptr;
    }
    return false;
}

string internalIndexName(const string& indexName) {

    // Check that the indexName string is of the required form
    vector<string> tokens;
    split(tokens, indexName, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2 || tokens.size() == 3,
               "Two or three tokens required in " << indexName << ": CCY-INDEX or CCY-INDEX-TERM");

    // Static map of allowable alternative external names to our unique internal name
    static map<string, string> m = {{"DKK-TNR", "DKK-DKKOIS"}, {"EUR-EURIB", "EUR-EURIBOR"}, {"CAD-BA", "CAD-CDOR"},
                                    {"EUR-ESTR", "EUR-ESTER"}, {"EUR-STR", "EUR-ESTER"},     {"JPY-TONA", "JPY-TONAR"},
                                    {"JPY-TORF", "JPY-TONAR"}};

    // Is start of indexName covered by the map? If so, update it.
    string tmpName = tokens[0] + "-" + tokens[1];
    if (m.count(tmpName) == 1) {
        tmpName = m.at(tmpName);
    }

    // If there were only two tokens, return the possibly updated two tokens.
    if (tokens.size() == 2) {
        return tmpName;
    }

    // Check if we have an overnight index
    // This covers cases like USD-FedFunds-1D and returns USD-FedFunds
    // (no need to check convention based overnight indices, they are always of the form CCY-INDEX)
    if (parsePeriod(tokens[2]) == 1 * Days && isOvernightIndex(tmpName)) {
        return tmpName;
    }

    // Allow USD-SIFMA-1W or USD-SIFMA-7D externally. USD-SIFMA is used internally.
    if (tmpName == "USD-SIFMA" && (tokens[2] == "1W" || tokens[2] == "7D")) {
        return tmpName;
    }

    return tmpName + "-" + tokens[2];
}

bool isFxIndex(const std::string& indexName) {
    std::vector<string> tokens;
    split(tokens, indexName, boost::is_any_of("-"));
    return tokens.size() == 4 && tokens[0] == "FX";
}

std::string inverseFxIndex(const std::string& indexName) {
    std::vector<string> tokens;
    split(tokens, indexName, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 4 && tokens[0] == "FX", "no fx index given (" << indexName << ")");
    return "FX-" + tokens[1] + "-" + tokens[3] + "-" + tokens[2];
}

} // namespace data
} // namespace ore
