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

/*! \file test/scenariogenerator.hpp
    \brief scenario generation test
    \ingroup tests
*/

#pragma once

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! Test scenario generation
/*!
  \ingroup tests
*/
class ScenarioGeneratorTest {
public:
    //! Test Martingale properties for single currency LGM scenarios with Pseudo Random numbers
    static void testLgmMersenneTwister();
    //! Test Martingale properties for single currency LGM scenarios with Pseudo Random numbers and antithetic sampling
    static void testLgmMersenneTwisterAntithetic();
    //! Test Martingale properties for single currency LGM scenarios with Sobol sequences
    static void testLgmLowDiscrepancy();
    //! Test Martingale properties for single currency LGM scenarios with Sobol sequences and Brownian bridge
    static void testLgmLowDiscrepancyBrownianBridge();
    //! Test Martingale properties for cross asset IR/FX model with Pseudo Random numbers
    static void testCrossAssetMersenneTwister();
    //! Test Martingale properties for cross asset IR/FX model with Pseudo Random numbers and antithetic sampling
    static void testCrossAssetMersenneTwisterAntithetic();
    //! Test Martingale properties for cross asset IR/FX model with Sobol sequences
    static void testCrossAssetLowDiscrepancy();
    //! Test Martingale properties for cross asset IR/FX model with Sobol sequences and Brownian bridge
    static void testCrossAssetLowDiscrepancyBrownianBridge();
    //! Test Martingale properties for cross asset IR/FX model scenarios after applying them to the simulation market
    static void testCrossAssetSimMarket();
    //! Test cross asset IR/FX model scenarios after applying to simulation market pathwise to the direct model output
    static void testCrossAssetSimMarket2();
    //! Test consistency of EUR and USD swap exposure evolutions with European Swaption prices
    static void testVanillaSwapExposure();
    //! Test consistency of FX Forward Exposure evolutions with FX Option prices
    static void testFxForwardExposure();
    //! Test consistency of FX Forward Exposure evolutions with FX Forward Option prices when IR vols vanish
    static void testFxForwardExposureZeroIrVol();

    static boost::unit_test_framework::test_suite* suite();
};
}
