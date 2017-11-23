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

#include "swaptionvolconstantspread.hpp"
#include "swaptionmarketdata.hpp"
#include "yieldcurvemarketdata.hpp"

#include <qle/termstructures/swaptionvolatilityconverter.hpp>
#include <qle/termstructures/swaptionvolconstantspread.hpp>
#include <qle/termstructures/swaptionvolcube2.hpp>

#include <ql/indexes/swap/euriborswap.hpp>

#include <boost/assign/list_of.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace boost::assign;

namespace {
// Variables to be used in the test
struct CommonVars {
    // Constructor
    CommonVars(Size expiryIndex = 0, Size termIndex = 0, Real atmVolShift = 0) : referenceDate(5, Feb, 2016) {

        // Reference date
        Settings::instance().evaluationDate() = referenceDate;

        // Link ibor index to correct forward curve
        boost::shared_ptr<IborIndex> iborIndex = conventions.floatIndex->clone(yieldCurves.forward6M);
        boost::shared_ptr<SwapIndex> swapIndex =
            conventions.swapIndex->clone(yieldCurves.forward6M, yieldCurves.forward6M);
        boost::shared_ptr<SwapIndex> shortSwapIndex =
            conventions.shortSwapIndex->clone(yieldCurves.forward6M, yieldCurves.forward6M);

        // Create underlying swap conventions
        swapConventions = boost::make_shared<SwapConventions>(conventions.settlementDays, conventions.fixedTenor,
                                                              conventions.fixedCalendar, conventions.fixedConvention,
                                                              conventions.fixedDayCounter, iborIndex);

        // Set up the various swaption matrices
        Matrix shiftedAtmVols = atmVols.nVols;
        QL_REQUIRE(expiryIndex < atmVols.optionTenors.size(), "expiryIndex out of range");
        QL_REQUIRE(termIndex < atmVols.swapTenors.size(), "termIndex out of range");
        shiftedAtmVols[expiryIndex][termIndex] += atmVolShift;
        atmNormalVolMatrix = boost::make_shared<SwaptionVolatilityMatrix>(
            referenceDate, conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors,
            atmVols.swapTenors, shiftedAtmVols, Actual365Fixed(), true, Normal);

        atmLogNormalVolMatrix = boost::make_shared<SwaptionVolatilityMatrix>(
            referenceDate, conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors,
            atmVols.swapTenors, shiftedAtmVols, Actual365Fixed(), true, ShiftedLognormal);

        // boost::make_shared can only handle 9 parameters
        atmShiftedLogNormalVolMatrix_1 = boost::shared_ptr<SwaptionVolatilityMatrix>(new SwaptionVolatilityMatrix(
            referenceDate, conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors,
            atmVols.swapTenors, shiftedAtmVols, Actual365Fixed(), true, ShiftedLognormal, atmVols.shifts_1));

        atmShiftedLogNormalVolMatrix_2 = boost::shared_ptr<SwaptionVolatilityMatrix>(new SwaptionVolatilityMatrix(
            referenceDate, conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors,
            atmVols.swapTenors, atmVols.slnVols_2, Actual365Fixed(), true, ShiftedLognormal, atmVols.shifts_2));

        normalVolCube = boost::shared_ptr<SwaptionVolCube2>(new SwaptionVolCube2(
            Handle<QuantLib::SwaptionVolatilityStructure>(atmNormalVolMatrix), atmVols.optionTenors, atmVols.swapTenors,
            atmVols.strikeSpreads, atmVols.nVolSpreads, swapIndex, shortSwapIndex, false, true));
        logNormalVolCube = boost::shared_ptr<SwaptionVolCube2>(new SwaptionVolCube2(
            Handle<QuantLib::SwaptionVolatilityStructure>(atmLogNormalVolMatrix), atmVols.optionTenors,
            atmVols.swapTenors, atmVols.strikeSpreads, atmVols.lnVolSpreads, swapIndex, shortSwapIndex, false, true));
        shiftedLogNormalVolCube = boost::shared_ptr<SwaptionVolCube2>(new SwaptionVolCube2(
            Handle<QuantLib::SwaptionVolatilityStructure>(atmShiftedLogNormalVolMatrix_1), atmVols.optionTenors,
            atmVols.swapTenors, atmVols.strikeSpreads, atmVols.slnVolSpreads, swapIndex, shortSwapIndex, false, true));

        normalVolCubeConstantSpread = boost::shared_ptr<SwaptionVolatilityConstantSpread>(
            new SwaptionVolatilityConstantSpread(Handle<QuantLib::SwaptionVolatilityStructure>(atmNormalVolMatrix),
                                                 Handle<QuantLib::SwaptionVolatilityStructure>(normalVolCube)));

        logNormalVolCubeConstantSpread = boost::shared_ptr<SwaptionVolatilityConstantSpread>(
            new SwaptionVolatilityConstantSpread(Handle<QuantLib::SwaptionVolatilityStructure>(atmLogNormalVolMatrix),
                                                 Handle<QuantLib::SwaptionVolatilityStructure>(logNormalVolCube)));

        shiftedLogNormalVolCubeConstantSpread =
            boost::shared_ptr<SwaptionVolatilityConstantSpread>(new SwaptionVolatilityConstantSpread(
                Handle<QuantLib::SwaptionVolatilityStructure>(atmShiftedLogNormalVolMatrix_1),
                Handle<QuantLib::SwaptionVolatilityStructure>(shiftedLogNormalVolCube)));
    }

