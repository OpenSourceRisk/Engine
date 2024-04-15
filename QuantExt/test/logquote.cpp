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
#include <ql/quotes/simplequote.hpp>
#include <qle/quotes/logquote.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using QuantExt::LogQuote;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(LogQuoteTest)

BOOST_AUTO_TEST_CASE(testLogQuote) {

    BOOST_TEST_MESSAGE("Testing QuantExt::LogQuote...");
    QuantLib::ext::shared_ptr<SimpleQuote> quote(new QuantLib::SimpleQuote(1.0));
    Handle<Quote> qh(quote);
    Handle<Quote> logQuote(QuantLib::ext::shared_ptr<Quote>(new LogQuote(qh)));

    BOOST_CHECK_EQUAL(logQuote->value(), std::log(quote->value()));

    quote->setValue(2.0);
    BOOST_CHECK_EQUAL(logQuote->value(), std::log(quote->value()));

    quote->setValue(3.0);
    BOOST_CHECK_EQUAL(logQuote->value(), std::log(quote->value()));

    quote->setValue(123.0);
    BOOST_CHECK_EQUAL(logQuote->value(), std::log(quote->value()));

    // LogQuote should throw when a negative value is set
    BOOST_CHECK_THROW(quote->setValue(-1.0), std::exception);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
