
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

/*! \file test/analyticlgmswaptionengine.hpp
    \brief analytic LGM swaption engine test
*/

#ifndef quantext_test_analyticlgmswaptionengine_hpp
#define quantext_test_analyticlgmswaptionengine_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! AnalyticLgmSwaptionEngine tests
class AnalyticLgmSwaptionEngineTest {
public:
    /*! Test if the fixed coupon adjustment in the analytic LGM swaption engine accounting for a dual curve setup is
     * zero in a mono curve setup. */
    static void testMonoCurve();

    /*! Test if the fixed coupon adjustment in the analytic LGM swaption engine accounting for a dual curve setup is
     * within a range of expected values for a given spread in a dual curve setup. */
    static void testDualCurve();

    /*! Test the result of the analytic LGM swaption engine against those of the Gaussian1dSwaptionEngine and
     * FdHullWhiteSwaptionEngine. */
    static void testAgainstOtherEngines();

    /*! Test whether the price of an european swaption in the analytic LGM swaption engine is invariant under the LGM
     * model invariances (scaling and shifting). */
    static void testLgmInvariances();

    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite

#endif
