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

/*! \file test/discountratiomodifiedcurve.hpp
    \brief Testing discount ratio modified curve
*/

#ifndef quantext_test_discount_ratio_modified_curve_hpp
#define quantext_test_discount_ratio_modified_curve_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Discount ratio modified curve tests
class DiscountingRatioModifiedCurveTest {
public:
    //! Test results with some standard curve setups
    static void testStandardCurves();
    //! Test extrapolation settings
    static void testExtrapolationSettings();
    //! Test construction with null underlying curves throw
    static void testConstructionNullUnderlyingCurvesThrow();
    //! Test linking with null underlying curves throw
    static void testLinkingNullUnderlyingCurvesThrow();

    static boost::unit_test_framework::test_suite* suite();
};
}

#endif
