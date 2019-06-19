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

/*! \file ored/utilities/indexparser.cpp
    \brief
    \ingroup utilities
*/

#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <map>
#include <ored/configuration/conventions.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>
#include <ql/indexes/all.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/all.hpp>
#include <qle/indexes/bmaindexwrapper.hpp>
#include <qle/indexes/cacpi.hpp>
#include <qle/indexes/dkcpi.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/indexes/genericiborindex.hpp>
#include <qle/indexes/ibor/audbbsw.hpp>
#include <qle/indexes/ibor/brlcdi.hpp>
#include <qle/indexes/ibor/chfsaron.hpp>
#include <qle/indexes/ibor/chftois.hpp>
#include <qle/indexes/ibor/clpcamara.hpp>
#include <qle/indexes/ibor/copibr.hpp>
#include <qle/indexes/ibor/corra.hpp>
#include <qle/indexes/ibor/czkpribor.hpp>
#include <qle/indexes/ibor/demlibor.hpp>
#include <qle/indexes/ibor/dkkcibor.hpp>
#include <qle/indexes/ibor/dkkois.hpp>
#include <qle/indexes/ibor/hkdhibor.hpp>
#include <qle/indexes/ibor/hufbubor.hpp>
#include <qle/indexes/ibor/idridrfix.hpp>
#include <qle/indexes/ibor/idrjibor.hpp>
#include <qle/indexes/ibor/ilstelbor.hpp>
#include <qle/indexes/ibor/inrmifor.hpp>
#include <qle/indexes/ibor/krwcd.hpp>
#include <qle/indexes/ibor/krwkoribor.hpp>
#include <qle/indexes/ibor/mxntiie.hpp>
#include <qle/indexes/ibor/myrklibor.hpp>
#include <qle/indexes/ibor/noknibor.hpp>
#include <qle/indexes/ibor/nowa.hpp>
#include <qle/indexes/ibor/nzdbkbm.hpp>
#include <qle/indexes/ibor/plnpolonia.hpp>
#include <qle/indexes/ibor/phpphiref.hpp>
#include <qle/indexes/ibor/plnwibor.hpp>
#include <qle/indexes/ibor/rubmosprime.hpp>
#include <qle/indexes/ibor/saibor.hpp>
#include <qle/indexes/ibor/seksior.hpp>
#include <qle/indexes/ibor/sekstibor.hpp>
#include <qle/indexes/ibor/sgdsibor.hpp>
#include <qle/indexes/ibor/sgdsor.hpp>
#include <qle/indexes/ibor/skkbribor.hpp>
#include <qle/indexes/ibor/thbbibor.hpp>
#include <qle/indexes/ibor/tonar.hpp>
#include <qle/indexes/ibor/twdtaibor.hpp>
#include <qle/indexes/secpi.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using ore::data::Convention;

