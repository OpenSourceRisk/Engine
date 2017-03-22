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

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/configuration/conventions.hpp>
#include <ql/errors.hpp>
#include <ql/indexes/all.hpp>
#include <ql/time/daycounters/all.hpp>
#include <ql/time/calendars/target.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/indexes/all.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/make_shared.hpp>
#include <map>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;
using ore::data::Convention;

namespace ore {
namespace data {

// Helper build classes for static map

class IborIndexParser {
public:
    virtual boost::shared_ptr<IborIndex> build(Period p, const Handle<YieldTermStructure>& h) const = 0;
};

template <class T> class IborIndexParserWithPeriod : public IborIndexParser {
public:
    boost::shared_ptr<IborIndex> build(Period p, const Handle<YieldTermStructure>& h) const override {
        QL_REQUIRE(p != 1 * Days, "must have a period longer than 1D");
        return boost::make_shared<T>(p, h);
    }
};

template <class T> class IborIndexParserOIS : public IborIndexParser {
public:
    boost::shared_ptr<IborIndex> build(Period p, const Handle<YieldTermStructure>& h) const override {
        QL_REQUIRE(p == 1 * Days, "must have period 1D");
        return boost::make_shared<T>(h);
    }
};
boost::shared_ptr<FxIndex> parseFxIndex(const string& s) {
    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 4, "four tokens required in " << s << ": FX-TAG-CCY1-CCY2");
    QL_REQUIRE(tokens[0] == "FX", "expected first token to be FX");
    return boost::make_shared<FxIndex>(tokens[0] + "/" + tokens[1], 0, parseCurrency(tokens[2]),
                                       parseCurrency(tokens[3]), NullCalendar());
}

boost::shared_ptr<IborIndex> parseIborIndex(const string& s, const Handle<YieldTermStructure>& h) {

    std::vector<string> tokens;
    split(tokens, s, boost::is_any_of("-"));

    QL_REQUIRE(tokens.size() == 2 || tokens.size() == 3, "Two or three tokens required in "
                                                             << s << ": CCY-INDEX or CCY-INDEX-TERM");

    Period p;
    if (tokens.size() == 3)
        p = parsePeriod(tokens[2]);
    else
        p = 1 * Days;

    static map<string, boost::shared_ptr<IborIndexParser>> m = {
        {"EUR-EONIA", boost::make_shared<IborIndexParserOIS<Eonia>>()},
        {"GBP-SONIA", boost::make_shared<IborIndexParserOIS<Sonia>>()},
        {"JPY-TONAR", boost::make_shared<IborIndexParserOIS<Tonar>>()},
        {"CHF-TOIS", boost::make_shared<IborIndexParserOIS<CHFTois>>()},
        {"USD-FedFunds", boost::make_shared<IborIndexParserOIS<FedFunds>>()},
        {"AUD-BBSW", boost::make_shared<IborIndexParserWithPeriod<AUDbbsw>>()},
        {"AUD-LIBOR", boost::make_shared<IborIndexParserWithPeriod<AUDLibor>>()},
        {"EUR-EURIBOR", boost::make_shared<IborIndexParserWithPeriod<Euribor>>()},
        {"EUR-EURIB", boost::make_shared<IborIndexParserWithPeriod<Euribor>>()},
        {"CAD-CDOR", boost::make_shared<IborIndexParserWithPeriod<Cdor>>()},
        {"CAD-BA", boost::make_shared<IborIndexParserWithPeriod<Cdor>>()},
        {"CZK-PRIBOR", boost::make_shared<IborIndexParserWithPeriod<CZKPribor>>()},
        {"EUR-LIBOR", boost::make_shared<IborIndexParserWithPeriod<EURLibor>>()},
        {"USD-LIBOR", boost::make_shared<IborIndexParserWithPeriod<USDLibor>>()},
        {"GBP-LIBOR", boost::make_shared<IborIndexParserWithPeriod<GBPLibor>>()},
        {"JPY-LIBOR", boost::make_shared<IborIndexParserWithPeriod<JPYLibor>>()},
        {"JPY-TIBOR", boost::make_shared<IborIndexParserWithPeriod<Tibor>>()},
        {"CAD-LIBOR", boost::make_shared<IborIndexParserWithPeriod<CADLibor>>()},
        {"CHF-LIBOR", boost::make_shared<IborIndexParserWithPeriod<CHFLibor>>()},
        {"SEK-LIBOR", boost::make_shared<IborIndexParserWithPeriod<SEKLibor>>()},
        {"SEK-STIBOR", boost::make_shared<IborIndexParserWithPeriod<SEKStibor>>()},
        {"NOK-NIBOR", boost::make_shared<IborIndexParserWithPeriod<NOKNibor>>()},
        {"HKD-HIBOR", boost::make_shared<IborIndexParserWithPeriod<HKDHibor>>()},
        {"SGD-SIBOR", boost::make_shared<IborIndexParserWithPeriod<SGDSibor>>()},
        {"SGD-SOR", boost::make_shared<IborIndexParserWithPeriod<SGDSor>>()},
        {"DKK-CIBOR", boost::make_shared<IborIndexParserWithPeriod<DKKCibor>>()},
        {"DKK-LIBOR", boost::make_shared<IborIndexParserWithPeriod<DKKLibor>>()},
        {"HUF-BUBOR", boost::make_shared<IborIndexParserWithPeriod<HUFBubor>>()},
        {"IDR-IDRFIX", boost::make_shared<IborIndexParserWithPeriod<IDRIdrfix>>()},
        {"INR-MIFOR", boost::make_shared<IborIndexParserWithPeriod<INRMifor>>()},
        {"MXN-TIIE", boost::make_shared<IborIndexParserWithPeriod<MXNTiie>>()},
        {"PLN-WIBOR", boost::make_shared<IborIndexParserWithPeriod<PLNWibor>>()},
        {"SKK-BRIBOR", boost::make_shared<IborIndexParserWithPeriod<SKKBribor>>()},
        {"NZD-BKBM", boost::make_shared<IborIndexParserWithPeriod<NZDBKBM>>()},
        {"TWD-TAIBOR", boost::make_shared<IborIndexParserWithPeriod<TWDTaibor>>()},
        {"MYR-KLIBOR", boost::make_shared<IborIndexParserWithPeriod<MYRKlibor>>()},
        {"KRW-KORIBOR", boost::make_shared<IborIndexParserWithPeriod<KRWKoribor>>()},
        {"ZAR-JIBAR", boost::make_shared<IborIndexParserWithPeriod<Jibar>>()}};

    auto it = m.find(tokens[0] + "-" + tokens[1]);
    if (it != m.end()) {
        return it->second->build(p, h);
    } else {
        QL_FAIL("parseIborIndex \"" << s << "\" not recognized");
    }
}

// Swap Index Parser base
class SwapIndexParser {
public:
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
    boost::shared_ptr<IborIndex> index = convention->index()->clone(f);
    Period tenor(convention->fixedFrequency());

    if (d.empty())
        return boost::make_shared<SwapIndex>(familyName, // familyName
                                             p,
                                             index->fixingDays(), // settlementDays
                                             ccy, convention->fixedCalendar(),
                                             tenor,                         // fixedLegTenor
                                             convention->fixedConvention(), // fixedLegConvention
                                             convention->fixedDayCounter(), // fixedLegDaycounter
                                             index);
    else
        return boost::make_shared<SwapIndex>(familyName, p, index->fixingDays(), ccy, convention->fixedCalendar(),
                                             tenor, convention->fixedConvention(), convention->fixedDayCounter(), index,
                                             d);
}

boost::shared_ptr<Index> parseIndex(const string& s, const Handle<YieldTermStructure>& h) {
    try {
        // TODO: Added non Ibor checks first (Inflation, etc)
        if (s.size() > 2 && s.substr(0, 2) == "FX") {
            return parseFxIndex(s);
        } else {
            return parseIborIndex(s, h);
        }
    } catch (...) {
        QL_FAIL("parseIndex \"" << s << "\" not recognized");
    }
}
}
}
