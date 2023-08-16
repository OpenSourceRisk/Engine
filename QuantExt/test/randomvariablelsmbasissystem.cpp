/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
*/

// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on

#include <orepbase/qle/math/randomvariablelsmbasissystem.hpp>

#include <oret/toplevelfixture.hpp>

#include <ql/time/date.hpp>

#include <iostream>
#include <iomanip>

using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(OREPlusTestSuite, ore::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(RandomVariableLsmBasisSystemTest)

BOOST_AUTO_TEST_CASE(testBasisSystem1d) {

    BOOST_TEST_MESSAGE("Testing lsm basis system for random variables...");

    constexpr double x0 = -2.0;
    for (Size order = 0; order < 200; ++order) {
        auto bs = RandomVariableLsmBasisSystem::pathBasisSystem(order);
        BOOST_REQUIRE(bs.size() == order + 1);
        for (Size i = 0; i <= order; ++i) {
            BOOST_CHECK_CLOSE(bs[i](RandomVariable(1, x0)).at(0), std::pow(x0, i), 1E-10);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
