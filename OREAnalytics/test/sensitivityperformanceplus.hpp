/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file test/sensitivityperformanceplus.hpp
    \brief Extended sensitivity preformance test
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Sensitivity Performance tests
/*!
  This is a performance test for the zero rate sensitivities generation
  in ORE+. It is almost identical to the test in ORE, but it instead makes
  use of oreplus::sensitivity::SensitivityAnalysis (which uses a DeltaScenarioFactory)
  \ingroup tests
*/
class SensitivityPerformancePlusTest {
public:
    //! Test performance of sensitivities run ("None" observation mode)
    static void testSensiPerformanceNoneObs();
    //! Test performance of sensitivities run ("Disable" observation mode)
    static void testSensiPerformanceDisableObs();
    //! Test performance of sensitivities run ("Defer" observation mode)
    static void testSensiPerformanceDeferObs();
    //! Test performance of sensitivities run ("Unregister" observation mode)
    static void testSensiPerformanceUnregisterObs();

    //! Test performance of sensitivities run (including cross-gammas) ("None" observation mode)
    static void testSensiPerformanceCrossGammaNoneObs();
    //! Test performance of sensitivities run (including cross-gammas) ("Disable" observation mode)
    static void testSensiPerformanceCrossGammaDisableObs();
    //! Test performance of sensitivities run (including cross-gammas) ("Defer" observation mode)
    static void testSensiPerformanceCrossGammaDeferObs();
    //! Test performance of sensitivities run (including cross-gammas) ("Unregister" observation mode)
    static void testSensiPerformanceCrossGammaUnregisterObs();

    //! Test performance of sensitivities run with lots of buckets  ("None" observation mode)
    static void testSensiPerformanceBigScenarioNoneObs();
    //! Test performance of sensitivities run with lots of buckets  ("Disable" observation mode)
    static void testSensiPerformanceBigScenarioDisableObs();
    //! Test performance of sensitivities run with lots of buckets  ("Defer" observation mode)
    static void testSensiPerformanceBigScenarioDeferObs();
    //! Test performance of sensitivities run with lots of buckets  ("Unregister" observation mode)
    static void testSensiPerformanceBigScenarioUnregisterObs();

    //! Test performance of sensitivities run for a large portfolio ("None" observation mode)
    static void testSensiPerformanceBigPortfolioNoneObs();
    //! Test performance of sensitivities run for a large portfolio ("Disable" observation mode)
    static void testSensiPerformanceBigPortfolioDisableObs();
    //! Test performance of sensitivities run for a large portfolio ("Defer" observation mode)
    static void testSensiPerformanceBigPortfolioDeferObs();
    //! Test performance of sensitivities run for a large portfolio ("Unregister" observation mode)
    static void testSensiPerformanceBigPortfolioUnregisterObs();

    //! Test performance of sensitivities run for a large portfolio, with lots of buckets  ("None" observation mode)
    static void testSensiPerformanceBigPortfolioBigScenarioNoneObs();
    //! Test performance of sensitivities run for a large portfolio, with lots of buckets  ("Disable" observation mode)
    static void testSensiPerformanceBigPortfolioBigScenarioDisableObs();
    //! Test performance of sensitivities run for a large portfolio, with lots of buckets  ("Defer" observation mode)
    static void testSensiPerformanceBigPortfolioBigScenarioDeferObs();
    //! Test performance of sensitivities run for a large portfolio, with lots of buckets  ("Unregister" observation
    // mode)
    static void testSensiPerformanceBigPortfolioBigScenarioUnregisterObs();

    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio  ("None" observation mode)
    static void testSensiPerformanceBigPortfolioCrossGammaNoneObs();
    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio  ("Disable" observation
    // mode)
    static void testSensiPerformanceBigPortfolioCrossGammaDisableObs();
    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio  ("Defer" observation mode)
    static void testSensiPerformanceBigPortfolioCrossGammaDeferObs();
    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio  ("Unregister" observation
    // mode)
    static void testSensiPerformanceBigPortfolioCrossGammaUnregisterObs();

    //! Test performance of sensitivities run (including cross-gammas) for a single-trade portfolio, with lots of
    // buckets  ("None" observation mode)
    static void testSensiPerformanceBigScenarioCrossGammaNoneObs();
    //! Test performance of sensitivities run (including cross-gammas) for a single-trade portfolio, with lots of
    // buckets  ("Disable" observation mode)
    static void testSensiPerformanceBigScenarioCrossGammaDisableObs();
    //! Test performance of sensitivities run (including cross-gammas) for a single-trade portfolio, with lots of
    // buckets  ("Defer" observation mode)
    static void testSensiPerformanceBigScenarioCrossGammaDeferObs();
    //! Test performance of sensitivities run (including cross-gammas) for a single-trade portfolio, with lots of
    // buckets  ("Unregister" observation mode)
    static void testSensiPerformanceBigScenarioCrossGammaUnregisterObs();

    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio, with lots of buckets
    //("None" observation mode)
    static void testSensiPerformanceBigPortfolioBigScenarioCrossGammaNoneObs();
    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio, with lots of buckets
    //("Disable" observation mode)
    static void testSensiPerformanceBigPortfolioBigScenarioCrossGammaDisableObs();
    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio, with lots of buckets
    //("Defer" observation mode)
    static void testSensiPerformanceBigPortfolioBigScenarioCrossGammaDeferObs();
    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio, with lots of buckets
    //("Unregister" observation mode)
    static void testSensiPerformanceBigPortfolioBigScenarioCrossGammaUnregisterObs();
    //! Test performance of sensitivities run (including cross-gammas) for a large portfolio, using the BackTest
    //! sensitivity setup
    //("None" observation mode)
    static void testSensiPerformanceBTSetupNoneObs();
    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite
