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

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

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

        // Set up the swaption matrices
        atmNormalVolMatrix = boost::make_shared<SwaptionVolatilityMatrix>(referenceDate, conventions.fixedCalendar, 
            conventions.fixedConvention, atmVols.optionTenors, atmVols.swapTenors, 
            atmVols.nVols, Actual365Fixed(), true, Normal);
    }

    // Members
    Date referenceDate;
    conventionsEUR conventions;
    AtmVolatilityEUR atmVols;
    YieldCurveEUR yieldCurves;
    boost::shared_ptr<SwapConventions> swapConventions;
    boost::shared_ptr<SwaptionVolatilityStructure> atmNormalVolMatrix;
    // boost::shared_ptr<SwaptionVolatilityStructure> atmLogNormalVolMatrix;
    // boost::shared_ptr<SwaptionVolatilityStructure> atmShiftedLogNormalVolMatrix;
    SavedSettings backup;
};
}

namespace testsuite {

void SwaptionVolatilityConverterTest::testNormalToLognormal() {
    BOOST_TEST_MESSAGE("Testing conversion of swaption vols from normal to lognormal...");

    CommonVars vars;

    // Set up the converter (Normal -> Lognormal with no shifts)
    SwaptionVolatilityConverter converter(vars.referenceDate, vars.atmNormalVolMatrix,
        vars.yieldCurves.discountEonia, vars.swapConventions, ShiftedLognormal);

    // Get back converted volatility structure and test result on pillar points
    boost::shared_ptr<SwaptionVolatilityStructure> convertedsvs = converter.convert();
    Volatility outVol = 0.0;
    Volatility inVol = 0.0;
    Real dummyStrike = 0.0;
    for (Size i = 0; i < vars.atmVols.optionTenors.size(); ++i) {
        for (Size j = 0; j < vars.atmVols.swapTenors.size(); ++j) {
            Period optionTenor = vars.atmVols.optionTenors[i];
            Period swapTenor = vars.atmVols.swapTenors[j];
            inVol = vars.atmNormalVolMatrix->volatility(optionTenor, swapTenor, dummyStrike);
            outVol = convertedsvs->volatility(optionTenor, swapTenor, dummyStrike);
            BOOST_TEST_MESSAGE("OptionTenor: " << optionTenor << ", SwapTenor: " << swapTenor 
                << ", inVol: " << inVol << ", outVol: " << outVol);
        }
    }
}

test_suite* SwaptionVolatilityConverterTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("SwaptionVolatilityConverterTests");

    suite->add(BOOST_TEST_CASE(&SwaptionVolatilityConverterTest::testNormalToLognormal));

    return suite;
}
}
