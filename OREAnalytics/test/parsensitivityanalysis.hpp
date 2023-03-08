/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file test/parsensitivityanalysis.hpp
    \brief Par Sensitivity analysis tests
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Sensitivity analysis tests
/*!
  \ingroup tests
*/
class ParSensitivityAnalysisTest {
public:
    //! Test that shifting all shift curve tenor points by DELTA yields DELTA shifts at all tenor points of the an
    // underlying curve
    static void test1dZeroShifts();
    //! Test that shifting all 2-d shift tenor points by DELTA yields DELTA shifts at all 2-d grid points of the
    // underlying data
    static void test2dZeroShifts();
    //! Test that the portfolio shows the expected sensitivity (regression test)
    static void testPortfolioZeroSensitivity();
    //! Test par conversion of sensitivities ("None" observation mode)
    static void testParConversionNoneObs();
    //! Test par conversion of sensitivities ("Disable" observation mode)
    static void testParConversionDisableObs();
    //! Test par conversion of sensitivities ("Defer" observation mode)
    static void testParConversionDeferObs();
    //! Test par conversion of sensitivities ("Unregister" observation mode)
    static void testParConversionUnregisterObs();
    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
