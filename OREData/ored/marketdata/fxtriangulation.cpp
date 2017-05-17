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

#include <boost/make_shared.hpp>
#include <ored/marketdata/fxtriangulation.hpp>
#include <ql/errors.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace QuantLib;
using std::string;

namespace {
// helper classes to enable the building
// of composite cross-currency fx quotes
class Triangulation {
public:
    Triangulation() {}
    Real operator()(Real a, Real b) const { return a / b; }
};

class Product {
public:
    Product() {}
    Real operator()(Real a, Real b) const { return a * b; }
};

class InverseProduct {
public:
    InverseProduct() {}
    Real operator()(Real a, Real b) const { return 1.0 / (a * b); }
};

// Inverse a single quote
class Inverse {
public:
    Inverse() {}
    Real operator()(Real a) const { return 1.0 / a; }
};
}

namespace ore {
namespace data {

Handle<Quote> FXTriangulation::getQuote(const string& pair) const {
    // First, look for the pair in the map
    auto it = map_.find(pair);
    if (it != map_.end())
        return it->second;

    // Now we have to break the pair up and search for it.
    QL_REQUIRE(pair.size() == 6, "Invalid ccypair " << pair);
    string domestic = pair.substr(0, 3);
    string foreign = pair.substr(3);

    // check reverse
    string reverse = foreign + domestic;
    it = map_.find(reverse);
    if (it != map_.end()) {
        Handle<Quote> invertedQuote(boost::make_shared<DerivedQuote<Inverse>>(it->second, Inverse()));
        map_[pair] = invertedQuote;
        return invertedQuote;
    }

    // check EUREUR
    if (foreign == domestic) {
        static Handle<Quote> unity(boost::make_shared<SimpleQuote>(1.0));
        return unity;
    }

    // Now we search for a pair of quotes that we can combine to construct the quote required.
    // We only search for a pair of quotes a single step apart.
    //
    // Suppose we want a USDJPY quote and we have EUR based data, there are 4 combinations to
    // consider:
    // EURUSD, EURJPY  => we want EURJPY / EURUSD [Triangulation]
    // EURUSD, JPYEUR  => we want 1 / (EURUSD * JPYEUR) [InverseProduct]
    // USDEUR, EURJPY  => we want USDEUR * EURJPY [Product]
    // USDEUR, JPYEUR  => we want USDEUR / JPYEUR [Triangulation (but in the reverse order)]
    //
    // Loop over the map, look for domestic then use the map to find the other side of the pair.
    for (const auto& kv : map_) {
        string keyDomestic = kv.first.substr(0, 3);
        string keyForeign = kv.first.substr(3);
        const Handle<Quote>& q1 = kv.second;

        if (domestic == keyDomestic) {
            // we have domestic, now look for foreign/keyForeign
            // USDEUR, JPYEUR  => we want USDEUR / JPYEUR [Triangulation (but in the reverse order)]
            it = map_.find(foreign + keyForeign);
            if (it != map_.end()) {
                // Here q1 is USDEUR and it->second is JPYEUR
                map_[pair] =
                    Handle<Quote>(boost::make_shared<CompositeQuote<Triangulation>>(q1, it->second, Triangulation()));
                return map_[pair];
            }
            // USDEUR, EURJPY  => we want USDEUR * EURJPY [Product]
            it = map_.find(keyForeign + foreign);
            if (it != map_.end()) {
                map_[pair] = Handle<Quote>(boost::make_shared<CompositeQuote<Product>>(q1, it->second, Product()));
                return map_[pair];
            }
        }

        if (domestic == keyForeign) {
            // EURUSD, JPYEUR  => we want 1 / (EURUSD * JPYEUR) [InverseProduct]
            it = map_.find(foreign + keyDomestic);
            if (it != map_.end()) {
                map_[pair] =
                    Handle<Quote>(boost::make_shared<CompositeQuote<InverseProduct>>(q1, it->second, InverseProduct()));
                return map_[pair];
            }
            // EURUSD, EURJPY  => we want EURJPY / EURUSD [Triangulation]
            it = map_.find(keyDomestic + foreign);
            if (it != map_.end()) {
                // Here q1 is EURUSD and it->second is EURJPY
                map_[pair] =
                    Handle<Quote>(boost::make_shared<CompositeQuote<Triangulation>>(it->second, q1, Triangulation()));
                return map_[pair];
            }
        }
    }

    QL_FAIL("Unable to build FXQuote for ccy pair " << pair);
}
}
}
