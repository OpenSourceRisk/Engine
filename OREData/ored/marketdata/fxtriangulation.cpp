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

#include <ored/configuration/conventions.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/quotes/compositevectorquote.hpp>

#include <ql/errors.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/quotes/simplequote.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

namespace {

std::pair<std::string, std::string> splitPair(const std::string& pair) {
    QL_REQUIRE(pair.size() == 6, "FXTriangulation: Invalid currency pair '" << pair << "'");
    return std::make_pair(pair.substr(0, 3), pair.substr(3));
}

Handle<YieldTermStructure> getMarketDiscountCurve(const Market* market, const std::string& ccy,
                                                  const std::string& configuration) {
    try {
        return market->discountCurve(ccy, configuration);
    } catch (const std::exception&) {
        WLOG("FXTriangulation: could not get market discount curve '"
             << ccy << "' (requested for configuration '" << configuration
             << "') - discounted fx spot rates will be replaced by non-discounted rates in future calculations, which "
                "might lead to inaccuracies");
        return Handle<YieldTermStructure>();
    }
}

} // namespace

FXTriangulation::FXTriangulation(std::map<std::string, Handle<Quote>> quotes) : quotes_(std::move(quotes)) {

    LOG("FXTriangulation: initializing");

    // collect all currencies from the pairs

    std::set<std::string> ccys;
    for (auto const& q : quotes_) {
        auto [ccy1, ccy2] = splitPair(q.first);
        ccys.insert(ccy1);
        ccys.insert(ccy2);
        TLOG("FXTriangulation: adding quote " << q.first);
    }

    /* - populate node to ccy vector
       - we insert currencies in the order we want to use them for triangulation if there
         are several shortest paths from CCY1 to CCY2 */

    static vector<string> ccyOrder = {"USD", "EUR", "GBP", "CHF", "JPY", "AUD", "CAD", "ZAR"};

    std::set<std::string> remainingCcys(ccys);

    for (auto const& c : ccyOrder) {
        if (ccys.find(c) != ccys.end()) {
            nodeToCcy_.push_back(c);
            remainingCcys.erase(c);
        }
    }

    for (auto const& c : remainingCcys) {
        nodeToCcy_.push_back(c);
    }

    // populate ccy to node vector

    for (Size i = 0; i < nodeToCcy_.size(); ++i)
        ccyToNode_[nodeToCcy_[i]] = i;

    // populate neighbours container

    neighbours_.resize(nodeToCcy_.size());
    for (auto const& q : quotes_) {
        auto [ccy1, ccy2] = splitPair(q.first);
        Size n1 = ccyToNode_[ccy1];
        Size n2 = ccyToNode_[ccy2];
        neighbours_[n1].insert(n2);
        neighbours_[n2].insert(n1);
    }

    LOG("FXTriangulation: initialized with " << quotes_.size() << " quotes, " << ccys.size() << " currencies.");
}

Handle<Quote> FXTriangulation::getQuote(const std::string& pair) const {

    // do we have a cached result?

    if (auto it = quoteCache_.find(pair); it != quoteCache_.end())
        return it->second;

    // we need to construct the quote from the input quotes

    Handle<Quote> result;
    auto [ccy1, ccy2] = splitPair(pair);

    // handle trivial case

    if (ccy1 == ccy2)
        return Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.0));

    // get the path from ccy1 to ccy2

    auto path = getPath(ccy1, ccy2);

    if (path.size() == 2) {

        // we can use a direct or inverted quote, but do not need a composite

        result = getQuote(path[0], path[1]);

    } else {

        // we need a composite quote

        std::vector<Handle<Quote>> quotes;

        // collect the quotes on the path and check consistency of the settlement dates

        for (Size i = 0; i < path.size() - 1; ++i) {
            quotes.push_back(getQuote(path[i], path[i + 1]));
        }

        // build the composite quote

        auto f = [](const std::vector<Real>& quotes) {
            return std::accumulate(quotes.begin(), quotes.end(), 1.0, std::multiplies());
        };
        result = Handle<Quote>(QuantLib::ext::make_shared<CompositeVectorQuote<decltype(f)>>(quotes, f));
    }

    // add the result to the lookup cache and return it

    quoteCache_[pair] = result;
    return result;
}

