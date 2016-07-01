/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/dynamicblackvoltermstructure.hpp
    \brief tests for DynamicBlackVolTermStructure
*/

#ifndef quantext_test_dynamicblackvoltermstructure_hpp
#define quantext_test_dynamicblackvoltermstructure_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {
    
//! DynamicBlackVolTermStructure tests
class DynamicBlackVolTermStructureTest {
  public:
    static void testConstantVarianceStickyStrike();
    static void testConstantVarianceStickyLogMoneyness();
    static void testForwardVarianceStickyStrike();
    static void testForwardVarianceStickyLogMoneyness();
    static boost::unit_test_framework::test_suite* suite();
};

}

#endif
