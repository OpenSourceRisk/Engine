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
#include <ql/time/date.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/capfloorhelper.hpp>
#include <qle/termstructures/piecewiseoptionletcurve.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace bdata = boost::unit_test::data;
typedef QuantLib::BootstrapHelper<QuantLib::OptionletVolatilityStructure> helper;
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
};

// Struct to hold a cap floor volatility column and some associated meta data
struct VolatilityColumn {
    Real strike;
    vector<Period> tenors;
    vector<Real> volatilities;
    VolatilityType type;
    Real displacement;
};

// Needed for data driven test case below as it writes out the VolatilityColumn
ostream& operator<<(ostream& os, const VolatilityColumn& vc) {
    return os << "Column with strike: " << vc.strike << ", volatility type: " << vc.type
              << ", shift: " << vc.displacement;
}

// From the EUR cap floor test volatility data in file test/capfloormarketdata.hpp, create a vector of
// VolatilityColumns which will be the data in the data driven test below
vector<VolatilityColumn> generateVolatilityColumns() {

    // Will hold the generated volatility columns
    vector<VolatilityColumn> volatilityColumns;

    // EUR cap floor test volatility data from file test/capfloormarketdata.hpp
    CapFloorVolatilityEUR testVols;

    VolatilityColumn volatilityColumn;
    vector<Real> volatilities(testVols.tenors.size());
    volatilityColumn.tenors = testVols.tenors;

    // The normal volatilities
    volatilityColumn.type = Normal;
    volatilityColumn.displacement = 0.0;
    for (Size j = 0; j < testVols.strikes.size(); j++) {
        for (Size i = 0; i < testVols.tenors.size(); i++) {
            volatilities[i] = testVols.nVols[i][j];
        }
        volatilityColumn.strike = testVols.strikes[j];
        volatilityColumn.volatilities = volatilities;
        volatilityColumns.push_back(volatilityColumn);
    }

    // The shifted lognormal volatilities with shift 1
    volatilityColumn.type = ShiftedLognormal;
    volatilityColumn.displacement = testVols.shift_1;
    for (Size j = 0; j < testVols.strikes.size(); j++) {
        for (Size i = 0; i < testVols.tenors.size(); i++) {
            volatilities[i] = testVols.slnVols_1[i][j];
        }
        volatilityColumn.strike = testVols.strikes[j];
        volatilityColumn.volatilities = volatilities;
        volatilityColumns.push_back(volatilityColumn);
    }

    // The shifted lognormal volatilities with shift 2
    volatilityColumn.type = ShiftedLognormal;
    volatilityColumn.displacement = testVols.shift_2;
    for (Size j = 0; j < testVols.strikes.size(); j++) {
        for (Size i = 0; i < testVols.tenors.size(); i++) {
            volatilities[i] = testVols.slnVols_2[i][j];
        }
        volatilityColumn.strike = testVols.strikes[j];
        volatilityColumn.volatilities = volatilities;
        volatilityColumns.push_back(volatilityColumn);
    }

    return volatilityColumns;
}

// Cap floor helper types for the data driven test case
vector<CapFloorHelper::Type> helperTypes =
    list_of(CapFloorHelper::Cap)(CapFloorHelper::Floor)(CapFloorHelper::Automatic);

// Quote types for the data driven test case
vector<CapFloorHelper::QuoteType> quoteTypes = list_of(CapFloorHelper::Volatility)(CapFloorHelper::Premium);

// Interpolation types for the data driven test case
typedef boost::variant<Linear, BackwardFlat, QuantExt::LinearFlat, Cubic, QuantExt::CubicFlat> InterpolationType;

vector<InterpolationType> interpolationTypes = list_of(InterpolationType(Linear()))(InterpolationType(BackwardFlat()))(
    InterpolationType(QuantExt::LinearFlat()))(InterpolationType(Cubic()))(InterpolationType(QuantExt::CubicFlat()));

vector<InterpolationType> interpolationTypesCached =
    list_of(InterpolationType(Linear()))(InterpolationType(BackwardFlat()))(InterpolationType(QuantExt::LinearFlat()));

// If the built optionlet structure in the test has a floating or fixed reference date
vector<bool> isMovingValues = list_of(true)(false);

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

