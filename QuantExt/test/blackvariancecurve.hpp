/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/blackvariancecurve.hpp
  \brief interpolated black variance curve test
*/

#ifndef quantext_test_blackvariancecurve_hpp
#define quantext_test_blackvariancecurve_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {
    
//! BlackVarianceCurve tests
/*!
  \ingroup tests
*/
class BlackVarianceCurveTest {
  public:
    static void testBlackVarianceCurve();
    static boost::unit_test_framework::test_suite* suite();
};

}

#endif
