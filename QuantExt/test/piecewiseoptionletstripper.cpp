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
#include <test/yieldcurvemarketdata.hpp>

#include <ql/instruments/makecapfloor.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/capfloorhelper.hpp>
#include <qle/termstructures/capfloortermvolcurve.hpp>
#include <qle/termstructures/optionletstripperwithatm.hpp>
#include <qle/termstructures/piecewiseoptionletstripper.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace bdata = boost::unit_test::data;
typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;
using TermVolSurface = QuantExt::CapFloorTermVolSurfaceExact;
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
          dayCounter(Actual365Fixed()), accuracy(1.0e-12), globalAccuracy(1.0e-10), tolerance(1.0e-10) {

        // Reference date
        Settings::instance().evaluationDate() = referenceDate;

        // Set cap floor ibor index to EUR-EURIBOR-6M and attach a forwarding curve
        iborIndex = QuantLib::ext::make_shared<Euribor6M>(testYieldCurves.forward6M);
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

    // Global accuracy for optionlet stripping
    Real globalAccuracy;

    // Test tolerance for comparing the NPVs
    Real tolerance;

    // Cap floor ibor index
    QuantLib::ext::shared_ptr<IborIndex> iborIndex;

    // EUR discount curve test data from file test/yieldcurvemarketdata.hpp
    YieldCurveEUR testYieldCurves;

    // EUR cap floor test volatility data from file test/capfloormarketdata.hpp
    CapFloorVolatilityEUR testVols;
};

// Type of input cap floor volatility
vector<VolatilityType> volatilityTypes = list_of(Normal)(ShiftedLognormal);

// Interpolation types for the data driven test case
typedef boost::variant<Linear, BackwardFlat, QuantExt::LinearFlat, Cubic, QuantExt::CubicFlat> InterpolationType;

vector<InterpolationType> timeInterpolationTypes =
    list_of(InterpolationType(Linear()))(InterpolationType(BackwardFlat()))(InterpolationType(QuantExt::LinearFlat()))(
        InterpolationType(Cubic()))(InterpolationType(QuantExt::CubicFlat()));

vector<InterpolationType> smileInterpolationTypes = list_of(InterpolationType(Linear()))(
    InterpolationType(QuantExt::LinearFlat()))(InterpolationType(Cubic()))(InterpolationType(QuantExt::CubicFlat()));

// If the optionlet structure has a flat first period or not
vector<bool> flatFirstPeriodValues = list_of(true)(false);

// If the built optionlet structure in the test has a floating or fixed reference date
vector<bool> isMovingValues = list_of(true)(false);

// Whether or not to try to add ATM values to the surface stripping
vector<bool> addAtmValues = list_of(true)(false);

// False to interpolate on cap floor term volatilities before bootstrapping
// True to interpolate on optionlet volatilities.
vector<bool> interpOnOptionletValues = list_of(true)(false);

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