Handle<FxIndex> FXTriangulation::getIndex(const std::string& indexOrPair, const Market* market,
                                          const std::string& configuration) const {

    // do we have a cached result?

    if (auto it = indexCache_.find(std::make_pair(indexOrPair, configuration)); it != indexCache_.end()) {
        return it->second;
    }

    // otherwise we need to construct the index

    Handle<FxIndex> result;

    std::string familyName;
    std::string forCcy;
    std::string domCcy;

    if (isFxIndex(indexOrPair)) {
        auto ind = parseFxIndex(indexOrPair);
        familyName = ind->familyName();
        forCcy = ind->sourceCurrency().code();
        domCcy = ind->targetCurrency().code();
    } else {
        familyName = "GENERIC";
        std::tie(forCcy, domCcy) = splitPair(indexOrPair);
    }

    // get the conventions of the result index

    auto [fixingDays, fixingCalendar, bdc] = getFxIndexConventions(indexOrPair);

    // get the discount curves for the result index

    auto sourceYts = getMarketDiscountCurve(market, forCcy, configuration);
    auto targetYts = getMarketDiscountCurve(market, domCcy, configuration);

    // get the path from ccy1 to ccy2

    auto path = getPath(forCcy, domCcy);

    if (path.size() == 2) {

        // we can use a direct or inverted quote, but do not need a composite

        auto fxSpot = getQuote(path[0], path[1]);
        result = Handle<FxIndex>(QuantLib::ext::make_shared<FxIndex>(familyName, fixingDays, parseCurrency(forCcy),
                                                             parseCurrency(domCcy), fixingCalendar, fxSpot, sourceYts,
                                                             targetYts));

    } else {

        // we need a composite quote

        std::vector<Handle<Quote>> quotes;

        // collect the quotes on the path and store them as FxRate quotes ("as of today" - quotes)

        for (Size i = 0; i < path.size() - 1; ++i) {

            auto q = getQuote(path[i], path[i + 1]);

            // we store a quote "as of today" to account for possible spot lag differences

            auto [fd, fc, bdc] = getFxIndexConventions(path[i] + path[i + 1]);
            auto s_yts = getMarketDiscountCurve(market, path[i], configuration);
            auto t_yts = getMarketDiscountCurve(market, path[i + 1], configuration);
            quotes.push_back(Handle<Quote>(QuantLib::ext::make_shared<FxRateQuote>(q, s_yts, t_yts, fd, fc)));
        }

        // build the composite quote "as of today"

        auto f = [](const std::vector<Real>& quotes) {
            return std::accumulate(quotes.begin(), quotes.end(), 1.0, std::multiplies());
        };
        Handle<Quote> compQuote(QuantLib::ext::make_shared<CompositeVectorQuote<decltype(f)>>(quotes, f));

        // build the spot quote

        Handle<Quote> spotQuote(
            QuantLib::ext::make_shared<FxSpotQuote>(compQuote, sourceYts, targetYts, fixingDays, fixingCalendar));

        // build the idnex

        result = Handle<FxIndex>(QuantLib::ext::make_shared<FxIndex>(familyName, fixingDays, parseCurrency(forCcy),
                                                             parseCurrency(domCcy), fixingCalendar, spotQuote,
                                                             sourceYts, targetYts));
    }

    // add the result to the lookup cache and return it

    indexCache_[std::make_pair(indexOrPair, configuration)] = result;
    return result;
}

