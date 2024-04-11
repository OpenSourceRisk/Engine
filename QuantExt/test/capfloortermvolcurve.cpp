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

#include <boost/assign.hpp>
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <boost/variant.hpp>

#include <test/capfloormarketdata.hpp>
#include <test/toplevelfixture.hpp>

#include <ql/math/interpolations/backwardflatinterpolation.hpp>
#include <ql/quotes/simplequote.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/capfloortermvolcurve.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace bdata = boost::unit_test::data;
using boost::assign::list_of;
using boost::assign::map_list_of;
using std::boolalpha;
using std::fixed;
using std::map;
using std::ostream;
using std::pair;
using std::setprecision;
using std::string;
using std::vector;

namespace {

// Variables to be used in the test
struct CommonVars : public qle::test::TopLevelFixture {

    // Constructor
    CommonVars()
        : referenceDate(5, Feb, 2016), settlementDays(0), calendar(TARGET()), bdc(Following),
          dayCounter(Actual365Fixed()), tolerance(1.0e-12) {

        // Reference date
        Settings::instance().evaluationDate() = referenceDate;

        // Test cap floor data
        CapFloorVolatilityEUR testData;
        tenors = testData.atmTenors;
        volQuotes.resize(tenors.size());
        volHandles.resize(tenors.size());
        for (Size i = 0; i < tenors.size(); ++i) {
            volQuotes[i] = QuantLib::ext::make_shared<SimpleQuote>(testData.nAtmVols[i]);
            volHandles[i] = Handle<Quote>(volQuotes[i]);
        }
    }

    // Valuation date for the test
    Date referenceDate;

    // Variables used in the optionlet volatility structure creation
    Natural settlementDays;
    Calendar calendar;
    BusinessDayConvention bdc;
    DayCounter dayCounter;

    // Test tolerance for comparing the NPVs
    Real tolerance;

    // Cap floor test data
    vector<Period> tenors;
    vector<QuantLib::ext::shared_ptr<SimpleQuote> > volQuotes;
    vector<Handle<Quote> > volHandles;
};

// Interpolation types for the data driven test case
typedef boost::variant<Linear, BackwardFlat, QuantExt::LinearFlat, Cubic, QuantExt::CubicFlat> InterpolationType;

vector<InterpolationType> interpolationTypes = list_of(InterpolationType(Linear()))(InterpolationType(BackwardFlat()))(
    InterpolationType(QuantExt::LinearFlat()))(InterpolationType(Cubic()))(InterpolationType(QuantExt::CubicFlat()));

// If the cap floor term volatility curve in the test has a floating or fixed reference date
vector<bool> isMovingValues = list_of(true)(false);

// If the cap floor term volatility curve has a flat first period or not
vector<bool> flatFirstPeriodValues = list_of(true)(false);

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
    case 3:
        result = "Cubic";
        break;
    case 4:
        result = "CubicFlat";
        break;
    default:
        BOOST_FAIL("Unexpected interpolation type");
    }
    return result;
}

} // namespace

// Needed for BOOST_DATA_TEST_CASE below
// https://stackoverflow.com/a/33965517/1771882
namespace boost {
namespace test_tools {
namespace tt_detail {
template <> struct print_log_value<InterpolationType> {
    void operator()(ostream& os, const InterpolationType& interpolationType) { os << to_string(interpolationType); }
};
} // namespace tt_detail
} // namespace test_tools
} // namespace boost

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CapFloorTermVolCurveTests)

