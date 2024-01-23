/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/math/randomvariablelsmbasissystem.hpp>

#include <ql/time/date.hpp>

#include <boost/test/unit_test.hpp>

using namespace QuantExt;

BOOST_AUTO_TEST_SUITE(QuantExtTestSuite)

BOOST_AUTO_TEST_SUITE(RandomVariableLsmBasisSystemTest)

BOOST_AUTO_TEST_CASE(testBasisSystem1d) {

    BOOST_TEST_MESSAGE("Testing lsm basis system for random variables...");

    constexpr double x0 = -2.0;
    for (Size order = 0; order < 200; ++order) {
        auto bs =
            RandomVariableLsmBasisSystem::pathBasisSystem(order, QuantLib::LsmBasisSystem::PolynomialType::Monomial);
        BOOST_REQUIRE(bs.size() == order + 1);
        for (Size i = 0; i <= order; ++i) {
            BOOST_CHECK_CLOSE(bs[i](RandomVariable(1, x0)).at(0), std::pow(x0, i), 1E-10);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
