/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include "xassetmodel.hpp"
#include "utilities.hpp"

#include <qle/models/all.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/math/array.hpp>
#include <ql/math/comparison.hpp>

using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

namespace {
void check(std::string s, const Real x, const Real y, const Real e,
           const Size n = 42) {
    if (!close_enough(y, e, n)) {
        BOOST_ERROR("failed to verify " << s << "(" << x << ") = " << e
                                        << ", it is " << y << " instead");
    }
};
} // anonymous namespace

void XAssetModelTest::testParametrizations() {

    BOOST_TEST_MESSAGE("Testing XAssetModel parametrizations...");

    // base class

    Parametrization p1((EURCurrency()));
    if (p1.parameterSize(42) != 0) {
        BOOST_ERROR("empty parametrization should have zero parameter "
                    "size, it is "
                    << p1.parameterSize(42) << " instead");
    }
    if (p1.parameterTimes(42) != Array()) {
        BOOST_ERROR("empty parametrization should have empty times "
                    "array, is has size "
                    << p1.parameterTimes(42).size() << " though");
    }

    // piecewise constant helpers

    Array noTimes, three(1, 3.0), zero(1, 0.0);
    PiecewiseConstantHelper1 helper11(noTimes, three);
    check("helper11.y", 0.0, helper11.y(0.0), 3.0);
    check("helper11.y", 1.0, helper11.y(1.0), 3.0);
    check("helper11.y", 3.0, helper11.y(3.0), 3.0);
    check("helper11.int_y_sqr", 0.0, helper11.int_y_sqr(0.0), 0.0);
    check("helper11.int_y_sqr", 1.0, helper11.int_y_sqr(1.0), 9.0);
    check("helper11.int_y_sqr", 3.0, helper11.int_y_sqr(3.0), 27.0);

    PiecewiseConstantHelper2 helper21(noTimes, three);
    check("helper21.y", 0.0, helper21.y(0.0), 3.0);
    check("helper21.y", 1.0, helper21.y(1.0), 3.0);
    check("helper21.y", 3.0, helper21.y(3.0), 3.0);
    check("helper21.exp_m_int_y", 0.0, helper21.exp_m_int_y(0.0), 1.0);
    check("helper21.exp_m_int_y", 1.0, helper21.exp_m_int_y(1.0),
          std::exp(-3.0));
    check("helper21.exp_m_int_y", 3.0, helper21.exp_m_int_y(3.0),
          std::exp(-9.0));
    check("helper21.int_exp_m_int_y", 0.0, helper21.int_exp_m_int_y(0.0), 0.0);
    check("helper21.int_exp_m_int_y", 1.0, helper21.int_exp_m_int_y(1.0),
          (1.0 - std::exp(-3.0)) / 3.0);
    check("helper21.int_exp_m_int_y", 3.0, helper21.int_exp_m_int_y(3.0),
          (1.0 - std::exp(-9.0)) / 3.0);

    PiecewiseConstantHelper2 helper31(noTimes, zero);
    check("helper31.y", 0.0, helper31.y(0.0), 0.0);
    check("helper31.y", 1.0, helper31.y(1.0), 0.0);
    check("helper31.y", 3.0, helper31.y(3.0), 0.0);
    check("helper31.exp_m_int_y", 0.0, helper31.exp_m_int_y(0.0), 1.0);
    check("helper31.exp_m_int_y", 1.0, helper31.exp_m_int_y(1.0), 1.0);
    check("helper31.exp_m_int_y", 3.0, helper31.exp_m_int_y(3.0), 1.0);
    check("helper31.int_exp_m_int_y", 0.0, helper31.int_exp_m_int_y(0.0), 0.0);
    check("helper31.int_exp_m_int_y", 1.0, helper31.int_exp_m_int_y(1.0), 1.0);
    check("helper31.int_exp_m_int_y", 3.0, helper31.int_exp_m_int_y(3.0), 3.0);
}

test_suite *XAssetModelTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("XAsset model tests");
    suite->add(QUANTEXT_TEST_CASE(&XAssetModelTest::testParametrizations));
    return suite;
}