namespace ore {
namespace data {

// Helper function to build an IborIndex with a specific period and term structure given an instance of the same IborIndex
boost::shared_ptr<IborIndex> build(const boost::shared_ptr<IborIndex>& index, const Period& p, const Handle<YieldTermStructure>& h) {

    QL_REQUIRE(p != 1 * Days, "The index family " << index->familyName() << " must have tenor greater than 1 day");

    // Deal with specific cases first

    // MXN TIIE
    // If tenor equates to 28 Days, i.e. tenor is 4W or 28D, ensure that the index is created
    // with a tenor of 4W under the hood. Things work better this way especially cap floor stripping.
    if (boost::shared_ptr<MXNTiie> idx = boost::dynamic_pointer_cast<MXNTiie>(index)) {
        if (p.units() == Days && p.length() == 28) {
            return boost::make_shared<MXNTiie>(4 * Weeks, h);
        } else {
            return boost::make_shared<MXNTiie>(p, h);
        }
    }

    // KRW CD
    // If tenor equates to 91 Days, ensure that the index is created with a tenor of 3M under the hood.
    if (boost::shared_ptr<KRWCd> idx = boost::dynamic_pointer_cast<KRWCd>(index)) {
        if (p.units() == Days && p.length() == 91) {
            return boost::make_shared<KRWCd>(3 * Months, h);
        } else {
            return boost::make_shared<KRWCd>(p, h);
        }
    }

    // General case
    return boost::make_shared<IborIndex>(index->familyName(), p, index->fixingDays(), index->currency(), 
        index->fixingCalendar(), index->businessDayConvention(), index->endOfMonth(), index->dayCounter(), h);
}

// Helper function to check that index name to index object is a one-to-one mapping
void checkOneToOne(const map<string, OvernightIndex>& onIndices, const map<string, boost::shared_ptr<IborIndex>>& iborIndices) {

    // Should not attempt to add the same family name to the set if the provided mappings are one to one
    set<string> familyNames;

    for (const auto& kv : onIndices) {
        auto p = familyNames.insert(kv.second.familyName());
        QL_REQUIRE(p.second, "Duplicate mapping for overnight index family " << *p.first << " not allowed");
    }

    for (const auto& kv : iborIndices) {
        auto p = familyNames.insert(kv.second->familyName());
        QL_REQUIRE(p.second, "Duplicate mapping for ibor index family " << *p.first << " not allowed");
    }
}

boost::shared_ptr<FxIndex> parseFxIndex(const string& s) {
    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 4, "four tokens required in " << s << ": FX-TAG-CCY1-CCY2");
    QL_REQUIRE(tokens[0] == "FX", "expected first token to be FX");
    return boost::make_shared<FxIndex>(tokens[0] + "/" + tokens[1], 0, parseCurrency(tokens[2]),
                                       parseCurrency(tokens[3]), NullCalendar());
}

boost::shared_ptr<EquityIndex> parseEquityIndex(const string& s) {
    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2, "two tokens required in " << s << ": EQ-NAME");
    QL_REQUIRE(tokens[0] == "EQ", "expected first token to be EQ");
    if (tokens.size() == 2) {
        return boost::make_shared<EquityIndex>(tokens[1], NullCalendar(), Currency());
    } else {
        QL_FAIL("Error parsing equity string " + s);
    }
}

bool tryParseIborIndex(const string& s, boost::shared_ptr<IborIndex>& index) {
    try {
        index = parseIborIndex(s);
    } catch (...) {
        return false;
    }
    return true;
}

boost::shared_ptr<IborIndex> parseIborIndex(const string& s, const Handle<YieldTermStructure>& h) {
    string dummy;
    return parseIborIndex(s, dummy, h);
}

