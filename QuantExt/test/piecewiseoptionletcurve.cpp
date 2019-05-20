/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <test/capfloormarketdata.hpp>
#include <test/yieldcurvemarketdata.hpp>
#include <test/toplevelfixture.hpp>

#include <qle/termstructures/piecewiseoptionletcurve.hpp>
#include <qle/termstructures/capfloorhelper.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace bdata = boost::unit_test::data;
typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;

namespace {

// Variables to be used in the test
struct CommonVars {
    
    // Constructor
    CommonVars()
        : referenceDate(5, Feb, 2016),
          settlementDays(0),
          calendar(TARGET()),
          bdc(Following),
          dayCounter(Actual365Fixed()),
          accuracy(1.0e-12),
          tolerance(1.0e-12) {
        
        // Reference date
        Settings::instance().evaluationDate() = referenceDate;
        
        // Set cap floor ibor index to EUR-EURIBOR-6M and attach a forwarding curve
        iborIndex = boost::make_shared<Euribor6M>(testYieldCurves.forward6M);
    }

    // Valuation date for the test
    Date referenceDate;

    // Variables used in the optionlet volatility structure creation
    Natural settlementDays;
    Calendar calendar;
    BusinessDayConvention bdc;
    DayCounter dayCounter;

    // Accuracy for optionlet stripping
    Real accuracy;

    // Test tolerance for comparing the NPVs
    Real tolerance;

    // Cap floor ibor index
    boost::shared_ptr<IborIndex> iborIndex;

    // EUR cap floor test volatility data from file test/capfloormarketdata.hpp
    CapFloorVolatilityEUR testVols;

    // EUR discount curve test data from file test/yieldcurvemarketdata.hpp
    YieldCurveEUR testYieldCurves;
};

}

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PiecewiseOptionletCurveTests)

BOOST_DATA_TEST_CASE_F(CommonVars, testPiecewiseOptionletStripping, bdata::xrange(6), strikeIdx) {
    
    BOOST_TEST_MESSAGE("Testing piecewise optionlet stripping of cap floor quotes along a strike column");

    CapFloor::Type capFloorType = CapFloor::Cap;
    CapFloorHelper::QuoteType quoteType = CapFloorHelper::Volatility;
    VolatilityType quoteVolatilityType = Normal;
    Real quoteDisplacement = 0.0;

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Cap floor type: " << capFloorType);
    BOOST_TEST_MESSAGE("  Cap floor strike: " << testVols.strikes[strikeIdx]);
    BOOST_TEST_MESSAGE("  Quote type: " << quoteType);
    if (quoteType == CapFloorHelper::Volatility) {
        BOOST_TEST_MESSAGE("  Quote volatility type: " << quoteVolatilityType);
        BOOST_TEST_MESSAGE("  Quote displacement: " << quoteDisplacement);
    }

    // Form the cap floor helper instrument for each tenor in the strike column
    vector<boost::shared_ptr<helper> > helpers(testVols.tenors.size());
    
    // Store each cap floor instrument in the strike column and its NPV using the flat cap floor volatilities
    vector<boost::shared_ptr<CapFloor> > instruments(testVols.tenors.size());
    vector<Real> flatNpvs(testVols.tenors.size());
    
    BOOST_TEST_MESSAGE("The input values at each tenor are:");
    for (Size i = 0; i < testVols.tenors.size(); i++) {
        
        // Get the relevant quote value
        Real volatility = testVols.nVols[i][strikeIdx];

        // Create the cap floor instrument and store its price using the quoted flat volatility
        instruments[i] = MakeCapFloor(capFloorType, testVols.tenors[i], iborIndex, testVols.strikes[strikeIdx]);
        if (quoteVolatilityType == ShiftedLognormal) {
            instruments[i]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter, quoteDisplacement));
        } else {
            instruments[i]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter));
        }
        flatNpvs[i] = instruments[i]->NPV();
        
        BOOST_TEST_MESSAGE("  (Tenor, Volatility, Flat NPV) = (" << testVols.tenors[i] << 
            ", " << volatility << ", " << flatNpvs[i] << ")");

        // Create a volatility or premium quote
        RelinkableHandle<Quote> quote;
        if (quoteType = CapFloorHelper::Volatility) {
            quote.linkTo(boost::make_shared<SimpleQuote>(volatility));
        } else {
            quote.linkTo(boost::make_shared<SimpleQuote>(flatNpvs[i]));
        }
        
        // Create the helper instrument
        helpers[i] = boost::make_shared<CapFloorHelper>(capFloorType, testVols.tenors[i], testVols.strikes[strikeIdx],
            quote, iborIndex, testYieldCurves.discountEonia, quoteType, quoteVolatilityType, quoteDisplacement);
    }

    // Create the piecewise optionlet curve and fail if it is not created
    VolatilityType curveVolatilityType = Normal;
    Real curveDisplacement = 0.0;
    boost::shared_ptr<PiecewiseOptionletCurve<Linear> > ovCurve;
    BOOST_REQUIRE_NO_THROW(ovCurve = boost::make_shared<PiecewiseOptionletCurve<Linear> >(referenceDate, helpers,
        calendar, bdc, dayCounter, curveVolatilityType, curveDisplacement, accuracy));
    Handle<OptionletVolatilityStructure> hovs(ovCurve);

    // Price each cap floor instrument using the piecewise optionlet curve and check it against the flat NPV
    BOOST_TEST_MESSAGE("The stripped values and differences at each tenor are:");
    Real strippedNpv;
    for (Size i = 0; i < testVols.tenors.size(); i++) {
        if (ovCurve->volatilityType() == ShiftedLognormal) {
            instruments[i]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, hovs));
        } else {
            instruments[i]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                testYieldCurves.discountEonia, hovs));
        }
        strippedNpv = instruments[i]->NPV();

        BOOST_TEST_MESSAGE("  (Tenor, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = (" << testVols.tenors[i] << 
            ", " << testVols.nVols[i][strikeIdx] << ", " << flatNpvs[i] << ", " << strippedNpv << ", " <<
            (flatNpvs[i] - strippedNpv) << ")");

        BOOST_CHECK_SMALL(fabs(flatNpvs[i] - strippedNpv), tolerance);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
