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

#include "optionletstripper.hpp"
#include "capfloormarketdata.hpp"
#include "yieldcurvemarketdata.hpp"

#include <qle/termstructures/optionletstripper1.hpp>
#include <qle/termstructures/optionletstripper2.hpp>

#include <ql/instruments/makecapfloor.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolcurve.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletadapter.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace {
// Variables to be used in the test
struct CommonVars {
    // Constructor
    CommonVars() : referenceDate(5, Feb, 2016), settlementDays(0), calendar(TARGET()), bdc(Following), 
        dayCounter(Actual365Fixed()), accuracy(1.0e-6), maxIter(100) {
        // Reference date
        Settings::instance().evaluationDate() = referenceDate;
        // Cap floor ibor index
        iborIndex = boost::make_shared<Euribor6M>(yieldCurves.forward6M);
    }

    // Members
    Date referenceDate;
    
    // Some common conventions for our cap floor curve and surface construction
    Natural settlementDays;
    Calendar calendar;
    BusinessDayConvention bdc;
    DayCounter dayCounter;
    
    // Accuracy and max iterations for optionlet stripping
    Real accuracy;
    Natural maxIter;

    boost::shared_ptr<IborIndex> iborIndex;
    CapFloorVolatilityEUR vols;
    YieldCurveEUR yieldCurves;

    SavedSettings backup;
};
}

namespace testsuite {

void OptionletStripperTest::testUsualNormalStripping() {
    BOOST_TEST_MESSAGE("Testing standard stripping of normal capfloor vols...");

    CommonVars vars;

    // EUR cap floor normal volatility surface
    boost::shared_ptr<CapFloorTermVolSurface> volSurface = boost::make_shared<CapFloorTermVolSurface>(
        vars.settlementDays, vars.calendar, vars.bdc, vars.vols.tenors, vars.vols.strikes, 
        vars.vols.nVols, vars.dayCounter);

    // Create Normal stripped optionlet surface and Normal engine
    boost::shared_ptr<OptionletStripper> stripper = boost::make_shared<OptionletStripper1>(volSurface, vars.iborIndex, 
        Null<Rate>(), vars.accuracy, vars.maxIter, vars.yieldCurves.discountEonia, Normal);
    boost::shared_ptr<StrippedOptionletAdapter> adapter = boost::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();
    boost::shared_ptr<BachelierCapFloorEngine> engine = 
        boost::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/foor surface and stripped optionlet surface and compare price
    boost::shared_ptr<CapFloor> cap;
    
    RelinkableHandle<Quote> quote(boost::make_shared<SimpleQuote>(0.0));
    boost::shared_ptr<BachelierCapFloorEngine> flatEngine =
        boost::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, quote, vars.dayCounter);
    
    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, 
                vars.vols.strikes[j], vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(boost::make_shared<SimpleQuote>(vars.vols.nVols[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy, 
                "\noption tenor:       " << vars.vols.tenors[i] <<
                "\nstrike:             " << io::rate(vars.vols.strikes[j]) <<
                "\nstripped vol price: " << io::rate(strippedPrice) <<
                "\nconstant vol price: " << io::rate(flatPrice) <<
                "\nerror:              " << io::rate(error) <<
                "\ntolerance:          " << io::rate(vars.accuracy)
            );
        }
    }
}

void OptionletStripperTest::testUsualNormalStrippingWithAtm() {
    BOOST_TEST_MESSAGE("Testing standard stripping of normal capfloor vols with overlayed ATM curve...");

    CommonVars vars;

    // EUR cap floor normal volatility surface
    boost::shared_ptr<CapFloorTermVolSurface> volSurface = boost::make_shared<CapFloorTermVolSurface>(
        vars.settlementDays, vars.calendar, vars.bdc, vars.vols.tenors, vars.vols.strikes,
        vars.vols.nVols, vars.dayCounter);
    
    // EUR cap floor normal ATM curve
    Handle<CapFloorTermVolCurve> atmVolCurve(boost::make_shared<CapFloorTermVolCurve>(vars.settlementDays, 
        vars.calendar, vars.bdc, vars.vols.atmTenors, vars.vols.nAtmVols, vars.dayCounter));

    // Create Normal stripped optionlet surface
    boost::shared_ptr<OptionletStripper1> tempStripper = boost::make_shared<OptionletStripper1>(volSurface, 
        vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter, vars.yieldCurves.discountEonia, Normal);

    // Overlay normal ATM curve
    boost::shared_ptr<OptionletStripper> stripper = boost::make_shared<OptionletStripper2>(
        tempStripper, atmVolCurve, Normal);

    boost::shared_ptr<StrippedOptionletAdapter> adapter = boost::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();

    // Create engine
    boost::shared_ptr<BachelierCapFloorEngine> engine =
        boost::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/foor surface and stripped optionlet surface and compare price
    boost::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(boost::make_shared<SimpleQuote>(0.0));
    boost::shared_ptr<BachelierCapFloorEngine> flatEngine =
        boost::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, quote, vars.dayCounter);

    // Non-ATM pillar points: check flat cap/foor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, 
                vars.vols.strikes[j], vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(boost::make_shared<SimpleQuote>(vars.vols.nVols[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy, 
                "\noption tenor:       " << vars.vols.tenors[i] <<
                "\nstrike:             " << io::rate(vars.vols.strikes[j]) <<
                "\nstripped vol price: " << io::rate(strippedPrice) <<
                "\nconstant vol price: " << io::rate(flatPrice) <<
                "\nerror:              " << io::rate(error) <<
                "\ntolerance:          " << io::rate(vars.accuracy)
            );
        }
    }

    // ATM pillar points: check flat cap/foor surface price = stripped optionlet surface price
    Volatility dummyVol = 0.10;
    boost::shared_ptr<BlackCapFloorEngine> tempEngine =
        boost::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, dummyVol);
    
    for (Size i = 0; i < vars.vols.atmTenors.size(); ++i) {
        // Null strike => cap is set up with ATM strike
        // Need a temp BlackCapFloorEngine for ATM strike to be calculated (bad - should be fixed in QL)
        cap = MakeCapFloor(CapFloor::Cap, vars.vols.atmTenors[i], vars.iborIndex, 
            Null<Rate>(), vars.settlementDays * Days).withPricingEngine(tempEngine);

        cap->setPricingEngine(engine);
        Real strippedPrice = cap->NPV();

        quote.linkTo(boost::make_shared<SimpleQuote>(vars.vols.nAtmVols[i]));
        cap->setPricingEngine(flatEngine);
        Real flatPrice = cap->NPV();

        Real error = std::fabs(strippedPrice - flatPrice);
        BOOST_CHECK_MESSAGE(error < vars.accuracy, 
            "\noption tenor:       " << vars.vols.atmTenors[i] <<
            "\natm strike:         " << io::rate(cap->atmRate(**vars.yieldCurves.discountEonia)) <<
            "\nstripped vol price: " << io::rate(strippedPrice) <<
            "\nconstant vol price: " << io::rate(flatPrice) <<
            "\nerror:              " << io::rate(error) <<
            "\ntolerance:          " << io::rate(vars.accuracy)
        );
    }
}

test_suite* OptionletStripperTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("OptionletStripperTests");

    suite->add(BOOST_TEST_CASE(&OptionletStripperTest::testUsualNormalStripping));
    suite->add(BOOST_TEST_CASE(&OptionletStripperTest::testUsualNormalStrippingWithAtm));

    return suite;
}
}