boost::shared_ptr<IborIndex> parseIborIndex(const string& s, string& tenor, const Handle<YieldTermStructure>& h) {

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

    // Map from our _unique internal name_ to an overnight index
    static map<string, OvernightIndex> onIndices = {
        { "EUR-EONIA", Eonia() },
        { "GBP-SONIA", Sonia() },
        { "JPY-TONAR", Tonar() },
        { "CHF-TOIS", CHFTois() },
        { "CHF-SARON", CHFSaron() },
        { "USD-FedFunds", FedFunds() },
        { "AUD-AONIA", Aonia() },
        { "CAD-CORRA", CORRA() },
        { "DKK-DKKOIS", DKKOis() },
        { "SEK-SIOR", SEKSior() },
        { "COP-IBR", COPIbr() },
        { "BRL-CDI", BRLCdi() },
        { "NOK-NOWA", Nowa() },
        { "CLP-CAMARA", CLPCamara() },
        { "NZD-OCR", Nzocr() },
        { "PLN-POLONIA", PLNPolonia() }
    };

    // Map from our _unique internal name_ to an ibor index (the period does not matter here)
    static map<string, boost::shared_ptr<IborIndex>> iborIndices = {
        { "AUD-BBSW", boost::make_shared<AUDbbsw>(3 * Months) },
        { "AUD-LIBOR", boost::make_shared<AUDLibor>(3 * Months) },
        { "EUR-EURIBOR", boost::make_shared<Euribor>(3 * Months) },
        { "CAD-CDOR", boost::make_shared<Cdor>(3 * Months) },
        { "CNY-SHIBOR", boost::make_shared<Shibor>(3 * Months) },
        { "CZK-PRIBOR", boost::make_shared<CZKPribor>(3 * Months) },
        { "EUR-LIBOR", boost::make_shared<EURLibor>(3 * Months) },
        { "USD-LIBOR", boost::make_shared<USDLibor>(3 * Months) },
        { "GBP-LIBOR", boost::make_shared<GBPLibor>(3 * Months) },
        { "JPY-LIBOR", boost::make_shared<JPYLibor>(3 * Months) },
        { "JPY-TIBOR", boost::make_shared<Tibor>(3 * Months) },
        { "CAD-LIBOR", boost::make_shared<CADLibor>(3 * Months) },
        { "CHF-LIBOR", boost::make_shared<CHFLibor>(3 * Months) },
        { "SEK-LIBOR", boost::make_shared<SEKLibor>(3 * Months) },
        { "SEK-STIBOR", boost::make_shared<SEKStibor>(3 * Months) },
        { "NOK-NIBOR", boost::make_shared<NOKNibor>(3 * Months) },
        { "HKD-HIBOR", boost::make_shared<HKDHibor>(3 * Months) },
        { "SAR-SAIBOR", boost::make_shared<SAibor>(3 * Months) },
        { "SGD-SIBOR", boost::make_shared<SGDSibor>(3 * Months) },
        { "SGD-SOR", boost::make_shared<SGDSor>(3 * Months) },
        { "DKK-CIBOR", boost::make_shared<DKKCibor>(3 * Months) },
        { "DKK-LIBOR", boost::make_shared<DKKLibor>(3 * Months) },
        { "HUF-BUBOR", boost::make_shared<HUFBubor>(3 * Months) },
        { "IDR-IDRFIX", boost::make_shared<IDRIdrfix>(3 * Months) },
        { "IDR-JIBOR", boost::make_shared<IDRJibor>(3 * Months) },
        { "ILS-TELBOR", boost::make_shared<ILSTelbor>(3 * Months) },
        { "INR-MIFOR", boost::make_shared<INRMifor>(3 * Months) },
        { "MXN-TIIE", boost::make_shared<MXNTiie>(3 * Months) },
        { "PLN-WIBOR", boost::make_shared<PLNWibor>(3 * Months) },
        { "SKK-BRIBOR", boost::make_shared<SKKBribor>(3 * Months) },
        { "NZD-BKBM", boost::make_shared<NZDBKBM>(3 * Months) },
        { "TRY-TRLIBOR", boost::make_shared<TRLibor>(3 * Months) },
        { "TWD-TAIBOR", boost::make_shared<TWDTaibor>(3 * Months) },
        { "MYR-KLIBOR", boost::make_shared<MYRKlibor>(3 * Months) },
        { "KRW-CD", boost::make_shared<KRWCd>(3 * Months) },
        { "KRW-KORIBOR", boost::make_shared<KRWKoribor>(3 * Months) },
        { "ZAR-JIBAR", boost::make_shared<Jibar>(3 * Months) },
        { "RUB-MOSPRIME", boost::make_shared<RUBMosprime>(3 * Months) },
        { "THB-BIBOR", boost::make_shared<THBBibor>(3 * Months) },
        { "PHP-PHIREF", boost::make_shared<PHPPhiref>(3 * Months) },
        { "DEM-LIBOR", boost::make_shared<DEMLibor>(3 * Months) }
    };

    // Check (once) that we have a one-to-one mapping
    static bool checked = false;
    if (!checked) {
        checkOneToOne(onIndices, iborIndices);
        checked = true;
    }

    // Simple single case for USD-SIFMA (i.e. BMA)
    if (indexStem == "USD-SIFMA") {
        QL_REQUIRE(tenor.empty(), "A tenor is not allowed with USD-SIFMA as it is implied");
        return boost::make_shared<BMAIndexWrapper>(boost::make_shared<BMAIndex>(h));
    }

    // Overnight indices
    auto onIt = onIndices.find(indexStem);
    if (onIt != onIndices.end()) {
        QL_REQUIRE(tenor.empty(), "A tenor is not allowed with the overnight index " << indexStem << " as it is implied");
        return onIt->second.clone(h);
    }

    // Ibor indices with a tenor
    auto it = iborIndices.find(indexStem);
    if (it != iborIndices.end()) {
        Period p = parsePeriod(tenor);
        return build(it->second, p, h);
    }

    // GENERIC indices
    if (tokens[1] == "GENERIC") {
        Period p = parsePeriod(tenor);
        auto ccy = parseCurrency(tokens[0]);
        return boost::make_shared<GenericIborIndex>(p, ccy, h);
    }

    QL_FAIL("parseIborIndex \"" << s << "\" not recognized");
}

