/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file test/saccr.hpp
    \brief SA-CCR tests
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! SA-CCR tests
/*! Compare the results of SA-CCR calculations against cached results

    \ingroup tests

*/
class SaccrTest {
public:

    //! Test that trades are being divided into hedging sets as expected
    static void testSACCR_HedgingSets();
    //! Test that the current notional is being calculated as expected
    static void testSACCR_CurrentNotional();
    //! Test that delta is being calculated as expected
    static void testSACCR_Delta();
    
    static void testSACCR_FxPortfolio();

    static void testSACCR_FlippedFxOptions();
    
    static boost::unit_test_framework::test_suite* suite();
    
};
} // namespace testsuite