// Create the OptionletVolatilityStructure using a PiecewiseOptionletStripper
template <class TI, class SI>
Handle<OptionletVolatilityStructure> createOvs(VolatilityType volatilityType, bool flatFirstPeriod, bool isMoving,
                                               TermVolSurface::InterpolationMethod vsInterpMethod,
                                               bool interpOnOptionlet, bool withAtm) {

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
    QuantLib::ext::shared_ptr<TermVolSurface> cfts;
    if (isMoving) {
        cfts = QuantLib::ext::make_shared<TermVolSurface>(vars.settlementDays, vars.calendar, vars.bdc, vars.testVols.tenors,
                                                  vars.testVols.strikes, vols, vars.dayCounter, vsInterpMethod);
    } else {
        cfts = QuantLib::ext::make_shared<TermVolSurface>(vars.referenceDate, vars.calendar, vars.bdc, vars.testVols.tenors,
                                                  vars.testVols.strikes, vols, vars.dayCounter, vsInterpMethod);
    }

    // Create the piecewise optionlet stripper
    VolatilityType ovType = Normal;
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> pwos = QuantLib::ext::make_shared<PiecewiseOptionletStripper<TI> >(
        cfts, vars.iborIndex, vars.testYieldCurves.discountEonia, flatFirstPeriod, volatilityType, displacement, ovType,
        0.0, interpOnOptionlet, TI(),
        QuantExt::IterativeBootstrap<
            typename PiecewiseOptionletStripper<TI, QuantExt::IterativeBootstrap>::optionlet_curve>(
            vars.accuracy, vars.globalAccuracy));

    // If true, we overlay ATM volatilities
    vector<Handle<Quote> > atmVolquotes(vars.testVols.atmTenors.size());
    if (withAtm) {

        QuantLib::ext::shared_ptr<CapFloorTermVolCurve> atmVolCurve;

        if (volatilityType == Normal) {

            for (Size i = 0; i < atmVolquotes.size(); ++i) {
                atmVolquotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(vars.testVols.nAtmVols[i]));
            }

            if (isMoving) {
                if (vsInterpMethod == TermVolSurface::Bilinear) {
                    atmVolCurve = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear> >(
                        vars.settlementDays, vars.calendar, vars.bdc, vars.testVols.atmTenors, atmVolquotes,
                        vars.dayCounter, flatFirstPeriod);
                } else {
                    atmVolCurve = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic> >(
                        vars.settlementDays, vars.calendar, vars.bdc, vars.testVols.atmTenors, atmVolquotes,
                        vars.dayCounter, flatFirstPeriod);
                }
            } else {
                if (vsInterpMethod == TermVolSurface::Bilinear) {
                    atmVolCurve = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear> >(
                        vars.referenceDate, vars.calendar, vars.bdc, vars.testVols.atmTenors, atmVolquotes,
                        vars.dayCounter, flatFirstPeriod);
                } else {
                    atmVolCurve = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic> >(
                        vars.referenceDate, vars.calendar, vars.bdc, vars.testVols.atmTenors, atmVolquotes,
                        vars.dayCounter, flatFirstPeriod);
                }
            }
        } else {

            for (Size i = 0; i < atmVolquotes.size(); ++i) {
                atmVolquotes[i] = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(vars.testVols.slnAtmVols_1[i]));
            }

            if (isMoving) {
                if (vsInterpMethod == TermVolSurface::Bilinear) {
                    atmVolCurve = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear> >(
                        vars.settlementDays, vars.calendar, vars.bdc, vars.testVols.atmTenors, atmVolquotes,
                        vars.dayCounter, flatFirstPeriod);
                } else {
                    atmVolCurve = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic> >(
                        vars.settlementDays, vars.calendar, vars.bdc, vars.testVols.atmTenors, atmVolquotes,
                        vars.dayCounter, flatFirstPeriod);
                }
            } else {
                if (vsInterpMethod == TermVolSurface::Bilinear) {
                    atmVolCurve = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear> >(
                        vars.referenceDate, vars.calendar, vars.bdc, vars.testVols.atmTenors, atmVolquotes,
                        vars.dayCounter, flatFirstPeriod);
                } else {
                    atmVolCurve = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic> >(
                        vars.referenceDate, vars.calendar, vars.bdc, vars.testVols.atmTenors, atmVolquotes,
                        vars.dayCounter, flatFirstPeriod);
                }
            }
        }

        Handle<CapFloorTermVolCurve> atmVolCurveH(atmVolCurve);
        pwos = QuantLib::ext::make_shared<OptionletStripperWithAtm<TI, SI> >(
            pwos, atmVolCurveH, vars.testYieldCurves.discountEonia, volatilityType, displacement);
    }

    // Create the OptionletVolatilityStructure
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ovs;
    if (isMoving) {
        ovs = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<TI, SI> >(pwos);
    } else {
        ovs = QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<TI, SI> >(vars.referenceDate, pwos);
    }

    return Handle<OptionletVolatilityStructure>(ovs);
}

// Variables for cached value tests

// Strikes: below min (-0.01), on pillar strikes (-0.005 & 0.03), between pillar strikes (0.015), above max strike
// (0.035)
vector<Rate> cachedStrikes = list_of(-0.01)(-0.005)(0.015)(0.030)(0.035);

