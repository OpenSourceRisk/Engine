/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file test/swapperformance.hpp
    \brief Swap Exposure preformance (1K swaps, 10K samples, 60 dates);
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Swap Exposure Performance tests
/*!
  \ingroup tests
*/
class SwapPerformanceTest {
public:
    //! Test performance of simulating a single 20Y swap, 80 quatertly time steps, 1000 samples
    static void testSingleSwapPerformance();
    //! Test performance of a portfolio of 100 swaps with 5 currencies and maturities between 2 and 30 years, 80
    // quatertly time steps, 1000 samples
    static void testSwapPerformance();
    static boost::unit_test_framework::test_suite* suite();
};
}
