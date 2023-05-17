/*
  Copyright (C) 2017 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

class SimmIsdaUnitTests {
public:
    static void test1_3();
    static void test1_3_38();
    static void test2_0();
    static void test2_1();
    static void test2_2();
    static void test2_3();
    static void test2_3_8();
    static void test2_5();
    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
