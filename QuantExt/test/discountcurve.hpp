/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file test/discountcurve.hpp
  \brief interpolated discount curve test
*/

#ifndef quantext_test_discountcurve_hpp
#define quantext_test_discountcurve_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {
    
//! InterpolatedDiscountCurve tests
/*!
  \ingroup tests
*/
class DiscountCurveTest {
  public:
    static void testDiscountCurve();
    static boost::unit_test_framework::test_suite* suite();
};

}

#endif
