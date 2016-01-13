/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/currency.hpp
  \brief Currency tests
*/

#ifndef quantext_test_currency_hpp
#define quantext_test_currency_hpp

#include <boost/test/unit_test.hpp>

//! Currency tests
/*!
  \ingroup tests
*/
class CurrencyTest {
  public:
    static void testCurrency();
    static boost::unit_test_framework::test_suite* suite();
};

#endif