// Cached optionlet fixing dates
vector<Date> cachedOptionletFixingDates = list_of(Date(5, Aug, 2016))(Date(7, Feb, 2017))(Date(7, Aug, 2017))(Date(
    7, Feb, 2018))(Date(7, Aug, 2018))(Date(7, Feb, 2019))(Date(7, Aug, 2019))(Date(6, Feb, 2020))(Date(6, Aug, 2020))(
    Date(5, Feb, 2021))(Date(5, Aug, 2021))(Date(7, Feb, 2022))(Date(5, Aug, 2022))(Date(7, Feb, 2023))(
    Date(7, Aug, 2023))(Date(7, Feb, 2024))(Date(7, Aug, 2024))(Date(6, Feb, 2025))(Date(7, Aug, 2025))(
    Date(5, Feb, 2026))(Date(6, Aug, 2026))(Date(5, Feb, 2027))(Date(5, Aug, 2027))(Date(7, Feb, 2028))(
    Date(7, Aug, 2028))(Date(7, Feb, 2029))(Date(7, Aug, 2029))(Date(7, Feb, 2030))(Date(7, Aug, 2030))(
    Date(6, Feb, 2031))(Date(7, Aug, 2031))(Date(5, Feb, 2032))(Date(5, Aug, 2032))(Date(7, Feb, 2033))(
    Date(5, Aug, 2033))(Date(7, Feb, 2034))(Date(7, Aug, 2034))(Date(7, Feb, 2035))(Date(7, Aug, 2035));

// Cached optionlet values at optionlet fixing dates
vector<Real> cachedValues =
    list_of(0.002457000000)(0.002457000000)(0.006386500000)(0.009938000000)(0.009938000000)(0.002880443155)(
        0.002880443155)(0.006459363495)(0.009751440414)(0.009751440414)(0.003292503430)(0.003292503430)(0.006530268295)(
        0.009569895870)(0.009569895870)(0.003711393433)(0.003711393433)(0.006602348312)(0.009385342300)(0.009385342300)(
        0.004123453708)(0.004123453708)(0.006673253111)(0.009203797756)(0.009203797756)(0.004542343711)(0.004542343711)(
        0.006745333128)(0.009019244187)(0.009019244187)(0.004954403986)(0.004954403986)(0.006816237927)(0.008837699643)(
        0.008837699643)(0.005371017413)(0.005371017413)(0.006887926205)(0.008654149082)(0.008654149082)(0.005785354264)(
        0.005785354264)(0.006959222744)(0.008471601530)(0.008471601530)(0.006092137648)(0.006092137648)(0.006976645818)(
        0.008397716202)(0.008397716202)(0.006395568208)(0.006395568208)(0.006993878476)(0.008324638363)(0.008324638363)(
        0.006707380827)(0.006707380827)(0.007011587175)(0.008249541800)(0.008249541800)(0.007007458563)(0.007007458563)(
        0.007028629417)(0.008177271451)(0.008177271451)(0.006955894311)(0.006955894311)(0.006917620059)(0.007958482968)(
        0.007958482968)(0.006905716195)(0.006905716195)(0.006809594824)(0.007745575895)(0.007745575895)(0.006854706398)(
        0.006854706398)(0.006699779115)(0.007529139976)(0.007529139976)(0.006804251054)(0.006804251054)(0.006591157055)(
        0.007315056621)(0.007315056621)(0.006753518484)(0.006753518484)(0.006481938171)(0.007099796984)(0.007099796984)(
        0.006703063140)(0.006703063140)(0.006373316111)(0.006885713629)(0.006885713629)(0.006623889360)(0.006623889360)(
        0.006363776326)(0.006870339773)(0.006870339773)(0.006544715579)(0.006544715579)(0.006354236541)(0.006854965916)(
        0.006854965916)(0.006465106778)(0.006465106778)(0.006344644340)(0.006839507588)(0.006839507588)(0.006386368018)(
        0.006386368018)(0.006335156972)(0.006824218203)(0.006824218203)(0.006305454154)(0.006305454154)(0.006325407521)(
        0.006808506460)(0.006808506460)(0.006226280373)(0.006226280373)(0.006315867737)(0.006793132603)(0.006793132603)(
        0.006146236551)(0.006146236551)(0.006306223119)(0.006777589803)(0.006777589803)(0.006067497791)(0.006067497791)(
        0.006296735750)(0.006762300419)(0.006762300419)(0.005987453969)(0.005987453969)(0.006287091133)(0.006746757619)(
        0.006746757619)(0.005908715209)(0.005908715209)(0.006277603764)(0.006731468234)(0.006731468234)(0.005829106407)(
        0.005829106407)(0.006268011563)(0.006716009906)(0.006716009906)(0.005749932627)(0.005749932627)(0.006258471778)(
        0.006700636049)(0.006700636049)(0.005670758846)(0.005670758846)(0.006248931994)(0.006685262193)(0.006685262193)(
        0.005591585065)(0.005591585065)(0.006239392209)(0.006669888336)(0.006669888336)(0.005510671202)(0.005510671202)(
        0.006229642758)(0.006654176593)(0.006654176593)(0.005432802483)(0.005432802483)(0.006220260223)(0.006639056152)(
        0.006639056152)(0.005351888619)(0.005351888619)(0.006210510772)(0.006623344408)(0.006623344408)(0.005273149860)(
        0.005273149860)(0.006201023404)(0.006608055023)(0.006608055023)(0.005193106037)(0.005193106037)(0.006191378786)(
        0.006592512223)(0.006592512223)(0.005114367277)(0.005114367277)(0.006181891418)(0.006577222839)(0.006577222839);

