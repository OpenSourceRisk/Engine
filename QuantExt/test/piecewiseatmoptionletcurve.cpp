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
#include <ql/tuple.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/termstructures/capfloorhelper.hpp>
#include <qle/termstructures/capfloortermvolcurve.hpp>
#include <qle/termstructures/piecewiseatmoptionletcurve.hpp>

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
          dayCounter(Actual365Fixed()), accuracy(1.0e-12), tolerance(1.0e-10) {

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

    // Test tolerance for comparing the NPVs
    Real tolerance;

    // Cap floor ibor index
    QuantLib::ext::shared_ptr<IborIndex> iborIndex;

    // EUR discount curve test data from file test/yieldcurvemarketdata.hpp
    YieldCurveEUR testYieldCurves;
};

// Struct to hold a cap floor volatility column and some associated meta data
struct AtmVolData {
    vector<Period> tenors;
    vector<Real> volatilities;
    VolatilityType type;
    Real displacement;
};

// Needed for data driven test case below as it writes out the VolatilityColumn
ostream& operator<<(ostream& os, const AtmVolData& vc) {
    return os << "Atm volatility data with volatility type: " << vc.type << ", shift: " << vc.displacement;
}

// From the EUR cap floor test volatility data in file test/capfloormarketdata.hpp, create a vector of
// AtmVolData which will be the data in the data driven test below
vector<AtmVolData> generateAtmVolData() {

    // Will hold the generated ATM volatility data
    vector<AtmVolData> data;

    // EUR cap floor test volatility data from file test/capfloormarketdata.hpp
    CapFloorVolatilityEUR testVols;

    // All the ATM data has shared tenors
    AtmVolData datum;
    datum.tenors = testVols.atmTenors;

    // The normal ATM volatility data
    datum.volatilities = testVols.nAtmVols;
    datum.type = Normal;
    datum.displacement = 0.0;
    data.push_back(datum);

    // The shifted lognormal volatilities with shift 1
    datum.volatilities = testVols.slnAtmVols_1;
    datum.type = ShiftedLognormal;
    datum.displacement = testVols.shift_1;
    data.push_back(datum);

    // The shifted lognormal volatilities with shift 2
    datum.volatilities = testVols.slnAtmVols_2;
    datum.type = ShiftedLognormal;
    datum.displacement = testVols.shift_2;
    data.push_back(datum);

    return data;
}

// Interpolation types for the data driven test case
typedef boost::variant<Linear, BackwardFlat, QuantExt::LinearFlat, Cubic, QuantExt::CubicFlat> InterpolationType;

// BackwardFlat does not work well with interpolation on term cap floors so this allows us to exclude it
class InterpDataset {
public:
    enum { arity = 1 };

    InterpDataset() {
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(Linear()), true));
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(QuantExt::LinearFlat()), true));
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(Cubic()), true));
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(QuantExt::CubicFlat()), true));
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(BackwardFlat()), true));
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(Linear()), false));
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(QuantExt::LinearFlat()), false));
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(Cubic()), false));
        samples_.push_back(QuantLib::ext::make_tuple(InterpolationType(QuantExt::CubicFlat()), false));
    }

    bdata::size_t size() const { return samples_.size(); }

    typedef vector<QuantLib::ext::tuple<InterpolationType, bool> >::const_iterator iterator;
    iterator begin() const { return samples_.begin(); }

private:
    vector<QuantLib::ext::tuple<InterpolationType, bool> > samples_;
};

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

} // namespace

// Needed for BOOST_DATA_TEST_CASE below
// https://stackoverflow.com/a/33965517/1771882
namespace boost {

namespace test_tools {
namespace tt_detail {
template <> struct print_log_value<QuantLib::ext::tuple<InterpolationType, bool> > {
    void operator()(ostream& os, const QuantLib::ext::tuple<InterpolationType, bool>& tp) {
        os << to_string(QuantLib::ext::get<0>(tp)) << ", " << boolalpha << QuantLib::ext::get<1>(tp);
    }
};
} // namespace tt_detail
} // namespace test_tools

namespace unit_test {
namespace data {
namespace monomorphic {
// Registering InterpDataset as a dataset
template <> struct is_dataset<InterpDataset> : boost::mpl::true_ {};
} // namespace monomorphic
} // namespace data
} // namespace unit_test

} // namespace boost

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PiecewiseAtmOptionletCurveTests)

BOOST_DATA_TEST_CASE_F(CommonVars, testPiecewiseAtmOptionletStripping,
                       bdata::make(generateAtmVolData()) * InterpDataset() * bdata::make(isMovingValues) *
                           bdata::make(flatFirstPeriodValues),
                       atmVolData, interpSample, isMoving, flatFirstPeriod) {

    BOOST_TEST_MESSAGE("Testing piecewise optionlet stripping of ATM cap floor curve");

    InterpolationType interpolationType = QuantLib::ext::get<0>(interpSample);
    bool interpOnOptionlets = QuantLib::ext::get<1>(interpSample);

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Quote volatility type: " << atmVolData.type);
    BOOST_TEST_MESSAGE("  Quote displacement: " << atmVolData.displacement);
    BOOST_TEST_MESSAGE("  Interpolation type: " << to_string(interpolationType));
    BOOST_TEST_MESSAGE("  Interp on optionlets: " << boolalpha << interpOnOptionlets);
    BOOST_TEST_MESSAGE("  Floating reference date: " << boolalpha << isMoving);
    BOOST_TEST_MESSAGE("  Flat first period: " << boolalpha << flatFirstPeriod);

    // Store each cap floor instrument in the strike column and its NPV using the flat cap floor volatilities
    vector<QuantLib::ext::shared_ptr<CapFloor> > instruments(atmVolData.tenors.size());
    vector<Real> flatNpvs(atmVolData.tenors.size());

    // Store the ATM volatility quotes and handles
    vector<QuantLib::ext::shared_ptr<SimpleQuote> > volQuotes(atmVolData.tenors.size());
    vector<Handle<Quote> > volHandles(atmVolData.tenors.size());

    BOOST_TEST_MESSAGE("The input values at each tenor are:");
    for (Size i = 0; i < atmVolData.tenors.size(); i++) {

        // Get the relevant quote value
        Real volatility = atmVolData.volatilities[i];
        volQuotes[i] = QuantLib::ext::make_shared<SimpleQuote>(volatility);
        volHandles[i] = Handle<Quote>(volQuotes[i]);

        // Create the ATM cap instrument and store its price using the quoted flat volatility
        instruments[i] = MakeCapFloor(CapFloor::Cap, atmVolData.tenors[i], iborIndex, 0.01);
        Rate atm = instruments[i]->atmRate(**testYieldCurves.discountEonia);
        instruments[i] = MakeCapFloor(CapFloor::Cap, atmVolData.tenors[i], iborIndex, atm);
        if (atmVolData.type == ShiftedLognormal) {
            instruments[i]->setPricingEngine(QuantLib::ext::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter, atmVolData.displacement));
        } else {
            instruments[i]->setPricingEngine(
                QuantLib::ext::make_shared<BachelierCapFloorEngine>(testYieldCurves.discountEonia, volatility, dayCounter));
        }
        flatNpvs[i] = instruments[i]->NPV();

        BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV) = (Cap, "
                           << atmVolData.tenors[i] << ", " << atm << ", " << fixed << setprecision(13) << volatility
                           << ", " << flatNpvs[i] << ")");
    }

    // Create the ATM optionlet curve, with the given interpolation type.
    // Doesn't matter what interpolation we use here for the cap floor term surface so just use the same
    // interpolation as the optionlet curve being stripped
    VolatilityType curveVolatilityType = Normal;
    Real curveDisplacement = 0.0;
    QuantLib::ext::shared_ptr<CapFloorTermVolCurve> cftvc;
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ovCurve;
    switch (interpolationType.which()) {
    case 0:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using Linear interpolation with a moving reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<Linear> >(
                settlementDays, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        } else {
            BOOST_TEST_MESSAGE("Using Linear interpolation with a fixed reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Linear> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<Linear> >(
                referenceDate, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        }
        break;
    case 1:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using BackwardFlat interpolation with a moving reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<BackwardFlat> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<BackwardFlat> >(
                settlementDays, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        } else {
            BOOST_TEST_MESSAGE("Using BackwardFlat interpolation with a fixed reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<BackwardFlat> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<BackwardFlat> >(
                referenceDate, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        }
        break;
    case 2:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using LinearFlat interpolation with a moving reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<LinearFlat> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<LinearFlat> >(
                settlementDays, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        } else {
            BOOST_TEST_MESSAGE("Using LinearFlat interpolation with a fixed reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<LinearFlat> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<LinearFlat> >(
                referenceDate, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        }
        break;
    case 3:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using Cubic interpolation with a moving reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<Cubic> >(
                settlementDays, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        } else {
            BOOST_TEST_MESSAGE("Using Cubic interpolation with a fixed reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<Cubic> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<Cubic> >(
                referenceDate, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        }
        break;
    case 4:
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using CubicFlat interpolation with a moving reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<CubicFlat> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<CubicFlat> >(
                settlementDays, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        } else {
            BOOST_TEST_MESSAGE("Using CubicFlat interpolation with a fixed reference date");
            cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<CubicFlat> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<CubicFlat> >(
                referenceDate, cftvc, iborIndex, testYieldCurves.discountEonia, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement, interpOnOptionlets);
        }
        break;
    default:
        BOOST_FAIL("Unexpected interpolation type");
    }
    Handle<OptionletVolatilityStructure> hovs(ovCurve);

    // Price each cap floor instrument using the piecewise optionlet curve and check it against the flat NPV
    BOOST_TEST_MESSAGE("The stripped values and differences at each tenor are:");
    Real strippedNpv;
    for (Size i = 0; i < atmVolData.tenors.size(); i++) {

        // Price the instrument using the stripped optionlet structure
        if (ovCurve->volatilityType() == ShiftedLognormal) {
            instruments[i]->setPricingEngine(
                QuantLib::ext::make_shared<BlackCapFloorEngine>(testYieldCurves.discountEonia, hovs));
        } else {
            instruments[i]->setPricingEngine(
                QuantLib::ext::make_shared<BachelierCapFloorEngine>(testYieldCurves.discountEonia, hovs));
        }
        strippedNpv = instruments[i]->NPV();

        BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = ("
                           << instruments[i]->type() << ", " << atmVolData.tenors[i] << ", "
                           << instruments[i]->capRates()[0] << ", " << fixed << setprecision(13)
                           << atmVolData.volatilities[i] << ", " << flatNpvs[i] << ", " << strippedNpv << ", "
                           << (flatNpvs[i] - strippedNpv) << ")");

        BOOST_CHECK_SMALL(fabs(flatNpvs[i] - strippedNpv), tolerance);
    }

    // Pick 10Y (arbitrary choice - 5th element) ATM vol, bump it and ensure stripping still works.
    BOOST_TEST_MESSAGE("Testing that stripping still works after bumping volatility quote");
    Size idx = 4;
    Volatility bumpedVol = volQuotes[idx]->value() * 1.10;
    volQuotes[idx]->setValue(bumpedVol);
    strippedNpv = instruments[idx]->NPV();
    if (atmVolData.type == ShiftedLognormal) {
        instruments[idx]->setPricingEngine(QuantLib::ext::make_shared<BlackCapFloorEngine>(
            testYieldCurves.discountEonia, bumpedVol, dayCounter, atmVolData.displacement));
    } else {
        instruments[idx]->setPricingEngine(
            QuantLib::ext::make_shared<BachelierCapFloorEngine>(testYieldCurves.discountEonia, bumpedVol, dayCounter));
    }
    Real newFlatNpv = instruments[idx]->NPV();
    BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = ("
                       << instruments[idx]->type() << ", " << atmVolData.tenors[idx] << ", "
                       << instruments[idx]->capRates()[0] << ", " << fixed << setprecision(13) << bumpedVol << ", "
                       << newFlatNpv << ", " << strippedNpv << ", " << (newFlatNpv - strippedNpv) << ")");
    BOOST_CHECK_GT(newFlatNpv, flatNpvs[idx]);
    BOOST_CHECK_SMALL(fabs(newFlatNpv - strippedNpv), tolerance);

    BOOST_TEST_MESSAGE("Test extrapolation settings with out of range date");
    Date oorDate = ovCurve->maxDate() + 1 * Months;
    BOOST_CHECK_NO_THROW(ovCurve->volatility(oorDate, 0.01, true));
    BOOST_CHECK_THROW(ovCurve->volatility(oorDate, 0.01), Error);
    ovCurve->enableExtrapolation();
    BOOST_CHECK_NO_THROW(ovCurve->volatility(oorDate, 0.01));

    BOOST_TEST_MESSAGE("Test term structure stripping still works after changing evaluation date");
    Date newDate = calendar.advance(referenceDate, 1 * Months);
    Settings::instance().evaluationDate() = newDate;

    for (Size i = 0; i < atmVolData.tenors.size(); i++) {

        Real volatility = volQuotes[i]->value();

        // Cap floor set up is different depending on whether we are testing the moving term structure or not.
        // Empty startDate, i.e. moving, means that cap floor will be relative to the global evaluation date.
        // If not moving, we keep the instrument's original start date.
        Date startDate;
        if (!isMoving) {
            startDate = iborIndex->fixingCalendar().adjust(referenceDate);
            startDate = iborIndex->fixingCalendar().advance(startDate, iborIndex->fixingDays() * Days);
        }
        instruments[i] =
            MakeCapFloor(CapFloor::Cap, atmVolData.tenors[i], iborIndex, 0.01).withEffectiveDate(startDate, true);
        Rate atm = instruments[i]->atmRate(**testYieldCurves.discountEonia);
        instruments[i] =
            MakeCapFloor(CapFloor::Cap, atmVolData.tenors[i], iborIndex, atm).withEffectiveDate(startDate, true);

        if (atmVolData.type == ShiftedLognormal) {
            instruments[i]->setPricingEngine(QuantLib::ext::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter, atmVolData.displacement));
        } else {
            instruments[i]->setPricingEngine(
                QuantLib::ext::make_shared<BachelierCapFloorEngine>(testYieldCurves.discountEonia, volatility, dayCounter));
        }
        newFlatNpv = instruments[i]->NPV();

        // Price the instrument using the stripped optionlet structure
        if (ovCurve->volatilityType() == ShiftedLognormal) {
            instruments[i]->setPricingEngine(
                QuantLib::ext::make_shared<BlackCapFloorEngine>(testYieldCurves.discountEonia, hovs));
        } else {
            instruments[i]->setPricingEngine(
                QuantLib::ext::make_shared<BachelierCapFloorEngine>(testYieldCurves.discountEonia, hovs));
        }
        strippedNpv = instruments[i]->NPV();

        BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = ("
                           << instruments[i]->type() << ", " << atmVolData.tenors[i] << ", "
                           << instruments[i]->capRates()[0] << ", " << fixed << setprecision(13)
                           << volQuotes[i]->value() << ", " << newFlatNpv << ", " << strippedNpv << ", "
                           << (newFlatNpv - strippedNpv) << ")");

        BOOST_CHECK_SMALL(fabs(newFlatNpv - strippedNpv), tolerance);
    }
}

BOOST_FIXTURE_TEST_CASE(testAtmStrippingExceptions, CommonVars) {

    BOOST_TEST_MESSAGE("Testing ATM stripping exception behaviour");

    // Use the normal ATM volatility test data
    CapFloorVolatilityEUR testVols;
    vector<Period> tenors = testVols.atmTenors;
    vector<Real> volatilities = testVols.nAtmVols;
    VolatilityType type = Normal;
    Real displacement = 0.0;

    // Store the ATM volatility quotes and handles
    vector<QuantLib::ext::shared_ptr<SimpleQuote> > volQuotes(tenors.size());
    vector<Handle<Quote> > volHandles(tenors.size());
    for (Size i = 0; i < tenors.size(); i++) {
        Real volatility = volatilities[i];
        volQuotes[i] = QuantLib::ext::make_shared<SimpleQuote>(volatility);
        volHandles[i] = Handle<Quote>(volQuotes[i]);
    }

    // Get configuration values for bootstrap
    Real globalAccuracy = 1e-10;
    bool dontThrow = false;

    // Cap floor term curve
    QuantLib::ext::shared_ptr<CapFloorTermVolCurve> cftvc = QuantLib::ext::make_shared<InterpolatedCapFloorTermVolCurve<LinearFlat> >(
        settlementDays, calendar, bdc, tenors, volHandles, dayCounter, true);

    // Piecewise curve
    VolatilityType curveVolatilityType = Normal;
    Real curveDisplacement = 0.0;
    bool interpOnOptionlets = true;
    QuantLib::ext::shared_ptr<OptionletVolatilityStructure> ovCurve =
        QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<LinearFlat> >(
            settlementDays, cftvc, iborIndex, testYieldCurves.discountEonia, true, type, displacement,
            curveVolatilityType, curveDisplacement, interpOnOptionlets, LinearFlat(),
            QuantExt::IterativeBootstrap<
                PiecewiseAtmOptionletCurve<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
                accuracy, globalAccuracy, dontThrow));

    // Checks
    Period oneYear = 1 * Years;
    Period fiveYears = 5 * Years;
    Period eightYears = 8 * Years;
    Volatility oneYearVol;
    Volatility fiveYearVol;
    Volatility eightYearVol;
    BOOST_CHECK_NO_THROW(oneYearVol = ovCurve->volatility(oneYear, 0.01, true));
    BOOST_TEST_MESSAGE("1Y vol: " << oneYearVol);
    BOOST_CHECK_NO_THROW(fiveYearVol = ovCurve->volatility(fiveYears, 0.01, true));
    BOOST_TEST_MESSAGE("5Y vol: " << fiveYearVol);
    BOOST_CHECK_NO_THROW(eightYearVol = ovCurve->volatility(eightYears, 0.01, true));
    BOOST_TEST_MESSAGE("8Y vol: " << eightYearVol);

    // Increase the 5Y volatility to introduce an exception. Can't bootstrap the 7Y and 10Y caps as a result
    volQuotes[2]->setValue(2 * volQuotes[2]->value());
    BOOST_CHECK_THROW(ovCurve->volatility(fiveYears, 0.01, true), Error);

    // Rebuild ovCurve with dontThrow set to true
    dontThrow = true;
    ovCurve = QuantLib::ext::make_shared<PiecewiseAtmOptionletCurve<LinearFlat> >(
        settlementDays, cftvc, iborIndex, testYieldCurves.discountEonia, true, type, displacement, curveVolatilityType,
        curveDisplacement, interpOnOptionlets, LinearFlat(),
        QuantExt::IterativeBootstrap<
            PiecewiseAtmOptionletCurve<LinearFlat, QuantExt::IterativeBootstrap>::optionlet_curve>(
            accuracy, globalAccuracy, dontThrow));

    // Check bootstrap passes and check the values.
    // - the 1Y optionlet volatility should not have been affected
    // - the 5Y optionlet volatility should have increased
    Volatility testVol = 0.0;
    BOOST_CHECK_NO_THROW(testVol = ovCurve->volatility(oneYear, 0.01, true));
    BOOST_CHECK_SMALL(fabs(testVol - oneYearVol), tolerance);
    BOOST_TEST_MESSAGE("1Y vol after bump using previous: " << testVol);
    BOOST_CHECK_NO_THROW(testVol = ovCurve->volatility(fiveYears, 0.01, true));
    BOOST_CHECK_GT(testVol, fiveYearVol);
    BOOST_TEST_MESSAGE("5Y vol after bump using previous: " << testVol);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
