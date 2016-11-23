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

/*! \file test/observationmode.hpp
    \brief Test swap exposure simulation with various observation modes
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Test reference date and term structure updates for various observation modes
/*!
    Observation Modes are None, Unregister and Disable
    Each mode is tested with a simulation grid that is shorter/longer than portfolio maturity
    Each test runs with and without checks that the index fixing data is correctly stored in the aggregation scenario data object 
    \ingroup tests
*/
class ObservationModeTest {
public:
    //! Observation mode Disable, long simulation grid
    static void testDisableLong();
    //! Observation mode Disable, short simulation grid
    static void testDisableShort();
    //! Observation mode Unregister
    static void testUnregister();
    //! Observation mode Defer
    static void testDefer();
    //! Observation mode None
    static void testNone();
    static boost::unit_test_framework::test_suite* suite();
};
}
