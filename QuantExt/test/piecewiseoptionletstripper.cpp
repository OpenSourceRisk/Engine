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
#include <boost/variant.hpp>
#include <boost/assign.hpp>
#include <boost/typeof/typeof.hpp>

#include <test/capfloormarketdata.hpp>
#include <test/yieldcurvemarketdata.hpp>
#include <test/toplevelfixture.hpp>

#include <qle/termstructures/piecewiseoptionletstripper.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>
#include <qle/termstructures/capfloorhelper.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace bdata = boost::unit_test::data;
typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;
using TermVolSurface = QuantExt::CapFloorTermVolSurface;
using std::ostream;
using std::boolalpha;
using std::string;
using std::map;
using std::vector;
using std::pair;
using std::fixed;
using std::setprecision;
using boost::assign::list_of;
using boost::assign::map_list_of;

namespace {

// Variables to be used in the test
struct CommonVars : public qle::test::TopLevelFixture {
    
    // Constructor
    CommonVars()
        : referenceDate(5, Feb, 2016),
          settlementDays(0),
          calendar(TARGET()),
          bdc(Following),
          dayCounter(Actual365Fixed()),
          accuracy(1.0e-12),
          tolerance(1.0e-10) {
        
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

    // EUR discount curve test data from file test/yieldcurvemarketdata.hpp
    YieldCurveEUR testYieldCurves;

    // EUR cap floor test volatility data from file test/capfloormarketdata.hpp
    CapFloorVolatilityEUR testVols;
};

// Type of input cap floor volatility
vector<VolatilityType> volatilityTypes = list_of(Normal)(ShiftedLognormal);

// Interpolation types for the data driven test case
typedef boost::variant<Linear, BackwardFlat, QuantExt::LinearFlat> InterpolationType;

vector<InterpolationType> timeInterpolationTypes = list_of
    (InterpolationType(Linear()))
    (InterpolationType(BackwardFlat()))
    (InterpolationType(QuantExt::LinearFlat()));

vector<InterpolationType> smileInterpolationTypes = list_of
    (InterpolationType(Linear()))
    (InterpolationType(QuantExt::LinearFlat()));

// If the optionlet structure has a flat first period or not
vector<bool> flatFirstPeriodValues = list_of(true)(false);

// If the built optionlet structure in the test has a floating or fixed reference date
vector<bool> isMovingValues = list_of(true)(false);

// The interpolation method on the cap floor term volatility surface
vector<TermVolSurface::InterpolationMethod> vsInterpMethods = 
    list_of(TermVolSurface::BicubicSpline)(TermVolSurface::Bilinear);

// So that I can reuse below
string to_string(const InterpolationType& interpolationType) {
    string result;
    switch (interpolationType.which()) {
    case 0:
        result = "Linear";
        break;
    case 1:
        result = "BackwardFlat";
        break;
    case 2:
        result = "LinearFlat";
        break;
    default:
        BOOST_FAIL("Unexpected interpolation type");
    }
    return result;
}

// Create the OptionletVolatilityStructure using a PiecewiseOptionletStripper
template <class TI, class SI>
Handle<OptionletVolatilityStructure> createOvs(VolatilityType volatilityType,
    bool flatFirstPeriod, bool isMoving, TermVolSurface::InterpolationMethod vsInterpMethod) {

    // Another instance of CommonVars - works but a bit inefficient
    CommonVars vars;

    // Decide on input volatilities depending on type requested
    Matrix vols;
    Real displacement = 0.0;
    if (volatilityType == Normal) {
        vols = vars.testVols.nVols;
    } else {
        vols = vars.testVols.slnVols_1;
        displacement = vars.testVols.shift_1;
    }

    // Create the cap floor term vol surface
    boost::shared_ptr<TermVolSurface> cfts;
    if (isMoving) {
        cfts = boost::make_shared<TermVolSurface>(vars.settlementDays, vars.calendar, vars.bdc, vars.testVols.tenors,
            vars.testVols.strikes, vols, vars.dayCounter, vsInterpMethod);
    } else {
        cfts = boost::make_shared<TermVolSurface>(vars.referenceDate, vars.calendar, vars.bdc, vars.testVols.tenors,
            vars.testVols.strikes, vols, vars.dayCounter, vsInterpMethod);
    }

    // Create the piecewise optionlet stripper
    VolatilityType ovType = Normal;
    boost::shared_ptr<QuantExt::OptionletStripper> pwos = boost::make_shared<PiecewiseOptionletStripper<TI> >(
        cfts, vars.iborIndex, vars.testYieldCurves.discountEonia, vars.accuracy, flatFirstPeriod, 
        volatilityType, displacement, ovType);

    // Create the OptionletVolatilityStructure
    boost::shared_ptr<OptionletVolatilityStructure> ovs;
    if (isMoving) {
        ovs = boost::make_shared<StrippedOptionletAdapter<TI, SI> >(pwos);
    } else {
        ovs = boost::make_shared<StrippedOptionletAdapter<TI, SI> >(vars.referenceDate, pwos);
    }

    return Handle<OptionletVolatilityStructure>(ovs);
}

}

// Needed for BOOST_DATA_TEST_CASE below
// https://stackoverflow.com/a/33965517/1771882
namespace boost {
namespace test_tools {
namespace tt_detail {
template <> struct print_log_value<InterpolationType> {
    void operator()(ostream& os, const InterpolationType& interpolationType) {
        os << to_string(interpolationType);
    }
};
}
}
}

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PiecewiseOptionletStripperTests)

