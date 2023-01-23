/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Test NPV cubes
/*!
  \ingroup tests
 */
class CubeTest {
public:
    //! Test reading and writing from/to a 100x100x1000 (trades, dates, samples) single precision cube
    static void testSinglePrecisionJaggedCube();
    //! Test reading and writing from/to a 100x100x1000 (trades, dates, samples) double precision cube
    static void testDoublePrecisionJaggedCube();

    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
