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
};

// Struct to hold a cap floor volatility surface and some associated meta data
struct Surface {
    boost::shared_ptr<QuantExt::CapFloorTermVolSurface> cfts;
    VolatilityType type;
    Real displacement;
    bool moving;
};

// Needed for data driven test case below as it writes out the Surface
ostream& operator<<(ostream& os, const Surface& s) {
    return os << "Surface: volatility type = " << s.type << ", shift = " << s.displacement << 
        ", moving = " << boolalpha << s.moving <<
        ", interpolation = " << s.cfts->interpolationMethod();
}

// From the EUR cap floor test volatility data in file test/capfloormarketdata.hpp, create a vector of 
// CapFloorTermVolSurface objects that will be the data in the data driven test below
vector<Surface> generateSurfaces() {

    // Will hold the generated surfaces
    vector<Surface> surfaces;

    // EUR cap floor test volatility data from file test/capfloormarketdata.hpp
    CapFloorVolatilityEUR testVols;

    // Some variables needed in surface creation
    Date referenceDate(5, Feb, 2016);
    Natural settlementDays = 0;
    Calendar calendar = TARGET();
    BusinessDayConvention bdc = Following;
    DayCounter dayCounter = Actual365Fixed();

    // Normal, BiCubic, floating reference date
    Surface s;
    s.type = Normal;
    s.displacement = 0.0;
    s.moving = true;
    s.cfts = boost::make_shared<QuantExt::CapFloorTermVolSurface>(settlementDays, calendar, bdc, testVols.tenors,
        testVols.strikes, testVols.nVols, dayCounter, QuantExt::CapFloorTermVolSurface::BicubicSpline);
    surfaces.push_back(s);

    // Normal, BiCubic, fixed reference date
    s.moving = false;
    s.cfts = boost::make_shared<QuantExt::CapFloorTermVolSurface>(referenceDate, calendar, bdc, testVols.tenors,
        testVols.strikes, testVols.nVols, dayCounter, QuantExt::CapFloorTermVolSurface::BicubicSpline);
    surfaces.push_back(s);

    // Normal, BiLinear, floating reference date
    s.moving = true;
    s.cfts = boost::make_shared<QuantExt::CapFloorTermVolSurface>(settlementDays, calendar, bdc, testVols.tenors,
        testVols.strikes, testVols.nVols, dayCounter, QuantExt::CapFloorTermVolSurface::Bilinear);
    surfaces.push_back(s);

    // Normal, BiLinear, fixed reference date
    s.moving = false;
    s.cfts = boost::make_shared<QuantExt::CapFloorTermVolSurface>(referenceDate, calendar, bdc, testVols.tenors,
        testVols.strikes, testVols.nVols, dayCounter, QuantExt::CapFloorTermVolSurface::Bilinear);
    surfaces.push_back(s);

    // Shifted Lognormal, BiCubic, floating reference date
    s.type = ShiftedLognormal;
    s.displacement = testVols.shift_1;
    s.moving = true;
    s.cfts = boost::make_shared<QuantExt::CapFloorTermVolSurface>(settlementDays, calendar, bdc, testVols.tenors,
        testVols.strikes, testVols.slnVols_1, dayCounter, QuantExt::CapFloorTermVolSurface::BicubicSpline);
    surfaces.push_back(s);

    // Shifted Lognormal, BiCubic, fixed reference date
    s.moving = false;
    s.cfts = boost::make_shared<QuantExt::CapFloorTermVolSurface>(referenceDate, calendar, bdc, testVols.tenors,
        testVols.strikes, testVols.slnVols_1, dayCounter, QuantExt::CapFloorTermVolSurface::BicubicSpline);
    surfaces.push_back(s);

    // Shifted Lognormal, BiLinear, floating reference date
    s.moving = true;
    s.cfts = boost::make_shared<QuantExt::CapFloorTermVolSurface>(settlementDays, calendar, bdc, testVols.tenors,
        testVols.strikes, testVols.slnVols_1, dayCounter, QuantExt::CapFloorTermVolSurface::Bilinear);
    surfaces.push_back(s);

    // Shifted Lognormal, BiLinear, fixed reference date
    s.moving = false;
    s.cfts = boost::make_shared<QuantExt::CapFloorTermVolSurface>(referenceDate, calendar, bdc, testVols.tenors,
        testVols.strikes, testVols.slnVols_1, dayCounter, QuantExt::CapFloorTermVolSurface::Bilinear);
    surfaces.push_back(s);

    return surfaces;
}

// Interpolation types for the data driven test case
typedef boost::variant<Linear, BackwardFlat, QuantExt::LinearFlat> InterpolationType;

vector<InterpolationType> timeInterpolationTypes = list_of
    (InterpolationType(Linear()))
    (InterpolationType(QuantExt::LinearFlat()))
    (InterpolationType(BackwardFlat()));

vector<InterpolationType> smileInterpolationTypes = list_of
    (InterpolationType(Linear()))
    (InterpolationType(QuantExt::LinearFlat()));

// If the optionlet structure has a flat first period or not
vector<bool> flatFirstPeriodValues = list_of(true)(false);

