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

#include "toplevelfixture.hpp"
#include <boost/assign/list_of.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/currencies/america.hpp>
#include <ql/math/interpolations/all.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

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
    QuantLib::ext::shared_ptr<SimpleQuote> flatZero;
    DayCounter dayCounter;
    vector<Period> priceTenors;
    vector<QuantLib::ext::shared_ptr<SimpleQuote> > priceQuotes;
    USDCurrency currency;

    CommonData() : tolerance(1e-10), priceTenors(5), priceQuotes(5) {
        flatZero = QuantLib::ext::make_shared<SimpleQuote>(0.015);
        dayCounter = Actual365Fixed();

        priceTenors[0] = 0 * Days;
        priceQuotes[0] = QuantLib::ext::make_shared<SimpleQuote>(14.5);
        priceTenors[1] = 6 * Months;
        priceQuotes[1] = QuantLib::ext::make_shared<SimpleQuote>(16.7);
        priceTenors[2] = 1 * Years;
        priceQuotes[2] = QuantLib::ext::make_shared<SimpleQuote>(19.9);
        priceTenors[3] = 2 * Years;
        priceQuotes[3] = QuantLib::ext::make_shared<SimpleQuote>(28.5);
        priceTenors[4] = 5 * Years;
        priceQuotes[4] = QuantLib::ext::make_shared<SimpleQuote>(38.8);
    }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PriceTermStructureAdapterTest)

BOOST_AUTO_TEST_CASE(testImpliedZeroRates) {

    BOOST_TEST_MESSAGE("Testing implied zero rates from PriceTermStructureAdapter");

    CommonData td;

    // Set arbitrary evaluation date
    Date asof(27, Feb, 2018);
    Settings::instance().evaluationDate() = asof;

    // Discount curve
    QuantLib::ext::shared_ptr<YieldTermStructure> discount =
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), Handle<Quote>(td.flatZero), td.dayCounter);

    // Price curve
    vector<Time> times(td.priceTenors.size());
    vector<Handle<Quote> > prices(td.priceTenors.size());
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        times[i] = td.dayCounter.yearFraction(asof, asof + td.priceTenors[i]);
        prices[i] = Handle<Quote>(td.priceQuotes[i]);
    }
    QuantLib::ext::shared_ptr<PriceTermStructure> priceCurve =
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear> >(td.priceTenors, prices, td.dayCounter, td.currency);

    // Adapted price curve i.e. implied yield termstructure
    PriceTermStructureAdapter priceAdapter(priceCurve, discount);

    // Check the implied zero rates
    Real spot = priceCurve->price(0);
    for (Size i = 1; i < times.size(); i++) {
        Real impliedZero = priceAdapter.zeroRate(times[i], Continuous);
        Real expectedZero = td.flatZero->value() - std::log(td.priceQuotes[i]->value() / spot) / times[i];
        BOOST_CHECK_CLOSE(impliedZero, expectedZero, td.tolerance);
    }

    // Bump price curve and check again
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        td.priceQuotes[i]->setValue(td.priceQuotes[i]->value() * 1.10);
    }

    // Check the implied zero rates
    spot = priceCurve->price(0);
    for (Size i = 1; i < times.size(); i++) {
        Real impliedZero = priceAdapter.zeroRate(times[i], Continuous);
        Real expectedZero = td.flatZero->value() - std::log(td.priceQuotes[i]->value() / spot) / times[i];
        BOOST_CHECK_CLOSE(impliedZero, expectedZero, td.tolerance);
    }

    // Bump discount curve and check again
    td.flatZero->setValue(td.flatZero->value() * 0.9);
    for (Size i = 1; i < times.size(); i++) {
        Real impliedZero = priceAdapter.zeroRate(times[i], Continuous);
        Real expectedZero = td.flatZero->value() - std::log(td.priceQuotes[i]->value() / spot) / times[i];
        BOOST_CHECK_CLOSE(impliedZero, expectedZero, td.tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testFloatingDiscountFixedPrice) {

    BOOST_TEST_MESSAGE("Testing behaviour of PriceTermStructureAdapter with floating reference discount curve and "
                       "fixed reference price curve");

    CommonData td;

    // Set arbitrary evaluation date
    Date asof(27, Feb, 2018);
    Settings::instance().evaluationDate() = asof;

    // Discount curve (floating reference)
    QuantLib::ext::shared_ptr<YieldTermStructure> floatReferenceDiscountCurve =
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), Handle<Quote>(td.flatZero), td.dayCounter);

    // Price curve (fixed reference)
    vector<Date> dates(td.priceTenors.size());
    vector<Handle<Quote> > prices(td.priceTenors.size());
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        dates[i] = asof + td.priceTenors[i];
        prices[i] = Handle<Quote>(td.priceQuotes[i]);
    }
    QuantLib::ext::shared_ptr<PriceTermStructure> fixedReferencePriceCurve =
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear> >(asof, dates, prices, td.dayCounter, td.currency);

    // Check construction of adapted price curve passes => reference dates same on construction
    BOOST_REQUIRE_NO_THROW(PriceTermStructureAdapter(fixedReferencePriceCurve, floatReferenceDiscountCurve));

    // Construct adapted price curve
    PriceTermStructureAdapter adaptedPriceCurve(fixedReferencePriceCurve, floatReferenceDiscountCurve);
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(0.5, Continuous));

    // Change Settings::instance().evaluationDate() - discount curve reference date changes and price curve's does not
    Settings::instance().evaluationDate() = asof + 1 * Days;
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(0.5, Continuous), Error);
}

