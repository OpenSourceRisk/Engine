/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include "pricetermstructureadapter.hpp"

#include <boost/assign/list_of.hpp>
#include <boost/make_shared.hpp>

#include <ql/math/interpolations/all.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>

#include <qle/termstructures/pricecurve.hpp>
#include <qle/termstructures/pricetermstructureadapter.hpp>

using namespace std;

using namespace boost::unit_test_framework;
using namespace boost::assign;

using namespace QuantLib;
using namespace QuantExt;

namespace {

class CommonData {
public:
    SavedSettings backup;

    Real tolerance;
    boost::shared_ptr<SimpleQuote> flatZero;
    DayCounter dayCounter;
    boost::shared_ptr<SimpleQuote> spotPrice;
    vector<Period> priceTenors;
    vector<boost::shared_ptr<SimpleQuote> > priceQuotes;

    CommonData() : tolerance(1e-10), priceTenors(4), priceQuotes(4) {
        flatZero = boost::make_shared<SimpleQuote>(0.015);
        dayCounter = Actual365Fixed();

        spotPrice = boost::make_shared<SimpleQuote>(14.5);
        priceTenors[0] = 6 * Months;  priceQuotes[0] = boost::make_shared<SimpleQuote>(16.7);
        priceTenors[1] = 1 * Years;   priceQuotes[1] = boost::make_shared<SimpleQuote>(19.9);
        priceTenors[2] = 2 * Years;   priceQuotes[2] = boost::make_shared<SimpleQuote>(28.5);
        priceTenors[3] = 5 * Years;   priceQuotes[3] = boost::make_shared<SimpleQuote>(38.8);
    }
};

}

namespace testsuite {

void PriceTermStructureAdapterTest::testImpliedZeroRates() {

    BOOST_TEST_MESSAGE("Testing implied zero rates from PriceTermStructureAdapter");

    CommonData td;

    // Set arbitrary evaluation date
    Date asof(27, Feb, 2018);
    Settings::instance().evaluationDate() = asof;

    // Discount curve
    boost::shared_ptr<YieldTermStructure> discount = boost::make_shared<FlatForward>(
        0, NullCalendar(), Handle<Quote>(td.flatZero), td.dayCounter);

    // Price curve
    vector<Time> times(td.priceTenors.size());
    vector<Handle<Quote> > prices(td.priceTenors.size());
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        times[i] = td.dayCounter.yearFraction(asof, asof + td.priceTenors[i]);
        prices[i] = Handle<Quote>(td.priceQuotes[i]);
    }
    boost::shared_ptr<PriceTermStructure> priceCurve = 
        boost::make_shared<InterpolatedPriceCurve<Linear> >(times, prices, td.dayCounter);

    // Adapted price curve i.e. implied yield termstructure
    PriceTermStructureAdapter priceAdapter(td.spotPrice, priceCurve, discount);

    // Check the implied zero rates
    for (Size i = 0; i < times.size(); i++) {
        Real impliedZero = priceAdapter.zeroRate(times[i], Continuous);
        Real expectedZero = td.flatZero->value() - std::log(td.priceQuotes[i]->value() / td.spotPrice->value()) / times[i];
        BOOST_CHECK_CLOSE(impliedZero, expectedZero, td.tolerance);
    }

