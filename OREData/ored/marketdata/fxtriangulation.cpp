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
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/configuration/conventions.hpp>
#include <ql/errors.hpp>
#include <ql/quotes/compositequote.hpp>
#include <ql/quotes/derivedquote.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace QuantLib;
using namespace QuantExt;
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

} // namespace

namespace ore {
namespace data {

FxSpotQuote::FxSpotQuote(Handle<Quote> todaysQuote, const Handle<YieldTermStructure>& sourceYts,
                         const Handle<YieldTermStructure>& targetYts, Natural fixingDays,
                         const Calendar& fixingCalendar, Date refDate)
    : todaysQuote_(todaysQuote), sourceYts_(sourceYts), targetYts_(targetYts), fixingDays_(fixingDays),
      fixingCalendar_(fixingCalendar), refDate_(refDate) {
    registerWith(todaysQuote_);
    registerWith(sourceYts_);
    registerWith(targetYts_);
}

Real FxSpotQuote::value() const {
    QL_ENSURE(isValid(), "invalid FxRateQuote");

    Date today = refDate_ == Null<Date>() ? Settings::instance().evaluationDate() : refDate_;
    Date refValueDate = fixingCalendar_.advance(fixingCalendar_.adjust(today), fixingDays_, Days);

    return todaysQuote_->value() * sourceYts_->discount(refValueDate) / targetYts_->discount(refValueDate);
}

bool FxSpotQuote::isValid() const {
    return !todaysQuote_.empty() && todaysQuote_->isValid() && !sourceYts_.empty() && !targetYts_.empty();
}

void FxSpotQuote::update() { notifyObservers(); }

void FXTriangulation::addQuote(const std::string& pair, const Handle<Quote>& spot) {
    auto it = std::find_if(map_.begin(), map_.end(),
                           [&pair](const std::pair<string, Handle<Quote>>& x) { return x.first == pair; });
    if (it != map_.end())
        *it = std::make_pair(pair, spot);
    else
        map_.push_back(std::make_pair(pair, spot));
}

Handle<Quote> FXTriangulation::getQuote(const string& pair) const {
    // First, look for the pair in the map
    auto it = std::find_if(map_.begin(), map_.end(),
                           [&pair](const std::pair<string, Handle<Quote>>& x) { return x.first == pair; });
    if (it != map_.end())
        return it->second;

    // Now we have to break the pair up and search for it.
    QL_REQUIRE(pair.size() == 6, "FXTriangulation: Invalid ccypair " << pair);
    string domestic = pair.substr(0, 3);
    string foreign = pair.substr(3);

    // check reverse
    string reverse = foreign + domestic;
    it = std::find_if(map_.begin(), map_.end(),
                      [&reverse](const std::pair<string, Handle<Quote>>& x) { return x.first == reverse; });
    if (it != map_.end()) {
        Handle<Quote> invertedQuote(boost::make_shared<DerivedQuote<Inverse>>(it->second, Inverse()));
        map_.push_back(std::make_pair(pair, invertedQuote));
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
            it = std::find_if(map_.begin(), map_.end(),
                              [&foreign, &keyForeign](const std::pair<string, Handle<Quote>>& x) {
                                  return x.first == foreign + keyForeign;
                              });
            if (it != map_.end()) {
                // Here q1 is USDEUR and it->second is JPYEUR
                auto tmp =
                    Handle<Quote>(boost::make_shared<CompositeQuote<Triangulation>>(q1, it->second, Triangulation()));
                map_.push_back(std::make_pair(pair, tmp));
                return tmp;
            }
            // USDEUR, EURJPY  => we want USDEUR * EURJPY [Product]
            it = std::find_if(map_.begin(), map_.end(),
                              [&foreign, &keyForeign](const std::pair<string, Handle<Quote>>& x) {
                                  return x.first == keyForeign + foreign;
                              });
            if (it != map_.end()) {
                auto tmp = Handle<Quote>(boost::make_shared<CompositeQuote<Product>>(q1, it->second, Product()));
                map_.push_back(std::make_pair(pair, tmp));
                return tmp;
            }
        }

        if (domestic == keyForeign) {
            // EURUSD, JPYEUR  => we want 1 / (EURUSD * JPYEUR) [InverseProduct]
            it = std::find_if(map_.begin(), map_.end(),
                              [&foreign, &keyDomestic](const std::pair<string, Handle<Quote>>& x) {
                                  return x.first == foreign + keyDomestic;
                              });
            if (it != map_.end()) {
                auto tmp =
                    Handle<Quote>(boost::make_shared<CompositeQuote<InverseProduct>>(q1, it->second, InverseProduct()));
                map_.push_back(std::make_pair(pair, tmp));
                return tmp;
            }
            // EURUSD, EURJPY  => we want EURJPY / EURUSD [Triangulation]
            it = std::find_if(map_.begin(), map_.end(),
                              [&foreign, &keyDomestic](const std::pair<string, Handle<Quote>>& x) {
                                  return x.first == keyDomestic + foreign;
                              });
            if (it != map_.end()) {
                // Here q1 is EURUSD and it->second is EURJPY
                auto tmp =
                    Handle<Quote>(boost::make_shared<CompositeQuote<Triangulation>>(it->second, q1, Triangulation()));
                map_.push_back(std::make_pair(pair, tmp));
                return tmp;
            }
        }
    }

