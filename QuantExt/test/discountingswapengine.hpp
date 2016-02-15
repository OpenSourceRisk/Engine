/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/discountingswapengine.hpp
  \brief discounting swap engine with simulated fixings test
*/

#ifndef quantext_test_discountingswapengine_hpp
#define quantext_test_discountingswapengine_hpp

#include <boost/test/unit_test.hpp>

//! DiscountingSwapEngine tests
/*!
  \ingroup tests
*/
class DiscountingSwapEngineTest {
  public:
    static void testVanillaSwap();
    static boost::unit_test_framework::test_suite* suite();
};

#endif