// Cached ad-hoc dates: before first fixing, between fixing dates, after max date
vector<Date> cachedAdHocDates = list_of(Date(5, May, 2016))(Date(5, May, 2026))(Date(5, May, 2036));

// Cached values at ad-hoc dates
vector<Real> cachedAdHocValues = list_of(0.002457000000)(0.002457000000)(0.006386500000)(0.009938000000)(
    0.009938000000)(0.006585172511)(0.006585172511)(0.006359111267)(0.006862821788)(0.006862821788)(0.005114367277)(
    0.005114367277)(0.006181891418)(0.006577222839)(0.006577222839);
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

BOOST_AUTO_TEST_SUITE(PiecewiseOptionletStripperTests)

BOOST_DATA_TEST_CASE_F(CommonVars, testPiecewiseOptionletSurfaceStripping,
                       bdata::make(volatilityTypes) * bdata::make(timeInterpolationTypes) *
                           bdata::make(smileInterpolationTypes) * bdata::make(flatFirstPeriodValues) *
                           bdata::make(isMovingValues) * bdata::make(vsInterpMethods) *
                           bdata::make(interpOnOptionletValues) * bdata::make(addAtmValues),
                       volatilityType, timeInterp, smileInterp, flatFirstPeriod, isMoving, vsInterpMethod,
                       interpOnOptionlet, addAtm) {

    BOOST_TEST_MESSAGE("Testing piecewise optionlet stripping of cap floor surface");

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Input volatility type:" << volatilityType);
    BOOST_TEST_MESSAGE("  Time Interpolation: " << to_string(timeInterp));
    BOOST_TEST_MESSAGE("  Smile Interpolation: " << to_string(smileInterp));
    BOOST_TEST_MESSAGE("  Flat first period: " << boolalpha << flatFirstPeriod);
    BOOST_TEST_MESSAGE("  Floating reference date: " << boolalpha << isMoving);
    BOOST_TEST_MESSAGE("  Cap floor term interpolation: " << vsInterpMethod);
    BOOST_TEST_MESSAGE("  Interpolate on optionlets: " << boolalpha << interpOnOptionlet);
    BOOST_TEST_MESSAGE("  Add in ATM curve: " << boolalpha << addAtm);

    // Create the piecewise optionlet stripper from the surface and wrap in an adapter
    Handle<OptionletVolatilityStructure> ovs;
    switch (timeInterp.which()) {
    case 0:
        if (smileInterp.which() == 0) {
            ovs = createOvs<Linear, Linear>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                            interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 2) {
            ovs = createOvs<Linear, LinearFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 3) {
            ovs = createOvs<Linear, Cubic>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod, interpOnOptionlet,
                                           addAtm);
        } else if (smileInterp.which() == 4) {
            ovs = createOvs<Linear, CubicFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                               interpOnOptionlet, addAtm);
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    case 1:
        if (smileInterp.which() == 0) {
            ovs = createOvs<BackwardFlat, Linear>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                  interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 2) {
            ovs = createOvs<BackwardFlat, LinearFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                      interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 3) {
            ovs = createOvs<BackwardFlat, Cubic>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                 interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 4) {
            ovs = createOvs<BackwardFlat, CubicFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                     interpOnOptionlet, addAtm);
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    case 2:
        if (smileInterp.which() == 0) {
            ovs = createOvs<LinearFlat, Linear>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 2) {
            ovs = createOvs<LinearFlat, LinearFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                    interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 3) {
            ovs = createOvs<LinearFlat, Cubic>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                               interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 4) {
            ovs = createOvs<LinearFlat, CubicFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                   interpOnOptionlet, addAtm);
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    case 3:
        if (smileInterp.which() == 0) {
            ovs = createOvs<Cubic, Linear>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod, interpOnOptionlet,
                                           addAtm);
        } else if (smileInterp.which() == 2) {
            ovs = createOvs<Cubic, LinearFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                               interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 3) {
            ovs = createOvs<Cubic, Cubic>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod, interpOnOptionlet,
                                          addAtm);
        } else if (smileInterp.which() == 4) {
            ovs = createOvs<Cubic, CubicFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                              interpOnOptionlet, addAtm);
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    case 4:
        if (smileInterp.which() == 0) {
            ovs = createOvs<CubicFlat, Linear>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                               interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 2) {
            ovs = createOvs<CubicFlat, LinearFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                   interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 3) {
            ovs = createOvs<CubicFlat, Cubic>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                              interpOnOptionlet, addAtm);
        } else if (smileInterp.which() == 4) {
            ovs = createOvs<CubicFlat, CubicFlat>(volatilityType, flatFirstPeriod, isMoving, vsInterpMethod,
                                                  interpOnOptionlet, addAtm);
        } else {
            BOOST_FAIL("Unexpected smile interpolation type");
        }
        break;
    default:
        BOOST_FAIL("Unexpected time interpolation type");
    }

    // Price all of the input surface instruments using the cap floor term volatilities and again with the optionlet
    // volatilities and check that the NPVs match
    Handle<YieldTermStructure> discount = testYieldCurves.discountEonia;
    QuantLib::ext::shared_ptr<CapFloor> capFloor;

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
                capFloor->setPricingEngine(
                    QuantLib::ext::make_shared<BlackCapFloorEngine>(discount, flatVol, dayCounter, testVols.shift_1));
            } else {
                flatVol = testVols.nVols[i][j];
                capFloor->setPricingEngine(QuantLib::ext::make_shared<BachelierCapFloorEngine>(discount, flatVol, dayCounter));
            }
            Real flatNpv = capFloor->NPV();

            // Price the instrument using the stripped (Normal) optionlet surface
            capFloor->setPricingEngine(QuantLib::ext::make_shared<BachelierCapFloorEngine>(discount, ovs));
            Real strippedNpv = capFloor->NPV();

            // Check that the difference is within the tolerance
            Real diff = fabs(flatNpv - strippedNpv);
            BOOST_CHECK_SMALL(diff, tolerance);

            BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = ("
                               << capFloor->type() << ", " << testVols.tenors[i] << ", " << testVols.strikes[j] << ", "
                               << flatVol << ", " << flatNpv << ", " << strippedNpv << ", " << diff << ")");
        }
    }

    // If we have added in ATM, test the ATM value also
    if (addAtm) {

        for (Size i = 0; i < testVols.atmTenors.size(); i++) {

            // Create the ATM cap (with a dummy strike first)
            capFloor = MakeCapFloor(CapFloor::Cap, testVols.atmTenors[i], iborIndex, 0.01);
            Rate atm = capFloor->atmRate(**discount);
            capFloor = MakeCapFloor(CapFloor::Cap, testVols.atmTenors[i], iborIndex, atm);

            // Price the instrument using the flat term volatility
            Volatility flatVol;
            if (volatilityType == ShiftedLognormal) {
                flatVol = testVols.slnAtmVols_1[i];
                capFloor->setPricingEngine(
                    QuantLib::ext::make_shared<BlackCapFloorEngine>(discount, flatVol, dayCounter, testVols.shift_1));
            } else {
                flatVol = testVols.nAtmVols[i];
                capFloor->setPricingEngine(QuantLib::ext::make_shared<BachelierCapFloorEngine>(discount, flatVol, dayCounter));
            }
            Real flatNpv = capFloor->NPV();

            // Price the instrument using the stripped (Normal) optionlet surface
            capFloor->setPricingEngine(QuantLib::ext::make_shared<BachelierCapFloorEngine>(discount, ovs));
            Real strippedNpv = capFloor->NPV();

            // Check that the difference is within the tolerance
            Real diff = fabs(flatNpv - strippedNpv);
            BOOST_CHECK_SMALL(diff, tolerance);

            BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = ("
                               << capFloor->type() << ", " << testVols.atmTenors[i] << ", ATM [" << atm << "], "
                               << flatVol << ", " << flatNpv << ", " << strippedNpv << ", " << diff << ")");
        }
    }
}

