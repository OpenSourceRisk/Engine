/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file test/cpicapfloor.hpp
    \brief cpi cap/floor tests, in addition to QuantLib's tests
*/

#ifndef quantext_cpicapfloor_test_hpp
#define quantext_cpicapfloor_test_hpp

// Boost.Test
#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Tests cpi cap/floor volatitlity bootstrap and pricing
/*!
  \todo Check why the parity test yields a ~bp upfront error in some cases (high and low strikes)
 */
class CPICapFloorTest {
public:
    //! tests CPI volatility surface stripping
    static void volatilitySurface();
    //! tests put call parity at the money, in and out of the money across strikes and maturities
    static void putCallParity();

    static boost::unit_test_framework::test_suite* suite();
};

} // namespace testsuite

#endif
