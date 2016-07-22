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

#include "swaptionvolatilityconverter.hpp"
#include "swaptionmarketdata.hpp"
#include "yieldcurvemarketdata.hpp"

#include <qle/termstructures/swaptionvolatilityconverter.hpp>

#include <boost/assign/list_of.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;
using namespace boost::assign;

namespace {
// Variables to be used in the test
struct CommonVars {
    // Constructor
    CommonVars() : referenceDate(5, Feb, 2016) {
        // Reference date
        Settings::instance().evaluationDate() = referenceDate;

        // Link ibor index to correct forward curve
        boost::shared_ptr<IborIndex> iborIndex = conventions.floatIndex->clone(yieldCurves.forward6M);

        // Create underlying swap conventions
        swapConventions = boost::make_shared<SwapConventions>(conventions.settlementDays, conventions.fixedTenor,
            conventions.fixedCalendar, conventions.fixedConvention, conventions.fixedDayCounter, iborIndex);

        // Set up the various swaption matrices
        atmNormalVolMatrix = boost::make_shared<SwaptionVolatilityMatrix>(referenceDate, conventions.fixedCalendar, 
            conventions.fixedConvention, atmVols.optionTenors, atmVols.swapTenors, 
            atmVols.nVols, Actual365Fixed(), true, Normal);

        atmLogNormalVolMatrix = boost::make_shared<SwaptionVolatilityMatrix>(referenceDate, conventions.fixedCalendar,
            conventions.fixedConvention, atmVols.optionTenors, atmVols.swapTenors,
            atmVols.lnVols, Actual365Fixed(), true, ShiftedLognormal);

        atmShiftedLogNormalVolMatrix_1 = boost::make_shared<SwaptionVolatilityMatrix>(referenceDate, 
            conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors, atmVols.swapTenors,
            atmVols.slnVols_1, Actual365Fixed(), true, ShiftedLognormal, atmVols.shifts_1);

        atmShiftedLogNormalVolMatrix_2 = boost::make_shared<SwaptionVolatilityMatrix>(referenceDate,
            conventions.fixedCalendar, conventions.fixedConvention, atmVols.optionTenors, atmVols.swapTenors,
            atmVols.slnVols_2, Actual365Fixed(), true, ShiftedLognormal, atmVols.shifts_2);
    }

    // Members
    Date referenceDate;
    conventionsEUR conventions;
    AtmVolatilityEUR atmVols;
    YieldCurveEUR yieldCurves;
    boost::shared_ptr<SwapConventions> swapConventions;
    boost::shared_ptr<SwaptionVolatilityStructure> atmNormalVolMatrix;
    boost::shared_ptr<SwaptionVolatilityStructure> atmLogNormalVolMatrix;
    boost::shared_ptr<SwaptionVolatilityStructure> atmShiftedLogNormalVolMatrix_1;
    boost::shared_ptr<SwaptionVolatilityStructure> atmShiftedLogNormalVolMatrix_2;
    SavedSettings backup;
};
}

namespace testsuite {

void SwaptionVolatilityConverterTest::testNormalToLognormal() {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from normal to lognormal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Normal -> Lognormal with no shifts)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmNormalVolMatrix,
        vars.yieldCurves.discountEonia, vars.swapConventions, ShiftedLognormal);

    // Get back converted volatility structure and test result on pillar points
    boost::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.lnVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

void SwaptionVolatilityConverterTest::testLognormalToNormal() {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from lognormal to normal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Lognormal with no shifts -> Normal)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmLogNormalVolMatrix,
        vars.yieldCurves.discountEonia, vars.swapConventions, Normal);

    // Get back converted volatility structure and test result on pillar points
    boost::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.nVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

void SwaptionVolatilityConverterTest::testNormalToShiftedLognormal() {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from normal to shifted lognormal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Normal -> Shifted Lognormal with shift set 1)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmNormalVolMatrix,
        vars.yieldCurves.discountEonia, vars.swapConventions, ShiftedLognormal, vars.atmVols.shifts_1);

    // Get back converted volatility structure and test result on pillar points
    boost::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.slnVols_1[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

void SwaptionVolatilityConverterTest::testShiftedLognormalToShiftedLognormal() {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from shifted lognormal to shifted lognormal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Normal -> Shifted Lognormal with shift set 1)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmShiftedLogNormalVolMatrix_1,
        vars.yieldCurves.discountEonia, vars.swapConventions, ShiftedLognormal, vars.atmVols.shifts_2);

    // Get back converted volatility structure and test result on pillar points
    boost::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.slnVols_2[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

void SwaptionVolatilityConverterTest::testShiftedLognormalToNormal() {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from shifted lognormal to normal...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Shifted Lognormal with shift set 2 -> Normal)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmShiftedLogNormalVolMatrix_2,
        vars.yieldCurves.discountEonia, vars.swapConventions, Normal);

    // Get back converted volatility structure and test result on pillar points
    boost::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.nVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

void SwaptionVolatilityConverterTest::testFailureImplyingVol() {
    BOOST_TEST_MESSAGE("Testing failure to imply lognormal vol from normal vol...");

    CommonVars vars;

    // Normal volatility matrix where we cannot imply lognormal vol at 3M x 1Y point
    vector<Period> optionTenors = list_of(Period(3, Months))(Period(1, Years));
    vector<Period> swapTenors = list_of(Period(1, Years))(Period(5, Years));
    Matrix normalVols(2, 2);
    normalVols[0][0] = 0.003340; normalVols[0][1] = 0.004973;
    normalVols[1][0] = 0.003543; normalVols[1][1] = 0.005270;

    boost::shared_ptr<SwaptionVolatilityStructure> volMatrix = boost::make_shared<SwaptionVolatilityMatrix>(
        vars.referenceDate, vars.conventions.fixedCalendar, vars.conventions.fixedConvention, optionTenors, 
        swapTenors, normalVols, Actual365Fixed(), true, Normal);

    // Set up the converter (Normal -> Lognormal)
    SwaptionVolatilityConverter converter(vars.referenceDate, volMatrix, vars.yieldCurves.discountEonia, 
        vars.swapConventions, ShiftedLognormal);

    // We expect the conversion to fail
    BOOST_CHECK_THROW(converter.convert(), QuantLib::Error);
}

void SwaptionVolatilityConverterTest::testNormalShiftsIgnored() {
    BOOST_TEST_MESSAGE("Testing shifts supplied to normal converter ignored...");

    CommonVars vars;

    // Tolerance used in boost check
    Real tolerance = 0.00001;

    // Set up the converter (Lognormal with no shifts -> Normal)
    // We supply target shifts but they are ignored since target type is Normal
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmLogNormalVolMatrix,
        vars.yieldCurves.discountEonia, vars.swapConventions, Normal, vars.atmVols.shifts_1);

    // Get back converted volatility structure and test result on pillar points
    boost::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility targetVol = 0.0;
    Volatility outVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            targetVol = vars.atmVols.nVols[i][j];
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_CHECK_SMALL(outVol - targetVol, tolerance);
        }
    }
}

test_suite* SwaptionVolatilityConverterTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("SwaptionVolatilityConverterTests");

    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityConverterTest::testNormalToLognormal));
    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityConverterTest::testLognormalToNormal));
    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityConverterTest::testNormalToShiftedLognormal));
    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityConverterTest::testShiftedLognormalToShiftedLognormal));
    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityConverterTest::testShiftedLognormalToNormal));
    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityConverterTest::testFailureImplyingVol));
    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityConverterTest::testNormalShiftsIgnored));

    return suite;
}
}