    // Bump price curve and check again
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        td.priceQuotes[i]->setValue(td.priceQuotes[i]->value() * 1.10);
    }

    // Check the implied zero rates
    for (Size i = 0; i < times.size(); i++) {
        Real impliedZero = priceAdapter.zeroRate(times[i], Continuous);
        Real expectedZero = td.flatZero->value() - std::log(td.priceQuotes[i]->value() / td.spotPrice->value()) / times[i];
        BOOST_CHECK_CLOSE(impliedZero, expectedZero, td.tolerance);
    }

    // Bump spot price and check again
    td.spotPrice->setValue(td.spotPrice->value() * 1.05);
    for (Size i = 0; i < times.size(); i++) {
        Real impliedZero = priceAdapter.zeroRate(times[i], Continuous);
        Real expectedZero = td.flatZero->value() - std::log(td.priceQuotes[i]->value() / td.spotPrice->value()) / times[i];
        BOOST_CHECK_CLOSE(impliedZero, expectedZero, td.tolerance);
    }

    // Bump discount curve and check again
    td.flatZero->setValue(td.flatZero->value() * 0.9);
    for (Size i = 0; i < times.size(); i++) {
        Real impliedZero = priceAdapter.zeroRate(times[i], Continuous);
        Real expectedZero = td.flatZero->value() - std::log(td.priceQuotes[i]->value() / td.spotPrice->value()) / times[i];
        BOOST_CHECK_CLOSE(impliedZero, expectedZero, td.tolerance);
    }

}

void PriceTermStructureAdapterTest::testFloatingDiscountFixedPrice() {

    BOOST_TEST_MESSAGE("Testing behaviour of PriceTermStructureAdapter with floating reference discount curve and fixed reference price curve");

    CommonData td;

    // Set arbitrary evaluation date
    Date asof(27, Feb, 2018);
    Settings::instance().evaluationDate() = asof;

    // Discount curve (floating reference)
    boost::shared_ptr<YieldTermStructure> floatReferenceDiscountCurve = boost::make_shared<FlatForward>(
        0, NullCalendar(), Handle<Quote>(td.flatZero), td.dayCounter);

    // Price curve (fixed reference)
    vector<Date> dates(td.priceTenors.size() + 1);
    vector<Handle<Quote> > prices(td.priceTenors.size() + 1);
    dates[0] = asof; prices[0] = Handle<Quote>(td.spotPrice);
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        dates[i + 1] = asof + td.priceTenors[i];
        prices[i + 1] = Handle<Quote>(td.priceQuotes[i]);
    }
    boost::shared_ptr<PriceTermStructure> fixedReferencePriceCurve =
        boost::make_shared<InterpolatedPriceCurve<Linear> >(dates, prices, td.dayCounter);

    // Check construction of adapted price curve passes => reference dates same on construction
    BOOST_REQUIRE_NO_THROW(PriceTermStructureAdapter(td.spotPrice, fixedReferencePriceCurve, floatReferenceDiscountCurve));

    // Construct adapted price curve
    PriceTermStructureAdapter adaptedPriceCurve(td.spotPrice, fixedReferencePriceCurve, floatReferenceDiscountCurve);
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(0.5, Continuous));

    // Change Settings::instance().evaluationDate() - discount curve reference date changes and price curve's does not
    Settings::instance().evaluationDate() = asof + 1 * Days;
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(0.5, Continuous), Error);
}

void PriceTermStructureAdapterTest::testFixedDiscountFloatingPrice() {

    BOOST_TEST_MESSAGE("Testing behaviour of PriceTermStructureAdapter with fixed reference discount curve and floating reference price curve");

    CommonData td;

    // Set arbitrary evaluation date
    Date asof(27, Feb, 2018);
    Settings::instance().evaluationDate() = asof;

    // Discount curve (fixed reference)
    boost::shared_ptr<YieldTermStructure> floatReferenceDiscountCurve = boost::make_shared<FlatForward>(
        asof, Handle<Quote>(td.flatZero), td.dayCounter);

    // Price curve (floating reference)
    vector<Time> times(td.priceTenors.size());
    vector<Handle<Quote> > prices(td.priceTenors.size());
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        times[i] = td.dayCounter.yearFraction(asof, asof + td.priceTenors[i]);
        prices[i] = Handle<Quote>(td.priceQuotes[i]);
    }
    boost::shared_ptr<PriceTermStructure> fixedReferencePriceCurve =
        boost::make_shared<InterpolatedPriceCurve<Linear> >(times, prices, td.dayCounter);

    // Check construction of adapted price curve passes => reference dates same on construction
    BOOST_REQUIRE_NO_THROW(PriceTermStructureAdapter(td.spotPrice, fixedReferencePriceCurve, floatReferenceDiscountCurve));

    // Construct adapted price curve
    PriceTermStructureAdapter adaptedPriceCurve(td.spotPrice, fixedReferencePriceCurve, floatReferenceDiscountCurve);
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(0.5, Continuous));

    // Change Settings::instance().evaluationDate() - price curve reference date changes and discount curve's does not
    Settings::instance().evaluationDate() = asof + 1 * Days;
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(0.5, Continuous), Error);
}

