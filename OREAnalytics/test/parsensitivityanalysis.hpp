/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.
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