std::vector<std::string> FXTriangulation::getPath(const std::string& forCcy, const std::string& domCcy) const {

    // see https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm

    Size sourceNode, targetNode;

    if (auto it = ccyToNode_.find(forCcy); it != ccyToNode_.end()) {
        sourceNode = it->second;
    } else {
        QL_FAIL("FXTriangulation: no conversion from '"
                << forCcy << "' to '" << domCcy << "' possible, since '" << forCcy
                << "' is not available as one of the currencies in any of the quotes (" << getAllQuotes() << ")");
    }

    if (auto it = ccyToNode_.find(domCcy); it != ccyToNode_.end()) {
        targetNode = it->second;
    } else {
        QL_FAIL("FXTriangulation: no conversion from '"
                << forCcy << "' to '" << domCcy << "' possible, since '" << domCcy
                << "' is not available as one of the currencies in any of the quotes (" << getAllQuotes() << ")");
    }

    // previous node on the current shortest path
    std::vector<Size> prev(nodeToCcy_.size(), Null<Size>());
    // distance per node
    std::vector<Size> dist(nodeToCcy_.size(), std::numeric_limits<Size>::max());
    // visited flag
    std::vector<bool> visited(nodeToCcy_.size(), false);

    // init source
    dist[sourceNode] = 0;

    // main loop
    Size noVisited = 0;
    while (noVisited < nodeToCcy_.size()) {
        Size u = Null<Size>(), min = std::numeric_limits<Size>::max();
        for (Size i = 0; i < dist.size(); ++i) {
            if (!visited[i] && dist[i] < min) {
                u = i;
                min = dist[i];
            }
        }
        QL_REQUIRE(u != Null<Size>(), "FXTriangulation: internal error, no minimum found in dist array for '"
                                          << forCcy << "' to '" << domCcy << "'. Quotes = " << getAllQuotes());
        if (u == targetNode)
            break;
        visited[u] = true;
        ++noVisited;
        for (auto const& n : neighbours_[u]) {
            if (visited[n])
                continue;
            Size alt = dist[u] + 1;
            if (alt < dist[n]) {
                dist[n] = alt;
                prev[n] = u;
            }
        }
    }

    // did we find a path?
    if (dist[targetNode] < std::numeric_limits<Size>::max()) {
        std::vector<std::string> result;
        Size u = targetNode;
        while (u != sourceNode) {
            result.insert(result.begin(), nodeToCcy_[u]);
            u = prev[u];
            QL_REQUIRE(u != Null<Size>(), "FXTriangulation: internal error u == null for '"
                                              << forCcy << "' to '" << domCcy
                                              << "'. Contact dev. Quotes = " << getAllQuotes() << ".");
        }
        result.insert(result.begin(), nodeToCcy_[sourceNode]);
        TLOG("FXTriangulation: found path of length "
             << result.size() - 1 << " from '" << forCcy << "' to '" << domCcy << "': "
             << std::accumulate(
                    result.begin(), result.end(), std::string(),
                    [](const std::string& s1, const std::string& s2) { return s1.empty() ? s2 : s1 + "-" + s2; }));
        return result;
    }

    QL_FAIL("FXTriangulation: no path from '" << forCcy << "' to '" << domCcy
                                              << "' found. Quotes = " << getAllQuotes());
}

Handle<Quote> FXTriangulation::getQuote(const std::string& forCcy, const std::string& domCcy) const {
    if (auto it = quotes_.find(forCcy + domCcy); it != quotes_.end()) {
        return it->second;
    }
    if (auto it = quotes_.find(domCcy + forCcy); it != quotes_.end()) {
        auto f = [](Real x) { return 1.0 / x; };
        return Handle<Quote>(QuantLib::ext::make_shared<DerivedQuote<decltype(f)>>(it->second, f));
    }
    QL_FAIL(
        "FXTriangulation::getQuote(" << forCcy << domCcy
                                     << ") - no such quote available. This is an internal error. Contact dev. Quotes = "
                                     << getAllQuotes());
}

std::string FXTriangulation::getAllQuotes() const {
    std::string result;
    if (quotes_.size() > 0) {
        for (auto const& d : quotes_) {
            result += d.first + ",";
        }
        result.erase(std::next(result.end(), -1));
    }
    return result;
}

} // namespace data
} // namespace ore