BOOST_AUTO_TEST_CASE(testFixedDiscountFloatingPrice) {

    BOOST_TEST_MESSAGE("Testing behaviour of PriceTermStructureAdapter with fixed reference discount curve and "
                       "floating reference price curve");

    CommonData td;

    // Set arbitrary evaluation date
    Date asof(27, Feb, 2018);
    Settings::instance().evaluationDate() = asof;

    // Discount curve (fixed reference)
    QuantLib::ext::shared_ptr<YieldTermStructure> floatReferenceDiscountCurve =
        QuantLib::ext::make_shared<FlatForward>(asof, Handle<Quote>(td.flatZero), td.dayCounter);

    // Price curve (floating reference)
    vector<Handle<Quote> > prices(td.priceTenors.size());
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        prices[i] = Handle<Quote>(td.priceQuotes[i]);
    }
    QuantLib::ext::shared_ptr<PriceTermStructure> fixedReferencePriceCurve =
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear> >(td.priceTenors, prices, td.dayCounter, td.currency);

    // Check construction of adapted price curve passes => reference dates same on construction
    BOOST_REQUIRE_NO_THROW(PriceTermStructureAdapter(fixedReferencePriceCurve, floatReferenceDiscountCurve));

    // Construct adapted price curve
    PriceTermStructureAdapter adaptedPriceCurve(fixedReferencePriceCurve, floatReferenceDiscountCurve);
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(0.5, Continuous));

    // Change Settings::instance().evaluationDate() - price curve reference date changes and discount curve's does not
    Settings::instance().evaluationDate() = asof + 1 * Days;
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(0.5, Continuous), Error);
}

BOOST_AUTO_TEST_CASE(testExtrapolation) {

    BOOST_TEST_MESSAGE("Testing extrapolation behaviour of PriceTermStructureAdapter");

    CommonData td;

    // Set arbitrary evaluation date
    Date asof(27, Feb, 2018);
    Settings::instance().evaluationDate() = asof;

    // Zero curve: times in ~ [0, 3], turn off extrapolation
    vector<Date> zeroDates(2);
    vector<Real> zeroRates(2);
    zeroDates[0] = asof;
    zeroRates[0] = td.flatZero->value();
    zeroDates[1] = asof + 3 * Years;
    zeroRates[1] = td.flatZero->value();
    QuantLib::ext::shared_ptr<YieldTermStructure> zeroCurve =
        QuantLib::ext::make_shared<ZeroCurve>(zeroDates, zeroRates, td.dayCounter);

    // Price curve: times in ~ [0, 5], turn off extrapolation
    vector<Time> times(td.priceTenors.size());
    vector<Handle<Quote> > prices(td.priceTenors.size());
    for (Size i = 0; i < td.priceTenors.size(); i++) {
        times[i] = td.dayCounter.yearFraction(asof, asof + td.priceTenors[i]);
        prices[i] = Handle<Quote>(td.priceQuotes[i]);
    }
    QuantLib::ext::shared_ptr<PriceTermStructure> priceCurve =
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear> >(td.priceTenors, prices, td.dayCounter, td.currency);

    // Check construction of adapted price curve passes
    BOOST_REQUIRE_NO_THROW(PriceTermStructureAdapter(priceCurve, zeroCurve));

    // Construct adapted price curve
    PriceTermStructureAdapter adaptedPriceCurve(priceCurve, zeroCurve);

    // Asking for zero at time ~ 1.0 should not throw
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(1.0, Continuous));
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(asof + 1 * Years, td.dayCounter, Continuous));

    // Asking for zero at time ~ 4.0 should throw because > 3.0, max time for discount curve
    // Note: max time for the adapted price curve is the min of the max times for the underlying curves.
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(4.0, Continuous), Error);
    BOOST_CHECK_THROW(adaptedPriceCurve.zeroRate(asof + 4 * Years, td.dayCounter, Continuous), Error);

    // Allow extrapolation on adapted price curve, expect no errors
    adaptedPriceCurve.enableExtrapolation();
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(6.0, Continuous));
    BOOST_CHECK_NO_THROW(adaptedPriceCurve.zeroRate(asof + 6 * Years, td.dayCounter, Continuous));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