bool isGenericIndex(const string& indexName) { 
    return indexName.find("-GENERIC-") != string::npos;
}

bool isInflationIndex(const string& indexName) {
    try {
        // Currently, only way to have an inflation index is to have a ZeroInflationIndex
        parseZeroInflationIndex(indexName);
    } catch (...) {
        return false;
    }
    return true;
}

// Swap Index Parser base
class SwapIndexParser {
public:
    virtual ~SwapIndexParser() {}
    virtual boost::shared_ptr<SwapIndex> build(Period p, const Handle<YieldTermStructure>& f,
                                               const Handle<YieldTermStructure>& d) const = 0;
};

// We build with both a forwarding and discounting curve
template <class T> class SwapIndexParserDualCurve : public SwapIndexParser {
public:
    boost::shared_ptr<SwapIndex> build(Period p, const Handle<YieldTermStructure>& f,
                                       const Handle<YieldTermStructure>& d) const override {
        return boost::make_shared<T>(p, f, d);
    }
};

boost::shared_ptr<SwapIndex> parseSwapIndex(const string& s, const Handle<YieldTermStructure>& f,
                                            const Handle<YieldTermStructure>& d,
                                            boost::shared_ptr<data::IRSwapConvention> convention) {

    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));

    QL_REQUIRE(tokens.size() == 3, "three tokens required in " << s << ": CCY-CMS-TENOR");
    QL_REQUIRE(tokens[0].size() == 3, "invalid currency code in " << s);
    QL_REQUIRE(tokens[1] == "CMS", "expected CMS as middle token in " << s);

    Period p = parsePeriod(tokens[2]);

    string familyName = tokens[0] + "LiborSwapIsdaFix";
    Currency ccy = parseCurrency(tokens[0]);

    boost::shared_ptr<IborIndex> index =
        f.empty() || !convention ? boost::shared_ptr<IborIndex>() : convention->index()->clone(f);
    QuantLib::Natural settlementDays = index ? index->fixingDays() : 0;
    QuantLib::Calendar calender = convention ? convention->fixedCalendar() : NullCalendar();
    Period fixedLegTenor = convention ? Period(convention->fixedFrequency()) : Period(1, Months);
    BusinessDayConvention fixedLegConvention = convention ? convention->fixedConvention() : ModifiedFollowing;
    DayCounter fixedLegDayCounter = convention ? convention->fixedDayCounter() : ActualActual();

    if (d.empty())
        return boost::make_shared<SwapIndex>(familyName, p, settlementDays, ccy, calender, fixedLegTenor,
                                             fixedLegConvention, fixedLegDayCounter, index);
    else
        return boost::make_shared<SwapIndex>(familyName, p, settlementDays, ccy, calender, fixedLegTenor,
                                             fixedLegConvention, fixedLegDayCounter, index, d);
}

// Zero Inflation Index Parser
class ZeroInflationIndexParserBase {
public:
    virtual ~ZeroInflationIndexParserBase() {}
    virtual boost::shared_ptr<ZeroInflationIndex> build(bool isInterpolated,
                                                        const Handle<ZeroInflationTermStructure>& h) const = 0;
};

template <class T> class ZeroInflationIndexParser : public ZeroInflationIndexParserBase {
public:
    boost::shared_ptr<ZeroInflationIndex> build(bool isInterpolated,
                                                const Handle<ZeroInflationTermStructure>& h) const override {
        return boost::make_shared<T>(isInterpolated, h);
    }
};