BOOST_FIXTURE_TEST_CASE(testExtrapolation, CommonVars) {

    BOOST_TEST_MESSAGE("Testing extrapolation settings");

    // A shift of 1bp that will be used below
    Real shift = 0.001;

    // Pick one configuration and check that extrapolation works as expected
    Handle<OptionletVolatilityStructure> ovs =
        createOvs<LinearFlat, LinearFlat>(Normal, true, false, TermVolSurface::Bilinear, true, false);

    // Boundaries
    Date maxDate = ovs->maxDate();
    Rate minStrike = ovs->minStrike();
    Rate maxStrike = ovs->maxStrike();

    // Trivial check
    BOOST_REQUIRE(maxStrike > minStrike);

    // Asking for vol before reference date throws
    Date testDate = referenceDate - 1 * Days;
    Rate testStrike = (maxStrike + minStrike) / 2;
    BOOST_CHECK_THROW(ovs->volatility(testDate, testStrike, false), Error);
    BOOST_CHECK_THROW(ovs->volatility(testDate, testStrike, true), Error);

    // Check that asking for a volatility within the boundary does not throw
    testDate = referenceDate + 1 * Days;
    BOOST_CHECK_NO_THROW(ovs->volatility(testDate, testStrike, false));

    testDate = maxDate - 1 * Days;
    BOOST_CHECK_NO_THROW(ovs->volatility(testDate, testStrike, false));

    // Check that asking for a volatility outside the boundary throws
    testDate = maxDate + 1 * Days;
    BOOST_CHECK_THROW(ovs->volatility(testDate, testStrike, false), Error);

    testDate = referenceDate + 1 * Days;
    testStrike = minStrike - shift;
    BOOST_CHECK_THROW(ovs->volatility(testDate, testStrike, false), Error);

    testStrike = maxStrike + shift;
    BOOST_CHECK_THROW(ovs->volatility(testDate, testStrike, false), Error);

    // Check that asking for a volatility outside the boundary, with explicit extrapolation on, does not throw
    testDate = maxDate + 1 * Days;
    testStrike = (maxStrike + minStrike) / 2;
    BOOST_CHECK_NO_THROW(ovs->volatility(testDate, testStrike, true));

    testDate = referenceDate + 1 * Days;
    testStrike = minStrike - shift;
    BOOST_CHECK_NO_THROW(ovs->volatility(testDate, testStrike, true));

    testStrike = maxStrike + shift;
    BOOST_CHECK_NO_THROW(ovs->volatility(testDate, testStrike, true));

    // Check that asking for a volatility outside the boundary, after turning on extrapolation, does not throw
    ovs->enableExtrapolation();

    testDate = maxDate + 1 * Days;
    testStrike = (maxStrike + minStrike) / 2;
    BOOST_CHECK_NO_THROW(ovs->volatility(testDate, testStrike, false));

    testDate = referenceDate + 1 * Days;
    testStrike = minStrike - shift;
    BOOST_CHECK_NO_THROW(ovs->volatility(testDate, testStrike, false));

    testStrike = maxStrike + shift;
    BOOST_CHECK_NO_THROW(ovs->volatility(testDate, testStrike, false));
}

