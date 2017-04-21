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

/*! \file test/equitycrossassetmodel.hpp
    \brief cross asset model - equity components test
*/

#ifndef quantext_test_equitycrossassetmodel_hpp
#define quantext_test_equitycrossassetmodel_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! EquityCrossAssetModel tests
class EquityCrossAssetModelTest {
public:
    /*! In a EUR-USD CrossAssetModel with two equities, test a Monte Carlo pricing of an equiy forward under the base
     *  currency numeraire against the analytical expectation. Perform similar checks for an equity option. */
    static void testEqLgm5fPayouts();

    /*! Test the calibration of all components of a full CrossAssetModel (3 IR LGM models and 2 FX Black Scholes
     * models and 2 equities) by comparing the model prices and market prices of the calibration instruments. */
    static void testLgm5fEqCalibration();

    /*! Compare the analytical (unconditional) expectation and covariance matrix of the 7 dimensional stochastic process
     * of a CrossAssetModel at t=10 against Monte Carlo estimates using both an exact and an Euler
     * discretisation. Special attention paid to the equity components of the process. */
    static void testLgm5fMoments();

    static boost::unit_test_framework::test_suite* suite();
};
}

#endif
