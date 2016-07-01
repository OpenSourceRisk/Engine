/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/staticallycorrectedyieldtermstructure.hpp
    \brief tests for StaticallyCorrectedYieldTermStructure
*/

#ifndef quantext_test_staticallycorrectedyieldtermstructure_hpp
#define quantext_test_staticallycorrectedyieldtermstructure_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {
    
//! DynamicBlackVolTermStructure tests
class StaticallyCorrectedYieldTermStructureTest {
  public:
    static void testCorrectedYts();
    static boost::unit_test_framework::test_suite* suite();
};

}

#endif