// Cached values for comparison below. We compare against cached values for a given interpolation type and setting of
// the bool flatFirstPeriod. The cached values are keyed on the Size: 2 * interpolationType + flatFirstPeriod.
// clang-format off
vector<Date> cachedDates = list_of(Date(5, Feb, 2016))(Date(7, Feb, 2017))(Date(6, Aug, 2020))(Date(5, Aug, 2022))(Date(7, Aug, 2025))(Date(7, Aug, 2035));
map<Size, vector<Real> > cachedValues = map_list_of
    // Linear, flat first = false
    (0, list_of(0.000000000000)(0.009939243164)(0.008398540935)(0.008216105988)(0.006859464219)(0.006598726907).convert_to_container<vector<Real> >())
    // Linear, flat first = true
    (1, list_of(0.009938000000)(0.009938000000)(0.008399019469)(0.008215852284)(0.006859635836)(0.006598586367).convert_to_container<vector<Real> >())
    // BackwardFlat, flat first = false
    (2, list_of(0.000000000000)(0.009938000000)(0.008799306892)(0.008279139515)(0.007401656494)(0.006715983817).convert_to_container<vector<Real> >())
    // BackwardFlat, flat first = true
    (3, list_of(0.009938000000)(0.009938000000)(0.008799306892)(0.008279139515)(0.007401656494)(0.006715983817).convert_to_container<vector<Real> >())
    // LinearFlat, flat first = false
    (4, list_of(0.000000000000)(0.009939243164)(0.008398540935)(0.008216105988)(0.006859464219)(0.006598726907).convert_to_container<vector<Real> >())
    // LinearFlat, flat first = true
    (5, list_of(0.009938000000)(0.009938000000)(0.008399019469)(0.008215852284)(0.006859635836)(0.006598586367).convert_to_container<vector<Real> >());
// clang-format on

// Cached values, on dates that are not curve nodes, for comparison below.
// We pick a value in the first curve period to check the flatFirstPeriod setting, in the middle of the curve to check
// the interpolation and after the last curve date to check the extrapolation.
// clang-format off
vector<Date> cachedNonNodeDates = list_of(Date(5, Aug, 2016))(Date(6, Aug, 2021))(Date(7, Aug, 2036));
map<Size, vector<Real> > cachedNonNodeValues = map_list_of
    // Linear, flat first = false
    (0, list_of(0.004915603956)(0.008307198335)(0.006572596059).convert_to_container<vector<Real> >())
    // Linear, flat first = true
    (1, list_of(0.009938000000)(0.008307310247)(0.006572424235).convert_to_container<vector<Real> >())
    // BackwardFlat, flat first = false
    (2, list_of(0.009938000000)(0.008279139515)(0.006715983817).convert_to_container<vector<Real> >())
    // BackwardFlat, flat first = true
    (3, list_of(0.009938000000)(0.008279139515)(0.006715983817).convert_to_container<vector<Real> >())
    // LinearFlat, flat first = false
    (4, list_of(0.004915603956)(0.008307198335)(0.006598726907).convert_to_container<vector<Real> >())
    // LinearFlat, flat first = true
    (5, list_of(0.009938000000)(0.008307310247)(0.006598586367).convert_to_container<vector<Real> >());
// clang-format on

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

BOOST_AUTO_TEST_SUITE(PiecewiseOptionletCurveTests)

