/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include "logquote.hpp"
#include <qle/quotes/logquote.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;
using QuantExt::LogQuote;

void LogQuoteTest::testLogQuote() {
    BOOST_MESSAGE("Testing QuantExt::LogQuote...");

    boost::shared_ptr<SimpleQuote> quote (new QuantLib::SimpleQuote(1.0));
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
