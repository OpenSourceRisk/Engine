/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file test/stabilisedglls.hpp
    \brief stabilised glls tests
*/

#ifndef quantext_test_glls_hpp
#define quantext_test_glls_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! stabilised glls tests
class StabilisedGLLSTest {
public:
    /*! GeneralLinearLeastSquares fails to produce resonable regression coefficients on the
      (untransformed) data used in this test case.  */
    static void testBigInputNumbers();

    /*! Test 2D regression on random data against GeneralLinearLeastSquares */
    static void test2DRegression();

    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite

#endif