    // Members
    Date referenceDate;
    SwaptionConventionsEUR conventions;
    SwaptionVolatilityEUR atmVols;
    YieldCurveEUR yieldCurves;
    boost::shared_ptr<SwapConventions> swapConventions;
    boost::shared_ptr<SwaptionVolatilityStructure> atmNormalVolMatrix;
    boost::shared_ptr<SwaptionVolatilityStructure> atmLogNormalVolMatrix;
    boost::shared_ptr<SwaptionVolatilityStructure> atmShiftedLogNormalVolMatrix_1;
    boost::shared_ptr<SwaptionVolatilityStructure> atmShiftedLogNormalVolMatrix_2;
    boost::shared_ptr<SwaptionVolatilityStructure> normalVolCube, normalVolCubeConstantSpread;
    boost::shared_ptr<SwaptionVolatilityStructure> logNormalVolCube, logNormalVolCubeConstantSpread;
    boost::shared_ptr<SwaptionVolatilityStructure> shiftedLogNormalVolCube, shiftedLogNormalVolCubeConstantSpread;

    SavedSettings backup;
};
} // namespace

namespace testsuite {

void SwaptionVolConstantSpreadTest::testAtmNormalVolShiftPropagation() {
    BOOST_TEST_MESSAGE("Testing ATM normal vol shift propagation...");

    CommonVars vars1;
    Real tolerance = 0.000001;
    Real shift = 0.0050;
    for (Size i = 0; i < vars1.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars1.atmVols.swapTenors.size(); ++j) {
            CommonVars vars2(i, j, shift);
            Period expiry = vars1.atmVols.optionTenors[i];
            Period term = vars1.atmVols.swapTenors[j];

            Real atmVol = vars1.atmNormalVolMatrix->volatility(expiry, term, 0.0);
            Real shiftedAtmVol = vars2.atmNormalVolMatrix->volatility(expiry, term, 0.0);
            BOOST_CHECK_SMALL(shiftedAtmVol - atmVol - shift, tolerance);

            // check that shift propagates to all strikes for thsi expiry/term
            for (Real strike = 0.01; strike <= 0.08; strike += 0.005) {
                Real otmVol = vars1.normalVolCubeConstantSpread->volatility(expiry, term, strike);
                Real shiftedOtmVol = vars2.normalVolCubeConstantSpread->volatility(expiry, term, strike);
                // BOOST_TEST_MESSAGE(i << "/" << j << "/" << strike << " : ATM Vol " << atmVol << " OTM Vol " << otmVol
                //                      << " ATM Vol Shift " << shiftedAtmVol - atmVol << " OTM Vol Shift "
                //                      << shiftedOtmVol - otmVol);
                BOOST_CHECK_SMALL(shiftedOtmVol - otmVol - shift, tolerance);
            }

            // check that there is no shift in any other expiry/term
            for (Size ii = 0; ii < vars1.atmVols.optionTenors.size(); ++ii) {
                if (i == ii)
                    continue;
                for (Size jj = 0; jj < vars1.atmVols.swapTenors.size(); ++jj) {
                    if (j == jj)
                        continue;
                    Period expiry = vars1.atmVols.optionTenors[ii];
                    Period term = vars1.atmVols.swapTenors[jj];
                    Real atmVol = vars1.atmNormalVolMatrix->volatility(expiry, term, 0.0);
                    Real shiftedAtmVol = vars2.atmNormalVolMatrix->volatility(expiry, term, 0.0);
                    BOOST_CHECK_SMALL(shiftedAtmVol - atmVol, tolerance);
                    for (Real strike = 0.01; strike <= 0.08; strike += 0.005) {
                        Real otmVol = vars1.normalVolCubeConstantSpread->volatility(expiry, term, strike);
                        Real shiftedOtmVol = vars2.normalVolCubeConstantSpread->volatility(expiry, term, strike);
                        BOOST_CHECK_SMALL(shiftedOtmVol - otmVol, tolerance);
                    }
                }
            }
        }
    }
}

