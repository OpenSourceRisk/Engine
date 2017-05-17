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

/*! \file test/sensitivityanalysis.hpp
    \brief Sensitivity analysis tests
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Sensitivity analysis tests
/*!
  \ingroup tests
*/
class SensitivityAnalysisTest {
public:
    //! Test that shifting all shift curve tenor points by DELTA yields DELTA shifts at all tenor points of the an
    // underlying curve
    static void test1dShifts();
    //! Test that shifting all 2-d shift tenor points by DELTA yields DELTA shifts at all 2-d grid points of the
    // underlying data
    static void test2dShifts();
    //! Test that the portfolio shows the expected sensitivity (regression test - "None" observation mode)
    static void testPortfolioSensitivityNoneObs();
    //! Test that the portfolio shows the expected sensitivity (regression test - "Disable" observation mode)
    static void testPortfolioSensitivityDisableObs();
    //! Test that the portfolio shows the expected sensitivity (regression test - "Defer" observation mode)
    static void testPortfolioSensitivityDeferObs();
    //! Test that the portfolio shows the expected sensitivity (regression test - "Unregister" observation mode)
    static void testPortfolioSensitivityUnregisterObs();
    //! Test computed delta/gamma/vega values for an FX option against analytic results
    static void testFxOptionDeltaGamma();
    //! Test computed cross gamma against archived values (smoke test only - no external benchmark)
    static void testCrossGamma();
    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