BOOST_DATA_TEST_CASE_F(CommonVars, testPiecewiseOptionletStripping,
                       bdata::make(generateVolatilityColumns()) * bdata::make(helperTypes) * bdata::make(quoteTypes) *
                           bdata::make(interpolationTypes) * bdata::make(isMovingValues) *
                           bdata::make(flatFirstPeriodValues),
                       volatilityColumn, helperType, quoteType, interpolationType, isMoving, flatFirstPeriod) {

    BOOST_TEST_MESSAGE("Testing piecewise optionlet stripping of cap floor quotes along a strike column");

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Cap floor helper type: " << helperType);
    BOOST_TEST_MESSAGE("  Cap floor strike: " << volatilityColumn.strike);
    BOOST_TEST_MESSAGE("  Quote type: " << quoteType);
    if (quoteType == CapFloorHelper::Volatility) {
        BOOST_TEST_MESSAGE("  Quote volatility type: " << volatilityColumn.type);
        BOOST_TEST_MESSAGE("  Quote displacement: " << volatilityColumn.displacement);
    }
    BOOST_TEST_MESSAGE("  Interpolation type: " << to_string(interpolationType));
    BOOST_TEST_MESSAGE("  Floating reference date: " << boolalpha << isMoving);
    BOOST_TEST_MESSAGE("  Flat first period: " << boolalpha << flatFirstPeriod);

    if (quoteType == CapFloorHelper::Premium && helperType == CapFloorHelper::Automatic) {

        // This is a combination that should throw an error when creating the helper
        // Don't care about the value of the premium quote
        Handle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
        BOOST_REQUIRE_THROW(
            QuantLib::ext::make_shared<CapFloorHelper>(helperType, volatilityColumn.tenors.front(), volatilityColumn.strike,
                                               quote, iborIndex, testYieldCurves.discountEonia, isMoving, Date(),
                                               quoteType, volatilityColumn.type, volatilityColumn.displacement),
            Error);

    } else {

        // Form the cap floor helper instrument for each tenor in the strike column
        vector<QuantLib::ext::shared_ptr<helper> > helpers(volatilityColumn.tenors.size());

        // Store each cap floor instrument in the strike column and its NPV using the flat cap floor volatilities
        vector<QuantLib::ext::shared_ptr<CapFloor> > instruments(volatilityColumn.tenors.size());
        vector<Real> flatNpvs(volatilityColumn.tenors.size());

        BOOST_TEST_MESSAGE("The input values at each tenor are:");
        for (Size i = 0; i < volatilityColumn.tenors.size(); i++) {

            // Get the relevant quote value
            Real volatility = volatilityColumn.volatilities[i];

            // Create the cap floor instrument and store its price using the quoted flat volatility
            CapFloor::Type capFloorType = CapFloor::Cap;
            if (helperType == CapFloorHelper::Floor) {
                capFloorType = CapFloor::Floor;
            }
            instruments[i] = MakeCapFloor(capFloorType, volatilityColumn.tenors[i], iborIndex, volatilityColumn.strike);
            if (volatilityColumn.type == ShiftedLognormal) {
                instruments[i]->setPricingEngine(QuantLib::ext::make_shared<BlackCapFloorEngine>(
                    testYieldCurves.discountEonia, volatility, dayCounter, volatilityColumn.displacement));
            } else {
                instruments[i]->setPricingEngine(
                    QuantLib::ext::make_shared<BachelierCapFloorEngine>(testYieldCurves.discountEonia, volatility, dayCounter));
            }
            flatNpvs[i] = instruments[i]->NPV();

            BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Volatility, Flat NPV) = ("
                               << capFloorType << ", " << volatilityColumn.tenors[i] << ", " << fixed
                               << setprecision(13) << volatility << ", " << flatNpvs[i] << ")");

            // Create a volatility or premium quote
            RelinkableHandle<Quote> quote;
            if (quoteType == CapFloorHelper::Volatility) {
                quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(volatility));
            } else {
                quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(flatNpvs[i]));
            }

            // Create the helper instrument
            helpers[i] =
                QuantLib::ext::make_shared<CapFloorHelper>(helperType, volatilityColumn.tenors[i], volatilityColumn.strike,
                                                   quote, iborIndex, testYieldCurves.discountEonia, isMoving, Date(),
                                                   quoteType, volatilityColumn.type, volatilityColumn.displacement);
        }

        // Create the piecewise optionlet curve, with the given interpolation type, and fail if it is not created
        VolatilityType curveVolatilityType = Normal;
        Real curveDisplacement = 0.0;
        QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ovCurve;
        switch (interpolationType.which()) {
        case 0:
            if (isMoving) {
                BOOST_TEST_MESSAGE("Using Linear interpolation with a moving reference date");
                BOOST_REQUIRE_NO_THROW(ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<Linear> >(
                                           settlementDays, helpers, calendar, bdc, dayCounter, curveVolatilityType,
                                           curveDisplacement, flatFirstPeriod));
            } else {
                BOOST_TEST_MESSAGE("Using Linear interpolation with a fixed reference date");
                BOOST_REQUIRE_NO_THROW(ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<Linear> >(
                                           referenceDate, helpers, calendar, bdc, dayCounter, curveVolatilityType,
                                           curveDisplacement, flatFirstPeriod));
            }
            break;
        case 1:
            if (isMoving) {
                BOOST_TEST_MESSAGE("Using BackwardFlat interpolation with a moving reference date");
                BOOST_REQUIRE_NO_THROW(ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<BackwardFlat> >(
                                           settlementDays, helpers, calendar, bdc, dayCounter, curveVolatilityType,
                                           curveDisplacement, flatFirstPeriod));
            } else {
                BOOST_TEST_MESSAGE("Using BackwardFlat interpolation with a fixed reference date");
                BOOST_REQUIRE_NO_THROW(ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<BackwardFlat> >(
                                           referenceDate, helpers, calendar, bdc, dayCounter, curveVolatilityType,
                                           curveDisplacement, flatFirstPeriod));
            }
            break;
        case 2:
            if (isMoving) {
                BOOST_TEST_MESSAGE("Using LinearFlat interpolation with a moving reference date");
                BOOST_REQUIRE_NO_THROW(ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<LinearFlat> >(
                                           settlementDays, helpers, calendar, bdc, dayCounter, curveVolatilityType,
                                           curveDisplacement, flatFirstPeriod));
            } else {
                BOOST_TEST_MESSAGE("Using LinearFlat interpolation with a fixed reference date");
                BOOST_REQUIRE_NO_THROW(ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<LinearFlat> >(
                                           referenceDate, helpers, calendar, bdc, dayCounter, curveVolatilityType,
                                           curveDisplacement, flatFirstPeriod));
            }
            break;
        case 3:
            if (isMoving) {
                BOOST_TEST_MESSAGE("Using Cubic interpolation with a moving reference date");
                BOOST_REQUIRE_NO_THROW(
                    ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<Cubic> >(
                        settlementDays, helpers, calendar, bdc, dayCounter, curveVolatilityType, curveDisplacement,
                        flatFirstPeriod, Cubic(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseOptionletCurve<Cubic, QuantExt::IterativeBootstrap>::this_curve>(accuracy,
                                                                                                      globalAccuracy)));
            } else {
                BOOST_TEST_MESSAGE("Using Cubic interpolation with a fixed reference date");
                BOOST_REQUIRE_NO_THROW(
                    ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<Cubic> >(
                        referenceDate, helpers, calendar, bdc, dayCounter, curveVolatilityType, curveDisplacement,
                        flatFirstPeriod, Cubic(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseOptionletCurve<Cubic, QuantExt::IterativeBootstrap>::this_curve>(accuracy,
                                                                                                      globalAccuracy)));
            }
            break;
        case 4:
            if (isMoving) {
                BOOST_TEST_MESSAGE("Using CubicFlat interpolation with a moving reference date");
                BOOST_REQUIRE_NO_THROW(
                    ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<CubicFlat> >(
                        settlementDays, helpers, calendar, bdc, dayCounter, curveVolatilityType, curveDisplacement,
                        flatFirstPeriod, CubicFlat(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseOptionletCurve<CubicFlat, QuantExt::IterativeBootstrap>::this_curve>(
                            accuracy, globalAccuracy)));
            } else {
                BOOST_TEST_MESSAGE("Using CubicFlat interpolation with a fixed reference date");
                BOOST_REQUIRE_NO_THROW(
                    ovCurve = QuantLib::ext::make_shared<PiecewiseOptionletCurve<CubicFlat> >(
                        referenceDate, helpers, calendar, bdc, dayCounter, curveVolatilityType, curveDisplacement,
                        flatFirstPeriod, CubicFlat(),
                        QuantExt::IterativeBootstrap<
                            PiecewiseOptionletCurve<CubicFlat, QuantExt::IterativeBootstrap>::this_curve>(
                            accuracy, globalAccuracy)));
            }
            break;
        default:
            BOOST_FAIL("Unexpected interpolation type");
        }
        Handle<OptionletVolatilityStructure> hovs(ovCurve);

        // Price each cap floor instrument using the piecewise optionlet curve and check it against the flat NPV
        BOOST_TEST_MESSAGE("The stripped values and differences at each tenor are:");
        Real strippedNpv;
        for (Size i = 0; i < volatilityColumn.tenors.size(); i++) {

            // May need to update instruments type if it is being chosen automatically in the bootstrap
            if (helperType == CapFloorHelper::Automatic && quoteType != CapFloorHelper::Premium) {
                Real volatility = volatilityColumn.volatilities[i];
                CapFloor::Type capFloorType =
                    QuantLib::ext::dynamic_pointer_cast<CapFloorHelper>(helpers[i])->capFloor()->type();
                if (capFloorType != instruments[i]->type()) {
                    // Need to update the instrument and the flat NPV for the test
                    instruments[i] =
                        MakeCapFloor(capFloorType, volatilityColumn.tenors[i], iborIndex, volatilityColumn.strike);
                    if (volatilityColumn.type == ShiftedLognormal) {
                        instruments[i]->setPricingEngine(QuantLib::ext::make_shared<BlackCapFloorEngine>(
                            testYieldCurves.discountEonia, volatility, dayCounter, volatilityColumn.displacement));
                    } else {
                        instruments[i]->setPricingEngine(QuantLib::ext::make_shared<BachelierCapFloorEngine>(
                            testYieldCurves.discountEonia, volatility, dayCounter));
                    }
                    flatNpvs[i] = instruments[i]->NPV();
                }
            }

            // Price the instrument using the stripped optionlet structure
            if (ovCurve->volatilityType() == ShiftedLognormal) {
                instruments[i]->setPricingEngine(
                    QuantLib::ext::make_shared<BlackCapFloorEngine>(testYieldCurves.discountEonia, hovs));
            } else {
                instruments[i]->setPricingEngine(
                    QuantLib::ext::make_shared<BachelierCapFloorEngine>(testYieldCurves.discountEonia, hovs));
            }
            strippedNpv = instruments[i]->NPV();

            BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = ("
                               << instruments[i]->type() << ", " << volatilityColumn.tenors[i] << ", " << fixed
                               << setprecision(13) << volatilityColumn.volatilities[i] << ", " << flatNpvs[i] << ", "
                               << strippedNpv << ", " << (flatNpvs[i] - strippedNpv) << ")");

            BOOST_CHECK_SMALL(fabs(flatNpvs[i] - strippedNpv), tolerance);
        }
    }
}

BOOST_DATA_TEST_CASE_F(CommonVars, testCachedValues,
                       bdata::make(interpolationTypesCached) * bdata::make(flatFirstPeriodValues), interpolationType,
                       flatFirstPeriod) {

    BOOST_TEST_MESSAGE("Testing stripping of single strike column against cached values");

    CapFloorHelper::Type helperType = CapFloorHelper::Automatic;
    CapFloorHelper::QuoteType quoteType = CapFloorHelper::Volatility;

    // Use EUR cap floor test volatility data from file test/capfloormarketdata.hpp
    // Take the highest strike column of the normal volatility matrix
    CapFloorVolatilityEUR testVols;
    vector<Period> tenors = testVols.tenors;
    vector<Real> volatilities(tenors.size());
    Size strikeIdx = testVols.strikes.size() - 1;
    Rate strike = testVols.strikes[strikeIdx];
    for (Size i = 0; i < tenors.size(); i++) {
        volatilities[i] = testVols.nVols[i][strikeIdx];
    }
    VolatilityType volatilityType = Normal;
    Real displacement = 0.0;

    // Make first tenor 18M highlight differences introduced by flatFirstPeriod setting.
    // With first tenor = 1Y and index tenor = 6M, we were not seeing a difference.
    tenors[0] = 18 * Months;

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Cap floor helper type: " << helperType);
    BOOST_TEST_MESSAGE("  Cap floor strike: " << strike);
    BOOST_TEST_MESSAGE("  Quote type: " << quoteType);
    BOOST_TEST_MESSAGE("  Quote volatility type: " << volatilityType);
    BOOST_TEST_MESSAGE("  Quote displacement: " << displacement);
    BOOST_TEST_MESSAGE("  Interpolation type: " << to_string(interpolationType));
    BOOST_TEST_MESSAGE("  Flat first period: " << boolalpha << flatFirstPeriod);

    // Form the cap floor helper instrument for each tenor in the strike column
    vector<QuantLib::ext::shared_ptr<helper> > helpers(tenors.size());
    for (Size i = 0; i < tenors.size(); i++) {
        Handle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(volatilities[i]));
        helpers[i] = QuantLib::ext::make_shared<CapFloorHelper>(helperType, tenors[i], strike, quote, iborIndex,
                                                        testYieldCurves.discountEonia, true, Date(), quoteType,
                                                        volatilityType, displacement);
    }

    // Create the piecewise optionlet curve, with the given interpolation type.
    // Store the nodes of this optionlet curve to compare with cached values
    VolatilityType curveVolatilityType = Normal;
    Real curveDisplacement = 0.0;
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ovs;
    vector<pair<Date, Real> > curveNodes;
    switch (interpolationType.which()) {
    case 0: {
        BOOST_TEST_MESSAGE("Using Linear interpolation");
        QuantLib::ext::shared_ptr<PiecewiseOptionletCurve<Linear> > ovCurve =
            QuantLib::ext::make_shared<PiecewiseOptionletCurve<Linear> >(referenceDate, helpers, calendar, bdc, dayCounter,
                                                                 curveVolatilityType, curveDisplacement,
                                                                 flatFirstPeriod);
        curveNodes = ovCurve->nodes();
        ovs = ovCurve;
    } break;
    case 1: {
        BOOST_TEST_MESSAGE("Using BackwardFlat interpolation");
        QuantLib::ext::shared_ptr<PiecewiseOptionletCurve<BackwardFlat> > ovCurve =
            QuantLib::ext::make_shared<PiecewiseOptionletCurve<BackwardFlat> >(referenceDate, helpers, calendar, bdc,
                                                                       dayCounter, curveVolatilityType,
                                                                       curveDisplacement, flatFirstPeriod);
        curveNodes = ovCurve->nodes();
        ovs = ovCurve;
    } break;
    case 2: {
        BOOST_TEST_MESSAGE("Using LinearFlat interpolation");
        QuantLib::ext::shared_ptr<PiecewiseOptionletCurve<LinearFlat> > ovCurve =
            QuantLib::ext::make_shared<PiecewiseOptionletCurve<LinearFlat> >(referenceDate, helpers, calendar, bdc, dayCounter,
                                                                     curveVolatilityType, curveDisplacement,
                                                                     flatFirstPeriod);
        curveNodes = ovCurve->nodes();
        ovs = ovCurve;
    } break;
    default:
        BOOST_FAIL("Unexpected interpolation type");
    }

    // Get the key for the cached results for the current test
    Size key = 2 * interpolationType.which() + static_cast<Size>(flatFirstPeriod);

    // Check stripped optionlet volatilities against cached values
    BOOST_REQUIRE_EQUAL(curveNodes.size(), cachedDates.size());
    BOOST_REQUIRE(cachedValues.count(key) == 1);
    BOOST_REQUIRE_EQUAL(curveNodes.size(), cachedValues.at(key).size());
    BOOST_TEST_MESSAGE("node_date,node_vol");
    for (Size i = 0; i < curveNodes.size(); i++) {
        // Check the date
        BOOST_CHECK_EQUAL(curveNodes[i].first, cachedDates[i]);
        // Check the value
        BOOST_CHECK_SMALL(fabs(curveNodes[i].second - cachedValues.at(key)[i]), accuracy);
        // Print out the curve
        BOOST_TEST_MESSAGE(io::iso_date(curveNodes[i].first)
                           << "," << fixed << setprecision(12) << curveNodes[i].second);
    }

    // Check stripped optionlet volatilities on non-node dates against cached values
    BOOST_REQUIRE(cachedNonNodeValues.count(key) == 1);
    BOOST_TEST_MESSAGE("date,vol,cached_vol,diff");
    for (Size i = 0; i < cachedNonNodeDates.size(); i++) {
        Date d = cachedNonNodeDates[i];

        // The last date has been picked past the maximum curve date to check extrapolation. Check that we get an
        // error and then turn on extrapolation.
        if (i == cachedNonNodeDates.size() - 1) {
            BOOST_CHECK_THROW(ovs->volatility(d, strike), Error);
            ovs->enableExtrapolation();
        }

        Volatility vol = ovs->volatility(d, strike);
        Volatility cachedVol = cachedNonNodeValues.at(key)[i];
        Volatility diff = fabs(vol - cachedVol);
        // Check the value
        BOOST_CHECK_SMALL(diff, accuracy);
        // Print out the curve
        BOOST_TEST_MESSAGE(io::iso_date(d)
                           << "," << fixed << setprecision(12) << vol << "," << cachedVol << "," << diff);

        // The strike should not matter so check that the same test passes above and below the strike
        Real shift = 0.0010;
        diff = fabs(ovs->volatility(d, strike - shift) - cachedVol);
        BOOST_CHECK_SMALL(diff, accuracy);
        diff = fabs(ovs->volatility(d, strike + shift) - cachedVol);
        BOOST_CHECK_SMALL(diff, accuracy);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
