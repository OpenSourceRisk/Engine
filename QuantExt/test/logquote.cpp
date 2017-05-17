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

#include "logquote.hpp"
#include <ql/quotes/simplequote.hpp>
#include <qle/quotes/logquote.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using QuantExt::LogQuote;

namespace testsuite {

void LogQuoteTest::testLogQuote() {
    BOOST_TEST_MESSAGE("Testing QuantExt::LogQuote...");

    boost::shared_ptr<SimpleQuote> quote(new QuantLib::SimpleQuote(1.0));
    Handle<Quote> qh(quote);
    Handle<Quote> logQuote(boost::shared_ptr<Quote>(new LogQuote(qh)));

    BOOST_CHECK(logQuote->value() == std::log(quote->value()));

    quote->setValue(2.0);
    BOOST_CHECK(logQuote->value() == std::log(quote->value()));

    quote->setValue(3.0);
    BOOST_CHECK(logQuote->value() == std::log(quote->value()));

    quote->setValue(123.0);
    BOOST_CHECK(logQuote->value() == std::log(quote->value()));

    // LogQuote should throw when a negative value is set
    BOOST_CHECK_THROW(quote->setValue(-1.0), std::exception);
}

test_suite* LogQuoteTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("LogQuoteTests");
    suite->add(BOOST_TEST_CASE(&LogQuoteTest::testLogQuote));
    return suite;
}
} // namespace testsuite