// Test cached values with LinearFlat time and smile interpolation
BOOST_FIXTURE_TEST_CASE(testCachedLinearFlat, CommonVars) {

    BOOST_TEST_MESSAGE("Testing against cached optionlet volatilities with LinearFlat time and smile interpolation");

    // Create the cap floor term vol surface
    QuantLib::ext::shared_ptr<TermVolSurface> cfts =
        QuantLib::ext::make_shared<TermVolSurface>(referenceDate, calendar, bdc, testVols.tenors, testVols.strikes,
                                           testVols.nVols, dayCounter, TermVolSurface::Bilinear);

    // Create the piecewise optionlet stripper
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> pwos = QuantLib::ext::make_shared<PiecewiseOptionletStripper<LinearFlat> >(
        cfts, iborIndex, testYieldCurves.discountEonia, true, Normal, 0.0, Normal);

    // Create the OptionletVolatilityStructure
    Handle<OptionletVolatilityStructure> ovs = Handle<OptionletVolatilityStructure>(
        QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat> >(referenceDate, pwos));
    ovs->enableExtrapolation();

    // Check optionlet fixing dates against cached fixing dates
    BOOST_CHECK_EQUAL_COLLECTIONS(cachedOptionletFixingDates.begin(), cachedOptionletFixingDates.end(),
                                  pwos->optionletFixingDates().begin(), pwos->optionletFixingDates().end());

    // Check cached optionlet values at optionlet fixing dates
    Matrix cached(cachedOptionletFixingDates.size(), cachedStrikes.size(), cachedValues.begin(), cachedValues.end());
    BOOST_TEST_MESSAGE("Optionlet volatilities at the fixing dates");
    BOOST_TEST_MESSAGE("date,strike,volatility,cached,diff");
    for (Size i = 0; i < pwos->optionletFixingDates().size(); i++) {
        for (Size j = 0; j < cachedStrikes.size(); j++) {
            Date d = pwos->optionletFixingDates()[i];
            Rate s = cachedStrikes[j];
            Volatility v = ovs->volatility(d, s);
            Volatility diff = fabs(v - cached[i][j]);
            BOOST_CHECK_SMALL(diff, tolerance);
            BOOST_TEST_MESSAGE(io::iso_date(d) << "," << s << "," << fixed << setprecision(12) << v << ","
                                               << cached[i][j] << "," << diff);
        }
    }

    // Check cached optionlet values at ad-hoc dates
    cached = Matrix(cachedAdHocDates.size(), cachedStrikes.size(), cachedAdHocValues.begin(), cachedAdHocValues.end());
    BOOST_TEST_MESSAGE("Optionlet volatilities at the ad-hoc dates");
    BOOST_TEST_MESSAGE("date,strike,volatility,cached,diff");
    for (Size i = 0; i < cachedAdHocDates.size(); i++) {
        for (Size j = 0; j < cachedStrikes.size(); j++) {
            Date d = cachedAdHocDates[i];
            Rate s = cachedStrikes[j];
            Volatility v = ovs->volatility(d, s);
            Volatility diff = fabs(v - cached[i][j]);
            BOOST_CHECK_SMALL(diff, tolerance);
            BOOST_TEST_MESSAGE(io::iso_date(d) << "," << s << "," << fixed << setprecision(12) << v << ","
                                               << cached[i][j] << "," << diff);
        }
    }
}

