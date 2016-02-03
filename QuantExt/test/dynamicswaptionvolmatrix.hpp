/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file dynamicswaptionvolmatrix.hpp
    \brief tests for DynamicSwaptionVolStructure
*/

#ifndef quantext_test_dynamicswaptionvolmatrix_hpp
#define quantext_test_dynamicswaptionvolmatrix_hpp

#include <boost/test/unit_test.hpp>

//! DynamicSwaptionVolMatrix tests
class DynamicSwaptionVolMatrixTest {
  public:
    static void testConstantVariance();
    static void testForwardForwardVariance();
    static boost::unit_test_framework::test_suite* suite();
};

#endif