void SwaptionVolConstantSpreadTest::testAtmLogNormalVolShiftPropagation() {
    BOOST_TEST_MESSAGE("Testing ATM log-normal vol shift propagation...");

    CommonVars vars1;
    Real tolerance = 0.000001;
    Real shift = 0.1;
    for (Size i = 0; i < vars1.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars1.atmVols.swapTenors.size(); ++j) {
            CommonVars vars2(i, j, shift);
            Period expiry = vars1.atmVols.optionTenors[i];
            Period term = vars1.atmVols.swapTenors[j];

            Real atmVol = vars1.atmLogNormalVolMatrix->volatility(expiry, term, 0.0);
            Real shiftedAtmVol = vars2.atmLogNormalVolMatrix->volatility(expiry, term, 0.0);
            BOOST_CHECK_SMALL(shiftedAtmVol - atmVol - shift, tolerance);

            // check that shift propagates to all strikes for thsi expiry/term
            for (Real strike = 0.01; strike <= 0.08; strike += 0.005) {
                Real otmVol = vars1.normalVolCubeConstantSpread->volatility(expiry, term, strike);
                Real shiftedOtmVol = vars2.normalVolCubeConstantSpread->volatility(expiry, term, strike);
                // BOOST_TEST_MESSAGE(i << "/" << j << "/" << strike << " : ATM Vol " << atmVol << " OTM Vol " << otmVol
                //                      << " ATM Vol Shift " << shiftedAtmVol - atmVol << " OTM Vol Shift "
                //                      << shiftedOtmVol - otmVol);
                BOOST_CHECK_SMALL(shiftedOtmVol - otmVol - shift, tolerance);
            }

            // check that there is no shift in any other expiry/term
            for (Size ii = 0; ii < vars1.atmVols.optionTenors.size(); ++ii) {
                if (i == ii)
                    continue;
                for (Size jj = 0; jj < vars1.atmVols.swapTenors.size(); ++jj) {
                    if (j == jj)
                        continue;
                    Period expiry = vars1.atmVols.optionTenors[ii];
                    Period term = vars1.atmVols.swapTenors[jj];
                    Real atmVol = vars1.atmLogNormalVolMatrix->volatility(expiry, term, 0.0);
                    Real shiftedAtmVol = vars2.atmLogNormalVolMatrix->volatility(expiry, term, 0.0);
                    BOOST_CHECK_SMALL(shiftedAtmVol - atmVol, tolerance);
                    for (Real strike = 0.01; strike <= 0.08; strike += 0.005) {
                        Real otmVol = vars1.logNormalVolCubeConstantSpread->volatility(expiry, term, strike);
                        Real shiftedOtmVol = vars2.logNormalVolCubeConstantSpread->volatility(expiry, term, strike);
                        BOOST_CHECK_SMALL(shiftedOtmVol - otmVol, tolerance);
                    }
                }
            }
        }
    }
}

test_suite* SwaptionVolConstantSpreadTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("SwaptionVolConstantSpreadTests");

    suite->add(BOOST_TEST_CASE(&SwaptionVolConstantSpreadTest::testAtmNormalVolShiftPropagation));
    suite->add(BOOST_TEST_CASE(&SwaptionVolConstantSpreadTest::testAtmLogNormalVolShiftPropagation));

    return suite;
}
} // namespace testsuite