BOOST_DATA_TEST_CASE_F(CommonVars, testPiecewiseOptionletSurfaceStripping, 
    bdata::make(volatilityTypes) * bdata::make(timeInterpolationTypes) * bdata::make(smileInterpolationTypes) * 
    bdata::make(flatFirstPeriodValues) * bdata::make(isMovingValues) * bdata::make(vsInterpMethods),
    volatilityType, timeInterp, smileInterp, flatFirstPeriod, isMoving, vsInterpMethod) {

    BOOST_TEST_MESSAGE("Testing piecewise optionlet stripping of cap floor surface");

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Input volatility type:" << volatilityType);
    BOOST_TEST_MESSAGE("  Time Interpolation: " << to_string(timeInterp));
    BOOST_TEST_MESSAGE("  Smile Interpolation: " << to_string(smileInterp));
    BOOST_TEST_MESSAGE("  Flat first period: " << boolalpha << flatFirstPeriod);
    BOOST_TEST_MESSAGE("  Floating reference date: " << boolalpha << isMoving);
    BOOST_TEST_MESSAGE("  Cap floor term interpolation: " << vsInterpMethod);

    // Create the piecewise optionlet stripper from the surface and wrap in an adapter
    Handle<OptionletVolatilityStructure> ovs;
    switch (timeInterp.which()) {
    case 0:
        if (smileInterp.which() == 0) {
            ovs = createOvs<BOOST_TYPEOF(boost::get<Linear>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))>(
                volatilityType, flatFirstPeriod, isMoving, vsInterpMethod);
        } else if (smileInterp.which() == 2) {
            ovs = createOvs<BOOST_TYPEOF(boost::get<Linear>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))>(
                volatilityType, flatFirstPeriod, isMoving, vsInterpMethod);
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    case 1:
        if (smileInterp.which() == 0) {
            ovs = createOvs<BOOST_TYPEOF(boost::get<BackwardFlat>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))>(
                volatilityType, flatFirstPeriod, isMoving, vsInterpMethod);
        } else if (smileInterp.which() == 2) {
            ovs = createOvs<BOOST_TYPEOF(boost::get<BackwardFlat>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))>(
                volatilityType, flatFirstPeriod, isMoving, vsInterpMethod);
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    case 2:
        if (smileInterp.which() == 0) {
            ovs = createOvs<BOOST_TYPEOF(boost::get<LinearFlat>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))>(
                volatilityType, flatFirstPeriod, isMoving, vsInterpMethod);
        } else if (smileInterp.which() == 2) {
            ovs = createOvs<BOOST_TYPEOF(boost::get<LinearFlat>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))>(
                volatilityType, flatFirstPeriod, isMoving, vsInterpMethod);
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    default:
        BOOST_FAIL("Unexpected time interpolation type");
    }

    // Price all of the input surface instruments using the cap floor term volatilities and again with the optionlet
    // volatilites and check that the NPVs match
    Handle<YieldTermStructure> discount = testYieldCurves.discountEonia;
    boost::shared_ptr<CapFloor> capFloor;

    for (Size i = 0; i < testVols.tenors.size(); i++) {
        for (Size j = 0; j < testVols.strikes.size(); j++) {
            
            // Create the OTM cap floor instrument that we will price
            capFloor = MakeCapFloor(CapFloor::Cap, testVols.tenors[i], iborIndex, testVols.strikes[j]);
            Rate atm = capFloor->atmRate(**discount);
            if (testVols.strikes[j] < atm) {
                capFloor = MakeCapFloor(CapFloor::Floor, testVols.tenors[i], iborIndex, testVols.strikes[j]);
            }

            // Price the instrument using the flat term volatility
            Volatility flatVol;
            if (volatilityType == ShiftedLognormal) {
                flatVol = testVols.slnVols_1[i][j];
                capFloor->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                    discount, flatVol, dayCounter, testVols.shift_1));
            } else {
                flatVol = testVols.nVols[i][j];
                capFloor->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                    discount, flatVol, dayCounter));
            }
            Real flatNpv = capFloor->NPV();

            // Price the instrument using the stripped (Normal) optionlet surface
            capFloor->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(discount, ovs));
            Real strippedNpv = capFloor->NPV();

            // Check that the difference is within the tolerance
            Real diff = fabs(flatNpv - strippedNpv);
            BOOST_CHECK_SMALL(diff, tolerance);
            
            BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = (" <<
                capFloor->type() << ", " << testVols.tenors[i] << ", " << flatVol <<
                ", " << flatNpv << ", " << strippedNpv << ", " << diff << ")");
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
