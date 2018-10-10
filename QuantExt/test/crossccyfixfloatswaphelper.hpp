/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file test/crossccyfixfloatswaphelper.hpp
    \brief Cross currency fix float swap helper test
*/

#ifndef quantext_test_cross_ccy_fix_float_swap_helper_hpp
#define quantext_test_cross_ccy_fix_float_swap_helper_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Cross currency fix float swap helper tests
class CrossCurrencyFixFloatSwapHelperTest {
public:
    // Test bootstrap works
    static void testBootstrap();
    // Test changing spot FX changes curve
    static void testSpotFxChange();
    // Test changing helper floating spread
    static void testSpreadChange();
    // Test moving the evaluation date
    static void testMovingEvaluationDate();

    static boost::unit_test_framework::test_suite* suite();
};
}

#endif