void PriceTermStructureAdapterTest::testExtrapolation() {

    BOOST_TEST_MESSAGE("Testing extrapolation behaviour of PriceTermStructureAdapter");

    CommonData td;

    // Set arbitrary evaluation date
    Date asof(27, Feb, 2018);
    Settings::instance().evaluationDate() = asof;

    // Zero curve: times in ~ [0, 3], turn off extrapolation
    vector<Date> zeroDates(2);
    vector<Real> zeroRates(2);
    zeroDates[0] = asof;             zeroRates[0] = td.flatZero->value();
    zeroDates[1] = asof + 3 * Years; zeroRates[1] = td.flatZero->value();
    boost::shared_ptr<YieldTermStructure> zeroCurve = boost::make_shared<ZeroCurve>(
        zeroDates, zeroRates, td.dayCounter);

    // Price curve: times in ~ [0.5, 5], turn off extrapolation
    vector<Time> times(td.priceTenors.size());
    vector<Handle<Quote> > prices(td.priceTenors.size());
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        times[i] = td.dayCounter.yearFraction(asof, asof + td.priceTenors[i]);
        prices[i] = Handle<Quote>(td.priceQuotes[i]);
    }
    boost::shared_ptr<PriceTermStructure> priceCurve =
        boost::make_shared<InterpolatedPriceCurve<Linear> >(times, prices, td.dayCounter);

    // Check construction of adapted price curve passes
    BOOST_REQUIRE_NO_THROW(PriceTermStructureAdapter(td.spotPrice, priceCurve, zeroCurve));

    // Construct adapted price curve
    PriceTermStructureAdapter adaptedPriceCurve(td.spotPrice, priceCurve, zeroCurve);

    // Asking for zero at time ~ 1.0 should not throw
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(1.0, Continuous));
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(asof + 1 * Years, td.dayCounter, Continuous));

    // Asking for zero at time ~ 0.25 should throw because < 0.5, min time for price curve
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(0.25, Continuous), Error);
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(asof + 3 * Months, td.dayCounter, Continuous), Error);

    // Asking for zero at time ~ 4.0 should throw because > 3.0, max time for discount curve
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(4.0, Continuous), Error);
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(asof + 4 * Years, td.dayCounter, Continuous), Error);

    // Allow extrapolation on discount curve - should hit the max date restriction on the price curve at ~ t = 5
    zeroCurve->enableExtrapolation();
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(6.0, Continuous), Error);
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(asof + 6 * Years, td.dayCounter, Continuous), Error);

    // Allow extrapolation on price curve, expect no errors below min time or above max time
    priceCurve->enableExtrapolation();
    // above max
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(6.0, Continuous));
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(asof + 6 * Years, td.dayCounter, Continuous));
    // below min
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(0.25, Continuous));
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(asof + 3 * Months, td.dayCounter, Continuous));
}

test_suite* PriceTermStructureAdapterTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("PriceTermStructureAdapterTests");

    suite->add(BOOST_TEST_CASE(&PriceTermStructureAdapterTest::testImpliedZeroRates));
    suite->add(BOOST_TEST_CASE(&PriceTermStructureAdapterTest::testFloatingDiscountFixedPrice));
    suite->add(BOOST_TEST_CASE(&PriceTermStructureAdapterTest::testFixedDiscountFloatingPrice));
    suite->add(BOOST_TEST_CASE(&PriceTermStructureAdapterTest::testExtrapolation));

    return suite;
}

}
