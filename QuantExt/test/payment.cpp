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

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/instruments/payment.hpp>
#include <qle/pricingengines/paymentdiscountingengine.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PaymentTest)

BOOST_AUTO_TEST_CASE(testDomesicPayment) {

    BOOST_TEST_MESSAGE("Testing Domestic Payment NPV...");

    SavedSettings backup;

    Date refDate = Date(8, Dec, 2016);
    Settings::instance().evaluationDate() = refDate;
    Date paymentDate = refDate + 10 * Years;
    Payment payment(100.0, EURCurrency(), paymentDate);
    BOOST_CHECK_EQUAL(payment.cashFlow()->amount(), 100.0);
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(0, TARGET(), 0.03, ActualActual(ActualActual::ISDA)));
    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<PaymentDiscountingEngine>(yts);
    payment.setPricingEngine(engine);

    Real expectedNpv = 100.0 * yts->discount(paymentDate);

    BOOST_CHECK_SMALL(payment.NPV() - expectedNpv, 0.000001);
}

BOOST_AUTO_TEST_CASE(testForeignPayment) {

    BOOST_TEST_MESSAGE("Testing Foreign Payment NPV...");

    SavedSettings backup;

    Date refDate = Date(8, Dec, 2016);
    Settings::instance().evaluationDate() = refDate;
    Date paymentDate = refDate + 10 * Years;
    Payment payment(100.0, EURCurrency(), paymentDate);
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(0, TARGET(), 0.03, ActualActual(ActualActual::ISDA)));
    Handle<Quote> fx(QuantLib::ext::make_shared<SimpleQuote>(0.789));
    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<PaymentDiscountingEngine>(yts, fx);
    payment.setPricingEngine(engine);

    Real expectedNpv = 100.0 * yts->discount(paymentDate) * fx->value();

    BOOST_CHECK_SMALL(payment.NPV() - expectedNpv, 0.000001);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
