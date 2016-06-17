/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/cashflow.hpp
  \brief Cashflow tests
*/

#ifndef quantext_test_cashflow_hpp
#define quantext_test_cashflow_hpp

#include <boost/test/unit_test.hpp>

//! CashFlow test
/*!
  \ingroup tests
*/
class CashFlowTest {
  public:
    static void testFXLinkedCashFlow();
    static boost::unit_test_framework::test_suite* suite();
};

#endif