// So that I can reuse below
string to_string(const InterpolationType& interpolationType) {
    string result;
    switch (interpolationType.which()) {
    case 0:
        result = "Linear";
        break;
    case 1:
        result = "LinearFlat";
        break;
    case 2:
        result = "BackwardFlat";
        break;
    default:
        BOOST_FAIL("Unexpected interpolation type");
    }
    return result;
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
    bdata::make(generateSurfaces()) * bdata::make(timeInterpolationTypes) * bdata::make(smileInterpolationTypes) * 
    bdata::make(flatFirstPeriodValues), surface, timeInterp, smileInterp, flatFirstPeriod) {

    BOOST_TEST_MESSAGE("Testing piecewise optionlet stripping of cap floor surface");

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  " << surface);
    BOOST_TEST_MESSAGE("  Time Interpolation: " << to_string(timeInterp));
    BOOST_TEST_MESSAGE("  Smile Interpolation: " << to_string(smileInterp));
    BOOST_TEST_MESSAGE("  Flat first period: " << boolalpha << flatFirstPeriod);

    // Create the piecewise optionlet stripper from the surface and wrap in an adapter
    boost::shared_ptr<QuantExt::OptionletStripper> pwos;
    boost::shared_ptr<OptionletVolatilityStructure> ovts;
    VolatilityType ovType = Normal;
    Handle<YieldTermStructure> discount = testYieldCurves.discountEonia;
    switch (timeInterp.which()) {
    case 0:
        pwos = boost::make_shared<PiecewiseOptionletStripper<BOOST_TYPEOF(boost::get<Linear>(timeInterp))> >(
            surface.cfts, iborIndex, discount, accuracy, flatFirstPeriod, surface.type, surface.displacement, ovType);
        if (smileInterp.which() == 0) {
            if (surface.moving) {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<Linear>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))> >(pwos);
            } else {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<Linear>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))> >(referenceDate, pwos);
            }
        } else if (smileInterp.which() == 2) {
            if (surface.moving) {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<Linear>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))> >(pwos);
            } else {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<Linear>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))> >(referenceDate, pwos);
            }
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    case 1:
        pwos = boost::make_shared<PiecewiseOptionletStripper<BOOST_TYPEOF(boost::get<LinearFlat>(timeInterp))> >(
            surface.cfts, iborIndex, discount, accuracy, flatFirstPeriod, surface.type, surface.displacement, ovType);
        if (smileInterp.which() == 0) {
            if (surface.moving) {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<LinearFlat>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))> >(pwos);
            } else {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<LinearFlat>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))> >(referenceDate, pwos);
            }
        } else if (smileInterp.which() == 2) {
            if (surface.moving) {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<LinearFlat>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))> >(pwos);
            } else {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<LinearFlat>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))> >(referenceDate, pwos);
            }
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    case 2:
        pwos = boost::make_shared<PiecewiseOptionletStripper<BOOST_TYPEOF(boost::get<BackwardFlat>(timeInterp))> >(
            surface.cfts, iborIndex, discount, accuracy, flatFirstPeriod, surface.type, surface.displacement, ovType);
        if (smileInterp.which() == 0) {
            if (surface.moving) {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<BackwardFlat>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))> >(pwos);
            } else {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<BackwardFlat>(timeInterp)), BOOST_TYPEOF(boost::get<Linear>(smileInterp))> >(referenceDate, pwos);
            }
        } else if (smileInterp.which() == 2) {
            if (surface.moving) {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<BackwardFlat>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))> >(pwos);
            } else {
                ovts = boost::make_shared<StrippedOptionletAdapter<BOOST_TYPEOF(boost::get<BackwardFlat>(timeInterp)), BOOST_TYPEOF(boost::get<LinearFlat>(smileInterp))> >(referenceDate, pwos);
            }
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    default:
        BOOST_FAIL("Unexpected time interpolation type");
    }
    Handle<OptionletVolatilityStructure> hovts(ovts);

    // Price all of the input surface instruments using the cap floor term volatilities and again with the optionlet
    // volatilites and check that the NPVs match
    vector<Period> tenors = surface.cfts->optionTenors();
    vector<Rate> strikes = surface.cfts->strikes();
    boost::shared_ptr<CapFloor> capFloor;

    for (Size i = 0; i < tenors.size(); i++) {
        for (Size j = 0; j < strikes.size(); j++) {
            
            // Create the OTM cap floor instrument that we will price
            capFloor = MakeCapFloor(CapFloor::Cap, tenors[i], iborIndex, strikes[j]);
            Rate atm = capFloor->atmRate(**discount);
            if (strikes[j] < atm) {
                capFloor = MakeCapFloor(CapFloor::Floor, tenors[i], iborIndex, strikes[j]);
            }

            // Price the instrument using the flat term volatility
            Volatility flatVol = surface.cfts->volatility(tenors[i], strikes[j]);
            if (surface.type == ShiftedLognormal) {
                capFloor->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                    discount, flatVol, surface.cfts->dayCounter(), surface.displacement));
            } else {
                capFloor->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                    discount, flatVol, surface.cfts->dayCounter()));
            }
            Real flatNpv = capFloor->NPV();

            // Price the instrument using the stripped (Normal) optionlet surface
            capFloor->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(discount, hovts));
            Real strippedNpv = capFloor->NPV();

            // Check that the difference is within the tolerance
            Real diff = fabs(flatNpv - strippedNpv);
            BOOST_CHECK_SMALL(diff, tolerance);
            
            BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = (" <<
                capFloor->type() << ", " << tenors[i] << ", " << flatVol <<
                ", " << flatNpv << ", " << strippedNpv << ", " << diff << ")");
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
