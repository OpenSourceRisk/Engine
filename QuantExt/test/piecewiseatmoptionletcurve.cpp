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

#include <test/capfloormarketdata.hpp>
#include <test/yieldcurvemarketdata.hpp>
#include <test/toplevelfixture.hpp>

#include <qle/termstructures/piecewiseatmoptionletcurve.hpp>
#include <qle/termstructures/capfloorhelper.hpp>
#include <qle/termstructures/capfloortermvolcurve.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/time/date.hpp>

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

// Struct to hold a cap floor volatility column and some associated meta data
struct AtmVolData {
    vector<Period> tenors;
    vector<Real> volatilities;
    VolatilityType type;
    Real displacement;
};

// Needed for data driven test case below as it writes out the VolatilityColumn
ostream& operator<<(ostream& os, const AtmVolData& vc) {
    return os << "Atm volatility data with volatility type: " << 
        vc.type << ", shift: " << vc.displacement;
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

vector<InterpolationType> interpolationTypes = list_of
    (InterpolationType(Linear()))
    (InterpolationType(BackwardFlat()))
    (InterpolationType(QuantExt::LinearFlat()))
    (InterpolationType(Cubic()))
    (InterpolationType(QuantExt::CubicFlat()));

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

BOOST_AUTO_TEST_SUITE(PiecewiseAtmOptionletCurveTests)

BOOST_DATA_TEST_CASE_F(CommonVars, testPiecewiseAtmOptionletStripping, 
    bdata::make(generateAtmVolData()) * bdata::make(interpolationTypes) * bdata::make(isMovingValues) * 
    bdata::make(flatFirstPeriodValues), atmVolData, interpolationType, isMoving, flatFirstPeriod) {

    BOOST_TEST_MESSAGE("Testing piecewise optionlet stripping of ATM cap floor curve");

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Quote volatility type: " << atmVolData.type);
    BOOST_TEST_MESSAGE("  Quote displacement: " << atmVolData.displacement);
    BOOST_TEST_MESSAGE("  Interpolation type: " << to_string(interpolationType));
    BOOST_TEST_MESSAGE("  Floating reference date: " << boolalpha << isMoving);
    BOOST_TEST_MESSAGE("  Flat first period: " << boolalpha << flatFirstPeriod);
        
    // Store each cap floor instrument in the strike column and its NPV using the flat cap floor volatilities
    vector<boost::shared_ptr<CapFloor> > instruments(atmVolData.tenors.size());
    vector<Real> flatNpvs(atmVolData.tenors.size());

    // Store the ATM volatility quotes and handles
    vector<boost::shared_ptr<SimpleQuote> > volQuotes(atmVolData.tenors.size());
    vector<Handle<Quote> > volHandles(atmVolData.tenors.size());

    BOOST_TEST_MESSAGE("The input values at each tenor are:");
    for (Size i = 0; i < atmVolData.tenors.size(); i++) {
        
        // Get the relevant quote value
        Real volatility = atmVolData.volatilities[i];
        volQuotes[i] = boost::make_shared<SimpleQuote>(volatility);
        volHandles[i] = Handle<Quote>(volQuotes[i]);

        // Create the ATM cap instrument and store its price using the quoted flat volatility
        instruments[i] = MakeCapFloor(CapFloor::Cap, atmVolData.tenors[i], iborIndex, 0.01);
        Rate atm = instruments[i]->atmRate(**testYieldCurves.discountEonia);
        instruments[i] = MakeCapFloor(CapFloor::Cap, atmVolData.tenors[i], iborIndex, atm);
        if (atmVolData.type == ShiftedLognormal) {
            instruments[i]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter, atmVolData.displacement));
        } else {
            instruments[i]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter));
        }
        flatNpvs[i] = instruments[i]->NPV();

        BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV) = (Cap, " << 
            atmVolData.tenors[i] << ", " << atm << ", " << fixed << setprecision(13) << volatility << 
            ", " << flatNpvs[i] << ")");
    }

    // Create the ATM optionlet curve, with the given interpolation type.
    // Doesn't matter what interpolation we use here for the cap floor term surface so just use the same 
    // interpolation as the optionlet curve being stripped
    VolatilityType curveVolatilityType = Normal;
    Real curveDisplacement = 0.0;
    boost::shared_ptr<OptionletVolatilityStructure> ovCurve;
    switch (interpolationType.which()) {
    case 0: {
        boost::shared_ptr<CapFloorTermVolCurve<Linear> > cftvc;
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using Linear interpolation with a moving reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<Linear> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<Linear, Linear> >(settlementDays, cftvc, 
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type, 
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        } else {
            BOOST_TEST_MESSAGE("Using Linear interpolation with a fixed reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<Linear> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<Linear, Linear> >(referenceDate, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        }
        break;
    }
    case 1: {
        boost::shared_ptr<CapFloorTermVolCurve<BackwardFlat> > cftvc;
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using BackwardFlat interpolation with a moving reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<BackwardFlat> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<BackwardFlat, BackwardFlat> >(settlementDays, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        } else {
            BOOST_TEST_MESSAGE("Using BackwardFlat interpolation with a fixed reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<BackwardFlat> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<BackwardFlat, BackwardFlat> >(referenceDate, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        }
        break;
    }
    case 2: {
        boost::shared_ptr<CapFloorTermVolCurve<LinearFlat> > cftvc;
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using LinearFlat interpolation with a moving reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<LinearFlat> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<LinearFlat, LinearFlat> >(settlementDays, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        } else {
            BOOST_TEST_MESSAGE("Using LinearFlat interpolation with a fixed reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<LinearFlat> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<LinearFlat, LinearFlat> >(referenceDate, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        }
        break;
    }
    case 3: {
        boost::shared_ptr<CapFloorTermVolCurve<Cubic> > cftvc;
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using Cubic interpolation with a moving reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<Cubic> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<Cubic, Cubic> >(settlementDays, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        } else {
            BOOST_TEST_MESSAGE("Using Cubic interpolation with a fixed reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<Cubic> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<Cubic, Cubic> >(referenceDate, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        }
        break;
    }
    case 4: {
        boost::shared_ptr<CapFloorTermVolCurve<CubicFlat> > cftvc;
        if (isMoving) {
            BOOST_TEST_MESSAGE("Using CubicFlat interpolation with a moving reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<CubicFlat> >(
                settlementDays, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<CubicFlat, CubicFlat> >(settlementDays, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        } else {
            BOOST_TEST_MESSAGE("Using CubicFlat interpolation with a fixed reference date");
            cftvc = boost::make_shared<CapFloorTermVolCurve<CubicFlat> >(
                referenceDate, calendar, bdc, atmVolData.tenors, volHandles, dayCounter, flatFirstPeriod);
            ovCurve = boost::make_shared<PiecewiseAtmOptionletCurve<CubicFlat, CubicFlat> >(referenceDate, cftvc,
                iborIndex, testYieldCurves.discountEonia, accuracy, flatFirstPeriod, atmVolData.type,
                atmVolData.displacement, curveVolatilityType, curveDisplacement);
        }
        break;
    }
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
            instruments[i]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, hovs));
        } else {
            instruments[i]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                testYieldCurves.discountEonia, hovs));
        }
        strippedNpv = instruments[i]->NPV();

        BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = (" <<
            instruments[i]->type() << ", " << atmVolData.tenors[i] << ", " << instruments[i]->capRates()[0] << ", " << 
            fixed << setprecision(13) << atmVolData.volatilities[i] << ", " << flatNpvs[i] << ", " << strippedNpv << 
            ", " << (flatNpvs[i] - strippedNpv) << ")");

        BOOST_CHECK_SMALL(fabs(flatNpvs[i] - strippedNpv), tolerance);
    }

    // Pick 10Y (arbitrary choice - 5th element) ATM vol, bump it and ensure stripping still works.
    BOOST_TEST_MESSAGE("Testing that stripping still works after bumping volatility quote");
    Size idx = 4;
    Volatility bumpedVol = volQuotes[idx]->value() * 1.10;
    volQuotes[idx]->setValue(bumpedVol);
    strippedNpv = instruments[idx]->NPV();
    if (atmVolData.type == ShiftedLognormal) {
        instruments[idx]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
            testYieldCurves.discountEonia, bumpedVol, dayCounter, atmVolData.displacement));
    } else {
        instruments[idx]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
            testYieldCurves.discountEonia, bumpedVol, dayCounter));
    }
    Real newFlatNpv = instruments[idx]->NPV();
    BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = (" <<
        instruments[idx]->type() << ", " << atmVolData.tenors[idx] << ", " << instruments[idx]->capRates()[0] << 
        ", " << fixed << setprecision(13) << bumpedVol << ", " << newFlatNpv << ", " <<
        strippedNpv << ", " << (newFlatNpv - strippedNpv) << ")");
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
        // If not moving, we keep the instrment's original start date.
        Date startDate;
        if (!isMoving) {
            startDate = iborIndex->fixingCalendar().adjust(referenceDate);
            startDate = iborIndex->fixingCalendar().advance(startDate, iborIndex->fixingDays() * Days);
        }
        instruments[i] = MakeCapFloor(CapFloor::Cap, atmVolData.tenors[i], iborIndex, 0.01)
            .withEffectiveDate(startDate, true);
        Rate atm = instruments[i]->atmRate(**testYieldCurves.discountEonia);
        instruments[i] = MakeCapFloor(CapFloor::Cap, atmVolData.tenors[i], iborIndex, atm)
            .withEffectiveDate(startDate, true);

        if (atmVolData.type == ShiftedLognormal) {
            instruments[i]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter, atmVolData.displacement));
        } else {
            instruments[i]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter));
        }
        newFlatNpv = instruments[i]->NPV();

        // Price the instrument using the stripped optionlet structure
        if (ovCurve->volatilityType() == ShiftedLognormal) {
            instruments[i]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, hovs));
        } else {
            instruments[i]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                testYieldCurves.discountEonia, hovs));
        }
        strippedNpv = instruments[i]->NPV();

        BOOST_TEST_MESSAGE("  (Cap/Floor, Tenor, Strike, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = (" <<
            instruments[i]->type() << ", " << atmVolData.tenors[i] << ", " << instruments[i]->capRates()[0] << ", " <<
            fixed << setprecision(13) << volQuotes[i]->value() << ", " << newFlatNpv << ", " << strippedNpv <<
            ", " << (newFlatNpv - strippedNpv) << ")");

        BOOST_CHECK_SMALL(fabs(newFlatNpv - strippedNpv), tolerance);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