    QL_FAIL("FXTriangulation: Unable to build FXQuote for ccy pair " << pair);
}

void FXIndexTriangulation::addIndex(const std::string& pair, const Handle<FxIndex>& index) {
    auto it = std::find_if(map_.begin(), map_.end(),
                           [&pair](const std::pair<string, Handle<FxIndex>>& x) { return x.first == pair; });
    if (it != map_.end())
        *it = std::make_pair(pair, index);
    else
        map_.push_back(std::make_pair(pair, index));
}

Handle<FxIndex> FXIndexTriangulation::getIndex(const string& pair, bool dontThrow) const {
    // First, look for the pair in the map
    auto it = std::find_if(map_.begin(), map_.end(),
                           [&pair](const std::pair<string, Handle<FxIndex>>& x) { return x.first == pair; });
    if (it != map_.end())
        return it->second;

    if (!isFxIndex(pair)) {

        // Now we have to break the pair up and search for it.
        QL_REQUIRE(pair.size() == 6, "FXIndexTriangulation: Invalid ccypair " << pair);
        string domestic = pair.substr(0, 3);
        string foreign = pair.substr(3);

        // check reverse
        string reverse = foreign + domestic;
        it = std::find_if(map_.begin(), map_.end(),
                          [&reverse](const std::pair<string, Handle<FxIndex>>& x) { return x.first == reverse; });
        if (it != map_.end()) {
            Handle<Quote> invertedQuote(
                boost::make_shared<DerivedQuote<Inverse>>(it->second->fxQuote(true), Inverse()));
            Handle<FxIndex> invertedIndex(boost::make_shared<FxIndex>(
                it->second->referenceDate(), it->second->familyName(), it->second->fixingDays(),
                it->second->targetCurrency(), it->second->sourceCurrency(), it->second->fixingCalendar(), invertedQuote,
                it->second->targetCurve(), it->second->sourceCurve()));
            map_.push_back(std::make_pair(pair, invertedIndex));
            return invertedIndex;
        }

        // check EUREUR
        if (foreign == domestic) {
            static Handle<FxIndex> unity(boost::make_shared<FxIndex>(
                "", 0, parseCurrency(foreign), parseCurrency(domestic), parseCalendar(foreign),
                Handle<Quote>(boost::make_shared<SimpleQuote>(1.0))));
            return unity;
        }

        // Now we search for a pair of indices that we can combine to construct the indexx required.
        // We only search for a pair of indices a single step apart.
        //
        // Suppose we want a USDJPY quote and we have EUR based data, there are 4 combinations to
        // consider:
        // EURUSD, EURJPY  => we want EURJPY / EURUSD [Triangulation]
        // EURUSD, JPYEUR  => we want 1 / (EURUSD * JPYEUR) [InverseProduct]
        // USDEUR, EURJPY  => we want USDEUR * EURJPY [Product]
        // USDEUR, JPYEUR  => we want USDEUR / JPYEUR [Triangulation (but in the reverse order)]
        //
        // Loop over the map, look for domestic then use the map to find the other side of the pair.

        // Here q1 is USDEUR and it->second is JPYEUR
        Natural spotDays;
        Calendar calendar;
        getFxIndexConventions(pair, spotDays, calendar);

        for (const auto& kv : map_) {
            if (!isFxIndex(kv.first)) {
                string keyDomestic = kv.first.substr(0, 3);
                string keyForeign = kv.first.substr(3);
                const Handle<FxIndex>& q1 = kv.second;

                if (domestic == keyDomestic) {
                    // we have domestic, now look for foreign/keyForeign
                    // USDEUR, JPYEUR  => we want USDEUR / JPYEUR [Triangulation (but in the reverse order)]
                    it = std::find_if(map_.begin(), map_.end(),
                                      [&foreign, &keyForeign](const std::pair<string, Handle<FxIndex>>& x) {
                                          return x.first == foreign + keyForeign;
                                      });
                    if (it != map_.end()) {
                        // we need a spot quote for the crossed fx index. The two underying spot quotes could
                        // have different spotDays so we can't just cross them directly. We cross the two 'todays'
                        // rates and then construst a derived FX Spot quote with the correct fixing days
                        Handle<Quote> todaysCross = Handle<Quote>(boost::make_shared<CompositeQuote<Triangulation>>(
                            q1->fxQuote(), it->second->fxQuote(), Triangulation()));
                        Handle<Quote> spotCross = Handle<Quote>(
                            boost::make_shared<FxSpotQuote>(todaysCross, q1->sourceCurve(), it->second->sourceCurve(),
                                                            spotDays, calendar, q1->referenceDate()));

                        auto tmp = Handle<FxIndex>(
                            boost::make_shared<FxIndex>(q1->referenceDate(), q1->familyName(), spotDays,
                                                        q1->sourceCurrency(), it->second->sourceCurrency(), calendar,
                                                        spotCross, q1->sourceCurve(), it->second->sourceCurve()));
                        map_.push_back(std::make_pair(pair, tmp));
                        return tmp;
                    }
                    // USDEUR, EURJPY  => we want USDEUR * EURJPY [Product]
                    it = std::find_if(map_.begin(), map_.end(),
                                      [&foreign, &keyForeign](const std::pair<string, Handle<FxIndex>>& x) {
                                          return x.first == keyForeign + foreign;
                                      });
                    if (it != map_.end()) {
                        Handle<Quote> todaysCross = Handle<Quote>(boost::make_shared<CompositeQuote<Product>>(
                            q1->fxQuote(), it->second->fxQuote(), Product()));
                        Handle<Quote> spotCross = Handle<Quote>(
                            boost::make_shared<FxSpotQuote>(todaysCross, q1->sourceCurve(), it->second->targetCurve(),
                                                            spotDays, calendar, q1->referenceDate()));

                        auto tmp = Handle<FxIndex>(
                            boost::make_shared<FxIndex>(q1->referenceDate(), q1->familyName(), spotDays,
                                                        q1->sourceCurrency(), it->second->targetCurrency(), calendar,
                                                        spotCross, q1->sourceCurve(), it->second->targetCurve()));
                        map_.push_back(std::make_pair(pair, tmp));
                        return tmp;
                    }
                }

                if (domestic == keyForeign) {
                    // EURUSD, JPYEUR  => we want 1 / (EURUSD * JPYEUR) [InverseProduct]
                    it = std::find_if(map_.begin(), map_.end(),
                                      [&foreign, &keyDomestic](const std::pair<string, Handle<FxIndex>>& x) {
                                          return x.first == foreign + keyDomestic;
                                      });
                    if (it != map_.end()) {
                        Handle<Quote> todaysCross = Handle<Quote>(boost::make_shared<CompositeQuote<InverseProduct>>(
                            q1->fxQuote(), it->second->fxQuote(), InverseProduct()));
                        Handle<Quote> spotCross = Handle<Quote>(
                            boost::make_shared<FxSpotQuote>(todaysCross, q1->targetCurve(), it->second->sourceCurve(),
                                                            spotDays, calendar, q1->referenceDate()));

                        auto tmp = Handle<FxIndex>(
                            boost::make_shared<FxIndex>(q1->referenceDate(), q1->familyName(), spotDays,
                                                        q1->targetCurrency(), it->second->sourceCurrency(), calendar,
                                                        spotCross, q1->targetCurve(), it->second->sourceCurve()));
                        map_.push_back(std::make_pair(pair, tmp));
                        return tmp;
                    }
                    // EURUSD, EURJPY  => we want EURJPY / EURUSD [Triangulation]
                    it = std::find_if(map_.begin(), map_.end(),
                                      [&foreign, &keyDomestic](const std::pair<string, Handle<FxIndex>>& x) {
                                          return x.first == keyDomestic + foreign;
                                      });
                    if (it != map_.end()) {
                        // Here q1 is EURUSD and it->second is EURJPY
                        Handle<Quote> todaysCross = Handle<Quote>(boost::make_shared<CompositeQuote<Triangulation>>(
                            it->second->fxQuote(), q1->fxQuote(), Triangulation()));
                        Handle<Quote> spotCross = Handle<Quote>(
                            boost::make_shared<FxSpotQuote>(todaysCross, q1->targetCurve(), it->second->targetCurve(),
                                                            spotDays, calendar, q1->referenceDate()));

                        auto tmp = Handle<FxIndex>(
                            boost::make_shared<FxIndex>(q1->referenceDate(), q1->familyName(), spotDays,
                                                        q1->targetCurrency(), it->second->targetCurrency(), calendar,
                                                        spotCross, q1->targetCurve(), it->second->targetCurve()));
                        map_.push_back(std::make_pair(pair, tmp));
                        return tmp;
                    }
                }
            }
        }
    }
    // if don't throw return an empty handle
    if (dontThrow)
        return Handle<FxIndex>();

    QL_FAIL("FXIndexTriangulation: Unable to build FxIndex for ccy pair " << pair);
}
} // namespace data
} // namespace ore
