/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/logquote.hpp
  \brief LogQuote test
*/

#ifndef quantext_test_logquote_hpp
#define quantext_test_logquote_hpp

#include <boost/test/unit_test.hpp>

//! LogQuotes tests
/*!
  Tests the LogQuote class by Comparing LogQuote values with the logs of Quote values
  \ingroup tests
*/
class LogQuoteTest {
  public:
    //! test LogQuotes
    static void testLogQuote();
    static boost::unit_test_framework::test_suite* suite();
};

#endif
