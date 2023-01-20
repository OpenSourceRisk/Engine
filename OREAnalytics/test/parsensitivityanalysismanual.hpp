/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file test/parsensitivityanalysismanual.hpp
    \brief More par Sensitivity analysis tests
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Par sensitivity analysis comparison against manual bump
/*! \ingroup tests
 */
class ParSensitivityAnalysisManualTest {
public:
    //! Benchmark par conversion against brute-force bump on the par instruments ("None" observation mode)
    static void testParSwapBenchmark();
    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