boost::shared_ptr<ZeroInflationIndex> parseZeroInflationIndex(const string& s, bool isInterpolated,
                                                              const Handle<ZeroInflationTermStructure>& h) {

    static map<string, boost::shared_ptr<ZeroInflationIndexParserBase>> m = {
        {"EUHICP", boost::make_shared<ZeroInflationIndexParser<EUHICP>>()},
        {"EU HICP", boost::make_shared<ZeroInflationIndexParser<EUHICP>>()},
        {"EUHICPXT", boost::make_shared<ZeroInflationIndexParser<EUHICPXT>>()},
        {"EU HICPXT", boost::make_shared<ZeroInflationIndexParser<EUHICPXT>>()},
        {"FRHICP", boost::make_shared<ZeroInflationIndexParser<FRHICP>>()},
        {"FR HICP", boost::make_shared<ZeroInflationIndexParser<FRHICP>>()},
        {"UKRPI", boost::make_shared<ZeroInflationIndexParser<UKRPI>>()},
        {"UK RPI", boost::make_shared<ZeroInflationIndexParser<UKRPI>>()},
        {"USCPI", boost::make_shared<ZeroInflationIndexParser<USCPI>>()},
        {"US CPI", boost::make_shared<ZeroInflationIndexParser<USCPI>>()},
        {"ZACPI", boost::make_shared<ZeroInflationIndexParser<ZACPI>>()},
        {"ZA CPI", boost::make_shared<ZeroInflationIndexParser<ZACPI>>()},
        {"SECPI", boost::make_shared<ZeroInflationIndexParser<SECPI>>()},
        {"DKCPI", boost::make_shared<ZeroInflationIndexParser<DKCPI>>()},
        {"CACPI", boost::make_shared<ZeroInflationIndexParser<CACPI>>()}};

    auto it = m.find(s);
    if (it != m.end()) {
        return it->second->build(isInterpolated, h);
    } else {
        QL_FAIL("parseZeroInflationIndex: \"" << s << "\" not recognized");
    }
}

boost::shared_ptr<BondIndex> parseBondIndex(const string& s) {
    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 2, "two tokens required in " << s << ": BOND-SECURITY");
    QL_REQUIRE(tokens[0] == "BOND", "expected first token to be BOND");
    return boost::make_shared<BondIndex>(tokens[1]);
}

boost::shared_ptr<Index> parseIndex(const string& s, const data::Conventions& conventions) {
    boost::shared_ptr<QuantLib::Index> ret_idx;
    try {
        ret_idx = parseIborIndex(s);
    } catch (...) {
    }
    if (!ret_idx) {
        try {
            auto c = boost::dynamic_pointer_cast<SwapIndexConvention>(conventions.get(s));
            QL_REQUIRE(c, "no swap index convention");
            auto c2 = boost::dynamic_pointer_cast<IRSwapConvention>(conventions.get(c->conventions()));
            QL_REQUIRE(c2, "no swap convention");
            ret_idx = parseSwapIndex(s, Handle<YieldTermStructure>(), Handle<YieldTermStructure>(), c2);
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseZeroInflationIndex(s);
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
            ret_idx = parseEquityIndex(s);
        } catch (...) {
        }
    }
    if (!ret_idx) {
        try {
            ret_idx = parseBondIndex(s);
        } catch (...) {
        }
    }
    QL_REQUIRE(ret_idx, "parseIndex \"" << s << "\" not recognized");
    return ret_idx;
}

bool isOvernightIndex(const string& indexName) {

    boost::shared_ptr<IborIndex> index;
    if (tryParseIborIndex(indexName, index)) {
        auto onIndex = boost::dynamic_pointer_cast<OvernightIndex>(index);
        if (onIndex)
            return true;
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
    static map<string, string> m = {
        { "DKK-TNR", "DKK-DKKOIS" },
        { "EUR-EURIB", "EUR-EURIBOR" },
        { "CAD-BA", "CAD-CDOR" }
    };
    
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
    if (isOvernightIndex(tmpName)) {
        Period p = parsePeriod(tokens[2]);
        QL_REQUIRE(p == 1 * Days, "The period " << tokens[2] << " is not compatible with the overnight index " << tmpName);
        return tmpName;
    }

    return tmpName + "-" + tokens[2];
}

} // namespace data
} // namespace ore