BOOST_FIXTURE_TEST_CASE(testChangingCapFloorSurface, CommonVars) {

    BOOST_TEST_MESSAGE("Testing changing the input cap floor surface");

    // Take four normal volatilities from test data and create quotes
    Size lastTenorIdx = testVols.tenors.size() - 1;
    Size lastStrikeIdx = testVols.strikes.size() - 1;

    vector<Period> tenors = list_of(testVols.tenors[0])(testVols.tenors[lastTenorIdx]);
    vector<Rate> strikes = list_of(testVols.strikes[0])(testVols.strikes[lastStrikeIdx]);

    vector<vector<QuantLib::ext::shared_ptr<SimpleQuote> > > quotes(2);
    quotes[0].push_back(QuantLib::ext::make_shared<SimpleQuote>(testVols.nVols[0][0]));
    quotes[0].push_back(QuantLib::ext::make_shared<SimpleQuote>(testVols.nVols[0][lastStrikeIdx]));
    quotes[1].push_back(QuantLib::ext::make_shared<SimpleQuote>(testVols.nVols[lastTenorIdx][0]));
    quotes[1].push_back(QuantLib::ext::make_shared<SimpleQuote>(testVols.nVols[lastTenorIdx][lastStrikeIdx]));

    vector<vector<Handle<Quote> > > quoteHs(2);
    quoteHs[0].push_back(Handle<Quote>(quotes[0][0]));
    quoteHs[0].push_back(Handle<Quote>(quotes[0][1]));
    quoteHs[1].push_back(Handle<Quote>(quotes[1][0]));
    quoteHs[1].push_back(Handle<Quote>(quotes[1][1]));

    // Create the cap floor term vol surface using the quotes
    QuantLib::ext::shared_ptr<TermVolSurface> cfts = QuantLib::ext::make_shared<TermVolSurface>(
        settlementDays, calendar, bdc, tenors, strikes, quoteHs, dayCounter, TermVolSurface::Bilinear);

    // Create the piecewise optionlet stripper
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> pwos = QuantLib::ext::make_shared<PiecewiseOptionletStripper<LinearFlat> >(
        cfts, iborIndex, testYieldCurves.discountEonia, true, Normal, 0.0, Normal);

    // Create the OptionletVolatilityStructure
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ovs =
        QuantLib::ext::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat> >(pwos);
    ovs->enableExtrapolation();

    // Request optionlet volatility at test date
    Date testDate = pwos->optionletFixingDates().back();
    Volatility initialVol = ovs->volatility(testDate, strikes[0]);
    BOOST_TEST_MESSAGE("Test vol before shift is: " << fixed << setprecision(12) << initialVol);
    Volatility expInitialVol = 0.007941492816;
    BOOST_CHECK_SMALL(fabs(expInitialVol - initialVol), tolerance);

    // Shift the input quote and request the same optionlet volatility
    Rate shift = 1.1;
    quotes[1][0]->setValue(shift * quotes[1][0]->value());
    Volatility shiftedVol = ovs->volatility(testDate, strikes[0]);
    BOOST_TEST_MESSAGE("Test vol after shift is: " << fixed << setprecision(12) << shiftedVol);
    Volatility expShiftedVol = 0.008926338986;
    BOOST_CHECK_SMALL(fabs(expShiftedVol - shiftedVol), tolerance);

    // Reset the input quote
    quotes[1][0]->setValue(quotes[1][0]->value() / shift);
    initialVol = ovs->volatility(testDate, strikes[0]);
    BOOST_TEST_MESSAGE("Test vol after reset is: " << fixed << setprecision(12) << initialVol);
    BOOST_CHECK_SMALL(fabs(expInitialVol - initialVol), tolerance);

    // Change the evaluation date
    Date newDate = referenceDate + 1 * Weeks;
    Settings::instance().evaluationDate() = newDate;

    // Check that the optionlet volatility structure's reference date has moved
    // Only the case because we used a "moving" adapter
    BOOST_CHECK_EQUAL(ovs->referenceDate(), newDate);

    // Check that one of the optionlet fixing dates in the PiecewiseOptionletStripper has moved
    // Only the case because we used a "moving" cap floor volatility term surface as input
    Date newLastOptionletDate = pwos->optionletFixingDates().back();
    BOOST_TEST_MESSAGE("Last fixing date moved from " << io::iso_date(testDate) << " to "
                                                      << io::iso_date(newLastOptionletDate));
    BOOST_CHECK(newLastOptionletDate > testDate);

    // Check the newly calculated optionlet vol for the old test date
    Volatility newVol = ovs->volatility(testDate, strikes[0]);
    BOOST_TEST_MESSAGE("Test vol after moving evaluation date is: " << fixed << setprecision(12) << newVol);
    Volatility expNewVol = 0.007932365669;
    BOOST_CHECK_SMALL(fabs(expNewVol - newVol), tolerance);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
