/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file test/sacva.hpp
    \brief SA-CVA tests
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! SA-CCR tests
/*! Compare the results of SA-CCR calculations against cached results

    \ingroup tests

*/
class SaCvaTest {
public:

    static void testSACVA_RiskFactorCorrelation();
    
    static void testSACVA_BucketCorrelation();
    
    static void testSACVA_RiskWeight();
    
    static void testSACVA_FxDeltaCalc();
    static void testSACVA_FxVegaCalc();
    static void testSACVA_IrVegaCalc();
    static void testSACVA_IrDeltaCalc();

    static boost::unit_test_framework::test_suite* suite();
    
};
} // namespace testsuite
