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
#include <boost/assign/list_of.hpp>
#include <boost/typeof/typeof.hpp>

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
using std::ostream;
using boost::assign::list_of;

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
struct VolatilityColumn {
    Real strike;
    vector<Period> tenors;
    vector<Real> volatilities;
    VolatilityType type;
    Real displacement;
};

// Needed for data driven test case below as it writes out the VolatilityColumn
ostream& operator<<(ostream& os, const VolatilityColumn& vc) {
    return os << "Column with strike: " << vc.strike << ", volatility type: " << 
        vc.type << ", shift: " << vc.displacement;
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

vector<CapFloor::Type> capFloorTypes = list_of(CapFloor::Cap)(CapFloor::Floor);

vector<CapFloorHelper::QuoteType> quoteTypes = list_of(CapFloorHelper::Volatility)(CapFloorHelper::Premium);

typedef boost::variant<Linear, BackwardFlat> InterpolationType;
vector<InterpolationType> interpolationTypes = list_of(InterpolationType(Linear()))(InterpolationType(BackwardFlat()));

}

// Needed for BOOST_DATA_TEST_CASE below
// https://stackoverflow.com/a/33965517/1771882
namespace boost {
namespace test_tools {
namespace tt_detail {
template <> struct print_log_value<InterpolationType> {
    void operator()(std::ostream& os, const InterpolationType& interpolationType) {
        switch (interpolationType.which()) {
        case 0:
            os << "Linear";
            break;
        case 1:
            os << "BackwardFlat";
            break;
        default:
            BOOST_FAIL("Unexpected interpolation type");
        }
    }
};
}
}
}

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PiecewiseOptionletCurveTests)

BOOST_DATA_TEST_CASE_F(CommonVars, testPiecewiseOptionletStripping, 
    bdata::make(generateVolatilityColumns()) * bdata::make(capFloorTypes) * bdata::make(quoteTypes) * bdata::make(interpolationTypes),
    volatilityColumn, capFloorType, quoteType, interpolationType) {

    BOOST_TEST_MESSAGE("Testing piecewise optionlet stripping of cap floor quotes along a strike column");

    BOOST_TEST_MESSAGE("Test inputs are:");
    BOOST_TEST_MESSAGE("  Cap floor type: " << capFloorType);
    BOOST_TEST_MESSAGE("  Cap floor strike: " << volatilityColumn.strike);
    BOOST_TEST_MESSAGE("  Quote type: " << quoteType);
    if (quoteType == CapFloorHelper::Volatility) {
        BOOST_TEST_MESSAGE("  Quote volatility type: " << volatilityColumn.type);
        BOOST_TEST_MESSAGE("  Quote displacement: " << volatilityColumn.displacement);
    }

    // Form the cap floor helper instrument for each tenor in the strike column
    vector<boost::shared_ptr<helper> > helpers(volatilityColumn.tenors.size());
    
    // Store each cap floor instrument in the strike column and its NPV using the flat cap floor volatilities
    vector<boost::shared_ptr<CapFloor> > instruments(volatilityColumn.tenors.size());
    vector<Real> flatNpvs(volatilityColumn.tenors.size());
    
    BOOST_TEST_MESSAGE("The input values at each tenor are:");
    for (Size i = 0; i < volatilityColumn.tenors.size(); i++) {
        
        // Get the relevant quote value
        Real volatility = volatilityColumn.volatilities[i];

        // Create the cap floor instrument and store its price using the quoted flat volatility
        instruments[i] = MakeCapFloor(capFloorType, volatilityColumn.tenors[i], iborIndex, volatilityColumn.strike);
        if (volatilityColumn.type == ShiftedLognormal) {
            instruments[i]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter, volatilityColumn.displacement));
        } else {
            instruments[i]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                testYieldCurves.discountEonia, volatility, dayCounter));
        }
        flatNpvs[i] = instruments[i]->NPV();
        
        BOOST_TEST_MESSAGE("  (Tenor, Volatility, Flat NPV) = (" << volatilityColumn.tenors[i] <<
            ", " << volatility << ", " << flatNpvs[i] << ")");

        // Create a volatility or premium quote
        RelinkableHandle<Quote> quote;
        if (quoteType == CapFloorHelper::Volatility) {
            quote.linkTo(boost::make_shared<SimpleQuote>(volatility));
        } else {
            quote.linkTo(boost::make_shared<SimpleQuote>(flatNpvs[i]));
        }
        
        // Create the helper instrument
        helpers[i] = boost::make_shared<CapFloorHelper>(capFloorType, volatilityColumn.tenors[i], volatilityColumn.strike,
            quote, iborIndex, testYieldCurves.discountEonia, quoteType, volatilityColumn.type, volatilityColumn.displacement);
    }

    // Create the piecewise optionlet curve, with the given interpolation type, and fail if it is not created
    VolatilityType curveVolatilityType = Normal;
    Real curveDisplacement = 0.0;
    boost::shared_ptr<OptionletVolatilityStructure> ovCurve;
    switch (interpolationType.which()) {
    case 0:
        BOOST_TEST_MESSAGE("Using Linear interpolation");
        BOOST_REQUIRE_NO_THROW(ovCurve = boost::make_shared<PiecewiseOptionletCurve<BOOST_TYPEOF(boost::get<Linear>(interpolationType))> >(
            referenceDate, helpers, calendar, bdc, dayCounter, curveVolatilityType, curveDisplacement, accuracy));
        break;
    case 1:
        BOOST_TEST_MESSAGE("Using BackwardFlat interpolation");
        BOOST_REQUIRE_NO_THROW(ovCurve = boost::make_shared<PiecewiseOptionletCurve<BOOST_TYPEOF(boost::get<BackwardFlat>(interpolationType))> >(
            referenceDate, helpers, calendar, bdc, dayCounter, curveVolatilityType, curveDisplacement, accuracy));
        break;
    default:
        BOOST_FAIL("Unexpected interpolation type");
    }
    Handle<OptionletVolatilityStructure> hovs(ovCurve);

    // Price each cap floor instrument using the piecewise optionlet curve and check it against the flat NPV
    BOOST_TEST_MESSAGE("The stripped values and differences at each tenor are:");
    Real strippedNpv;
    for (Size i = 0; i < volatilityColumn.tenors.size(); i++) {
        if (ovCurve->volatilityType() == ShiftedLognormal) {
            instruments[i]->setPricingEngine(boost::make_shared<BlackCapFloorEngine>(
                testYieldCurves.discountEonia, hovs));
        } else {
            instruments[i]->setPricingEngine(boost::make_shared<BachelierCapFloorEngine>(
                testYieldCurves.discountEonia, hovs));
        }
        strippedNpv = instruments[i]->NPV();

        BOOST_TEST_MESSAGE("  (Tenor, Volatility, Flat NPV, Stripped NPV, Flat - Stripped) = (" << volatilityColumn.tenors[i] <<
            ", " << volatilityColumn.volatilities[i] << ", " << flatNpvs[i] << ", " << strippedNpv << ", " <<
            (flatNpvs[i] - strippedNpv) << ")");

        BOOST_CHECK_SMALL(fabs(flatNpvs[i] - strippedNpv), tolerance);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