BOOST_DATA_TEST_CASE_F(CommonVars, testCapFloorTermVolCurveInterpolation,
                       bdata::make(interpolationTypes) * bdata::make(isMovingValues) *
                           bdata::make(flatFirstPeriodValues),
                       interpolationType, isMoving, flatFirstPeriod) {

    BOOST_TEST_MESSAGE("Testing cap floor term volatility curve with different interpolation methods");

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Interpolation type: " << to_string(interpolationType));
    BOOST_TEST_MESSAGE("  Floating reference date: " << boolalpha << isMoving);
    BOOST_TEST_MESSAGE("  Flat first period: " << boolalpha << flatFirstPeriod);

    // Create the CapFloorTermVolatilityStructure using the appropriate interpolation and reference date/settlement days
    QuantLib::ext::shared_ptr<CapFloorTermVolatilityStructure> cftvs;
    switch (interpolationType.which()) {
    case 0:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using Linear interpolation with a moving reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear> >(
                                       settlementDays, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        } else {
            BOOST_TEST_MESSAGE("Using Linear interpolation with a fixed reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear> >(
                                       referenceDate, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        }
        break;
    case 1:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using BackwardFlat interpolation with a moving reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<BackwardFlat> >(
                                       settlementDays, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        } else {
            BOOST_TEST_MESSAGE("Using BackwardFlat interpolation with a fixed reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<BackwardFlat> >(
                                       referenceDate, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        }
        break;
    case 2:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using LinearFlat interpolation with a moving reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<LinearFlat> >(
                                       settlementDays, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        } else {
            BOOST_TEST_MESSAGE("Using LinearFlat interpolation with a fixed reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<LinearFlat> >(
                                       referenceDate, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        }
        break;
    case 3:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using Cubic interpolation with a moving reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic> >(
                                       settlementDays, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        } else {
            BOOST_TEST_MESSAGE("Using Cubic interpolation with a fixed reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic> >(
                                       referenceDate, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        }
        break;
    case 4:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using CubicFlat interpolation with a moving reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<CubicFlat> >(
                                       settlementDays, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        } else {
            BOOST_TEST_MESSAGE("Using CubicFlat interpolation with a fixed reference date");
            BOOST_REQUIRE_NO_THROW(cftvs = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<CubicFlat> >(
                                       referenceDate, calendar, bdc, tenors, volHandles, dayCounter, flatFirstPeriod));
        }
        break;
    default:
        BOOST_FAIL("Unexpected interpolation type");
    }

    BOOST_TEST_MESSAGE("Test the initial curve dates");
    for (Size i = 0; i < tenors.size(); ++i) {
        Date curveDate = cftvs->optionDateFromTenor(tenors[i]);
        Date manualDate = calendar.advance(referenceDate, tenors[i], bdc);
        BOOST_CHECK_EQUAL(curveDate, manualDate);
    }

    BOOST_TEST_MESSAGE("Test that curve returns input values on pillars");
    for (Size i = 0; i < tenors.size(); ++i) {
        Volatility vol = cftvs->volatility(tenors[i], 0.01);
        BOOST_CHECK_SMALL(volQuotes[i]->value() - vol, tolerance);
    }

    // Bump the 5Y ATM volatility quote (3rd element in quote vector)
    Size idx = 2;
    Volatility bump = 0.0005;
    Volatility baseValue = volQuotes[idx]->value();
    volQuotes[idx]->setValue(baseValue + bump);

    BOOST_TEST_MESSAGE("Test that curve returns input values on pillars after bump");
    for (Size i = 0; i < tenors.size(); ++i) {
        Volatility vol = cftvs->volatility(tenors[i], 0.01);
        BOOST_CHECK_SMALL(volQuotes[i]->value() - vol, tolerance);
        // Also check the point was bumped
        if (i == idx) {
            BOOST_CHECK_SMALL(vol - baseValue - bump, tolerance);
        }
    }

    BOOST_TEST_MESSAGE("Test the curve dates after moving the evaluation date");
    Date manualDate;
    Date newDate = calendar.advance(referenceDate, 1 * Months);
    Settings::instance().evaluationDate() = newDate;
    for (Size i = 0; i < tenors.size(); ++i) {
        Date curveDate = cftvs->optionDateFromTenor(tenors[i]);
        manualDate =
            isMoving ? calendar.advance(newDate, tenors[i], bdc) : calendar.advance(referenceDate, tenors[i], bdc);
        BOOST_CHECK_EQUAL(curveDate, manualDate);
    }

    BOOST_TEST_MESSAGE("Test that curve returns input values after moving the evaluation date");
    for (Size i = 0; i < tenors.size(); ++i) {
        Volatility vol = cftvs->volatility(tenors[i], 0.01);
        BOOST_CHECK_SMALL(volQuotes[i]->value() - vol, tolerance);
    }

    // Reset the evaluation date
    Settings::instance().evaluationDate() = referenceDate;

    BOOST_TEST_MESSAGE("Test extrapolation settings with out of range date");
    Date oorDate = cftvs->maxDate() + 1 * Months;
    BOOST_CHECK_NO_THROW(cftvs->volatility(oorDate, 0.01, true));
    BOOST_CHECK_THROW(cftvs->volatility(oorDate, 0.01), Error);
    cftvs->enableExtrapolation();
    BOOST_CHECK_NO_THROW(cftvs->volatility(oorDate, 0.01));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
