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

/*! \file test/crossassetmodel.hpp
    \brief cross asset model test
*/

#ifndef quantext_test_crossassetmodel_hpp
#define quantext_test_crossassetmodel_hpp

#include <boost/test/unit_test.hpp>

namespace testsuite {

//! CrossAssetModel tests
class CrossAssetModelTest {
public:
    /*! Test whether the prices of a bermudan swaption are consistent in the LGM model with the
     * NumericLgmSwaptionEngine, in the LGM model with the Gaussian1dSwaptionEngine and in the GSR model with the
     * Gaussian1dSwaptionEngine. */
    static void testBermudanLgm1fGsr();

    /*! Test if the price of a bermudan swaption is invariant under the LGM model invariances (scaling and shifting) in
     * the LGM model with the NumericLgmSwaptionEngine and in the LGM model with the Gaussian1dSwaptionEngine. */
    static void testBermudanLgmInvariances();

    /*! Test the non-standard Bermudan swaption engine against the standard engine */
    static void testNonstandardBermudanSwaption();

    /*! Calibrate the LGM and the GSR model to a coterminal swaption basket and compare the calibrated model parameters.
     * Perform the same calibration in the LGM model as a component of the CrossAssetModel and check whether the results
     * are the same and if other components are not affected by this calibration. */
    static void testLgm1fCalibration();

    /*! In a EUR-USD CrossAssetModel, test a Monte Carlo pricing of an USD cashflow under the EUR numeraire against the
     * analytical expectation. Perform similar checks for an USD zero bond under the EUR numeraire and an USD-EUR FX
     * option. */
    static void testCcyLgm3fForeignPayouts();

    /*! Compare the calibration of FX components in a EUR-USD model to those in a EUR-USD-GBP model, i.e. if the
     * calibration results in the smaller model stay the same when embed this model into a model with more components.
     */
    static void testLgm5fFxCalibration();

    /*! Test the calibration of all components of a EUR-USD-GBP CrossAssetModel (3 IR LGM models and 2 FX Black Scholes
     * models) by comparing the model prices and market prices of the calibration instruments. */
    static void testLgm5fFullCalibration();

    /*! Compare the analytical (unconditional) expectation and covariance matrix of the 5 dimensional stochastic process
     * of a EUR-USD-GBP CrossAssetModel at t=10 against Monte Carlo estimates using both an exact and an Euler
     * discretisation. */
    static void testLgm5fMoments();

    /*! Test whether the GSR model (i.e. the Hull White model under the T forward measure) is equivalent to the LGM
     * model with shift T in the sense that they generate identical paths up to numerical accuracy. */
    static void testLgmGsrEquivalence();

    /*! Test the pricing of a single cashflow at T=50 in a LGM model with Monte Carlo using different LGM shifts and
     * check whether the error estimates go to zero when the shift approaches T=50. */
    static void testLgmMcWithShift();

    /*! Test martingale properties analogous to the test above, but with dynamic credit compenent. */
    static void testIrFxCrMartingaleProperty();

    /*! Test moments analogous to the test above, but with dynamic credit compenent. */
    static void testIrFxCrMoments();

    /*! Test martingale properties analogous to the test above, but with dynamic inflation and credit compenent. */
    static void testIrFxInfCrMartingaleProperty();

    /*! Test moments analogous to the test above, but with dynamic inflation and credit compenent. */
    static void testIrFxInfCrMoments();

    /*! Test martingale properties analogous to the test above, but with dynamic inflation, credit, equity compenent. */
    static void testIrFxInfCrEqMartingaleProperty();

    /*! Test moments analogous to the test above, but with dynamic inflation, credit, equity compenent. */
    static void testIrFxInfCrEqMoments();

    /*! Test DK component calibration in alpha against MC full repricing of calibration instruments. */
    static void testCpiCalibrationByAlpha();

    /*! Test DK component calibration in H against MC full repricing of calibration instruments. */
    static void testCpiCalibrationByH();

    /*! Test CR component calibration against MC full repricing of calibration instruments. */
    static void testCrCalibration();

    /*! In a EUR-USD CrossAssetModel with two equities, test a Monte Carlo pricing of an equiy forward under the base
     *  currency numeraire against the analytical expectation. Perform similar checks for an equity option. */
    static void testEqLgm5fPayouts();

    /*! Test the calibration of all components of a full CrossAssetModel (3 IR LGM models and 2 FX Black Scholes
     * models and 2 equities) by comparing the model prices and market prices of the calibration instruments. */
    static void testEqLgm5fCalibration();

    /*! Compare the analytical (unconditional) expectation and covariance matrix of the 7 dimensional stochastic process
     * of a CrossAssetModel at t=10 against Monte Carlo estimates using both an exact and an Euler
     * discretisation. Special attention paid to the equity components of the process. */
    static void testEqLgm5fMoments();

    /*! Test whether the input correlation matrix for a CrossAssetModel with 1 up to 100 currencies is recovered as the
     * analytical and Euler correlation matrix estimated over a small time step dt. */
    static void testCorrelationRecovery();

    /*! Test correlation recovery analogous to the test above, but with dynamic credit compenent. */
    static void testIrFxCrCorrelationRecovery();

    /*! Test correlation recovery analogous to the test above, but with dynamic inflation and credit compenent. */
    static void testIrFxInfCrCorrelationRecovery();

    /*! Test correlation recovery analogous to the test above, but with dynamic inflation, credit, equity compenent. */
    static void testIrFxInfCrEqCorrelationRecovery();

    static boost::unit_test_framework::test_suite* suite();
};
} // namespace testsuite

#endif
