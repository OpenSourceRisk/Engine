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

/*! \file testsuite.cpp
    \brief wrapper calling all individual test cases
    \ingroup
*/

#include <iomanip>
#include <iostream>

#include <oret/config.hpp>

// Boost.Test
#define BOOST_TEST_MODULE "QuantExtTestSuite"
#ifdef ORE_ENABLE_PARALLEL_UNIT_TEST_RUNNER
#include <test-suite/paralleltestrunner.hpp>
#else
#include <boost/test/included/unit_test.hpp>
#endif

// Boost
using namespace boost;
using boost::unit_test::test_suite;
using boost::unit_test::framework::master_test_suite;

#include "toplevelfixture.hpp"

#ifdef BOOST_MSVC
#include <ql/auto_link.hpp>
#include <qle/auto_link.hpp>
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_timer
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_chrono
#include <boost/config/auto_link.hpp>
#endif

