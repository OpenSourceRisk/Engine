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

/*! \file test/crossassetmodel2.hpp
    \brief cross asset model test
*/

#ifndef quantext_test_crossassetmodel2_hpp
#define quantext_test_crossassetmodel2_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! CrossAssetModel tests, part 2
class CrossAssetModelTest2 {
public:
    /*! Test if a 31 factor IR-FX CrossAssetModel has positive semidefinite analytical and Euler covariance matrices on
     * a typical simulation grid. */
    static void testLgm31fPositiveCovariance();

    /*! Compare the analytical (unconditional) expectation and covariance matrix of the stochastic process of a 31
     * factor IR-FX CrossAssetModel at t=10 against Monte Carlo estimates using both an exact and an Euler
     * discretisation. */
    static void testLgm31fMoments();

    /*! Test whether the price process of a constant cashflow in all possible currencies under the domestic numeraire in
     * a 31 factor IR-FX CrossAssetModel is a martingale. */
    static void testLgm31fMartingaleProperty();

    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite

#endif
