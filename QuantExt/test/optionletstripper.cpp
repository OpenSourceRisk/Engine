/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include "capfloormarketdata.hpp"
#include "yieldcurvemarketdata.hpp"

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <qle/termstructures/optionletstripper1.hpp>
#include <qle/termstructures/optionletstripper2.hpp>

#include <ql/instruments/makecapfloor.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/termstructures/volatility/capfloor/capfloortermvolcurve.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletadapter.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

using namespace QuantExt;
using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace {
// Variables to be used in the test
struct CommonVars {
    // Constructor
    CommonVars()
        : referenceDate(5, Feb, 2016), settlementDays(0), calendar(TARGET()), bdc(Following),
          dayCounter(Actual365Fixed()), accuracy(1.0e-6), maxIter(100) {
        // Reference date
        Settings::instance().evaluationDate() = referenceDate;
        // Cap floor ibor index
        iborIndex = QuantLib::ext::make_shared<Euribor6M>(yieldCurves.forward6M);
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

    QuantLib::ext::shared_ptr<IborIndex> iborIndex;
    CapFloorVolatilityEUR vols;
    YieldCurveEUR yieldCurves;

    SavedSettings backup;
};
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(OptionletStripperTest)

BOOST_AUTO_TEST_CASE(testUsualNormalStripping) {
    BOOST_TEST_MESSAGE("Testing standard stripping of normal capfloor vols...");

    CommonVars vars;

    // EUR cap floor normal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.nVols,
                                                             vars.dayCounter);

    // Create Normal stripped optionlet surface and Normal engine
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                               vars.yieldCurves.discountEonia, Normal));
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> engine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> flatEngine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, quote, vars.dayCounter);

    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.nVols[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }
}

BOOST_AUTO_TEST_CASE(testUsualShiftedLognormalStripping) {
    BOOST_TEST_MESSAGE("Testing standard stripping of shifted lognormal capfloor vols...");

    CommonVars vars;

    // EUR cap floor shifted lognormal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.slnVols_1,
                                                             vars.dayCounter);

    // Create shifted lognormal stripped optionlet surface and shifted lognormal engine
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new QuantExt::OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                                         vars.yieldCurves.discountEonia, ShiftedLognormal, vars.vols.shift_1));
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> engine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> flatEngine = QuantLib::ext::make_shared<BlackCapFloorEngine>(
        vars.yieldCurves.discountEonia, quote, vars.dayCounter, vars.vols.shift_1);

    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnVols_1[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }
}

BOOST_AUTO_TEST_CASE(testNormalToShiftedLognormalStripping) {
    BOOST_TEST_MESSAGE("Testing stripping of normal capfloor vols to give shifted lognormal optionlet vols...");

    CommonVars vars;

    // EUR cap floor normal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.nVols,
                                                             vars.dayCounter);

    // Create shifted lognormal stripped optionlet surface and Black engine
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                               vars.yieldCurves.discountEonia, Normal, 0.0, ShiftedLognormal, vars.vols.shift_1));
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> engine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> flatEngine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, quote, vars.dayCounter);

    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.nVols[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }
}

BOOST_AUTO_TEST_CASE(testShiftedLognormalToNormalStripping) {
    BOOST_TEST_MESSAGE("Testing stripping of shifted lognormal capfloor vols to give normal optionlet vols...");

    CommonVars vars;

    // EUR cap floor shifted lognormal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.slnVols_2,
                                                             vars.dayCounter);

    // Create normal stripped optionlet surface and Bachelier engine
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                               vars.yieldCurves.discountEonia, ShiftedLognormal, vars.vols.shift_2, Normal, 0.0));
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> engine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> flatEngine = QuantLib::ext::make_shared<BlackCapFloorEngine>(
        vars.yieldCurves.discountEonia, quote, vars.dayCounter, vars.vols.shift_2);

    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnVols_2[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }
}

BOOST_AUTO_TEST_CASE(testShiftedLognormalToShiftedLognormalStripping) {
    BOOST_TEST_MESSAGE("Testing stripping with shifted lognormal vols with different shifts...");

    CommonVars vars;

    // EUR cap floor shifted lognormal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.slnVols_2,
                                                             vars.dayCounter);

    // Create shifted lognormal stripped optionlet surface and shifted lognormal engine with different shift
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper =
        QuantLib::ext::shared_ptr<OptionletStripper1>(new OptionletStripper1(
            volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter, vars.yieldCurves.discountEonia,
            ShiftedLognormal, vars.vols.shift_2, ShiftedLognormal, vars.vols.shift_1));
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> engine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> flatEngine = QuantLib::ext::make_shared<BlackCapFloorEngine>(
        vars.yieldCurves.discountEonia, quote, vars.dayCounter, vars.vols.shift_2);

    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnVols_2[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }
}

BOOST_AUTO_TEST_CASE(testUsualNormalStrippingWithAtm) {
    BOOST_TEST_MESSAGE("Testing standard stripping of normal capfloor vols with overlayed ATM curve...");

    CommonVars vars;

    // EUR cap floor normal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.nVols,
                                                             vars.dayCounter);

    // EUR cap floor normal ATM curve
    Handle<CapFloorTermVolCurve> atmVolCurve(QuantLib::ext::make_shared<CapFloorTermVolCurve>(
        vars.settlementDays, vars.calendar, vars.bdc, vars.vols.atmTenors, vars.vols.nAtmVols, vars.dayCounter));

    // Create Normal stripped optionlet surface
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper1> tempStripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                               vars.yieldCurves.discountEonia, Normal));

    // Overlay normal ATM curve
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::make_shared<QuantExt::OptionletStripper2>(
        tempStripper, atmVolCurve, vars.yieldCurves.discountEonia, Normal);

    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();

    // Create engine
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> engine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> flatEngine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, quote, vars.dayCounter);

    // Non-ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.nVols[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }

    // ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    Volatility dummyVol = 0.10;
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> tempEngine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, dummyVol);

    for (Size i = 0; i < vars.vols.atmTenors.size(); ++i) {
        // Null strike => cap is set up with ATM strike
        // Need a temp BlackCapFloorEngine for ATM strike to be calculated (bad - should be fixed in QL)
        cap = MakeCapFloor(CapFloor::Cap, vars.vols.atmTenors[i], vars.iborIndex, Null<Rate>(),
                           vars.settlementDays * Days)
                  .withPricingEngine(tempEngine);

        cap->setPricingEngine(engine);
        Real strippedPrice = cap->NPV();

        quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.nAtmVols[i]));
        cap->setPricingEngine(flatEngine);
        Real flatPrice = cap->NPV();

        Real error = std::fabs(strippedPrice - flatPrice);
        BOOST_CHECK_MESSAGE(error < vars.accuracy,
                            "\noption tenor:       " << vars.vols.atmTenors[i] << "\natm strike:         "
                                                     << io::rate(cap->atmRate(**vars.yieldCurves.discountEonia))
                                                     << "\nstripped vol price: " << io::rate(strippedPrice)
                                                     << "\nconstant vol price: " << io::rate(flatPrice)
                                                     << "\nerror:              " << io::rate(error)
                                                     << "\ntolerance:          " << io::rate(vars.accuracy));
    }
}

BOOST_AUTO_TEST_CASE(testUsualShiftedLognormalStrippingWithAtm) {
    BOOST_TEST_MESSAGE("Testing standard stripping of shifted lognormal capfloor vols with overlayed ATM curve...");

    CommonVars vars;

    // EUR cap floor shifted lognormal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.slnVols_2,
                                                             vars.dayCounter);

    // EUR cap floor shifted lognormal ATM curve
    Handle<CapFloorTermVolCurve> atmVolCurve(QuantLib::ext::make_shared<CapFloorTermVolCurve>(
        vars.settlementDays, vars.calendar, vars.bdc, vars.vols.atmTenors, vars.vols.slnAtmVols_2, vars.dayCounter));

    // Create shifted lognormal stripped optionlet surface
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper1> tempStripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new QuantExt::OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                                         vars.yieldCurves.discountEonia, ShiftedLognormal, vars.vols.shift_2));

    // Overlay shifted lognormal ATM curve
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::make_shared<QuantExt::OptionletStripper2>(
        tempStripper, atmVolCurve, vars.yieldCurves.discountEonia, ShiftedLognormal, vars.vols.shift_2);

    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();

    // Create engine
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> engine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> flatEngine = QuantLib::ext::make_shared<BlackCapFloorEngine>(
        vars.yieldCurves.discountEonia, quote, vars.dayCounter, vars.vols.shift_2);

    // Non-ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnVols_2[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }

    // ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.atmTenors.size(); ++i) {
        // Null strike => cap is set up with ATM strike
        cap = MakeCapFloor(CapFloor::Cap, vars.vols.atmTenors[i], vars.iborIndex, Null<Rate>(),
                           vars.settlementDays * Days)
                  .withPricingEngine(engine);
        Real strippedPrice = cap->NPV();

        quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnAtmVols_2[i]));
        cap->setPricingEngine(flatEngine);
        Real flatPrice = cap->NPV();

        Real error = std::fabs(strippedPrice - flatPrice);
        BOOST_CHECK_MESSAGE(error < vars.accuracy,
                            "\noption tenor:       " << vars.vols.atmTenors[i] << "\natm strike:         "
                                                     << io::rate(cap->atmRate(**vars.yieldCurves.discountEonia))
                                                     << "\nstripped vol price: " << io::rate(strippedPrice)
                                                     << "\nconstant vol price: " << io::rate(flatPrice)
                                                     << "\nerror:              " << io::rate(error)
                                                     << "\ntolerance:          " << io::rate(vars.accuracy));
    }
}

BOOST_AUTO_TEST_CASE(testNormalToShiftedLognormalStrippingWithAtm) {
    BOOST_TEST_MESSAGE("Testing stripping of normal capfloor vols with ATM to give shifted lognormal...");

    CommonVars vars;

    // EUR cap floor normal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.nVols,
                                                             vars.dayCounter);

    // EUR cap floor normal ATM curve
    Handle<CapFloorTermVolCurve> atmVolCurve(QuantLib::ext::make_shared<CapFloorTermVolCurve>(
        vars.settlementDays, vars.calendar, vars.bdc, vars.vols.atmTenors, vars.vols.nAtmVols, vars.dayCounter));

    // Create shifted lognormal stripped optionlet surface
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper1> tempStripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                               vars.yieldCurves.discountEonia, Normal, 0.0, ShiftedLognormal, vars.vols.shift_1));

    // Overlay shifted lognormal ATM curve
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::make_shared<QuantExt::OptionletStripper2>(
        tempStripper, atmVolCurve, vars.yieldCurves.discountEonia, Normal);

    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();

    // Create engine
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> engine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> flatEngine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, quote, vars.dayCounter);

    // Non-ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.nVols[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }

    // ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.atmTenors.size(); ++i) {
        // Null strike => cap is set up with ATM strike
        cap = MakeCapFloor(CapFloor::Cap, vars.vols.atmTenors[i], vars.iborIndex, Null<Rate>(),
                           vars.settlementDays * Days)
                  .withPricingEngine(engine);
        Real strippedPrice = cap->NPV();

        quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.nAtmVols[i]));
        cap->setPricingEngine(flatEngine);
        Real flatPrice = cap->NPV();

        Real error = std::fabs(strippedPrice - flatPrice);
        BOOST_CHECK_MESSAGE(error < vars.accuracy,
                            "\noption tenor:       " << vars.vols.atmTenors[i] << "\natm strike:         "
                                                     << io::rate(cap->atmRate(**vars.yieldCurves.discountEonia))
                                                     << "\nstripped vol price: " << io::rate(strippedPrice)
                                                     << "\nconstant vol price: " << io::rate(flatPrice)
                                                     << "\nerror:              " << io::rate(error)
                                                     << "\ntolerance:          " << io::rate(vars.accuracy));
    }
}

BOOST_AUTO_TEST_CASE(testShiftedLognormalToNormalStrippingWithAtm) {
    BOOST_TEST_MESSAGE("Testing stripping of shifted lognormal capfloor vols with ATM to give normal...");

    CommonVars vars;

    // EUR cap floor shifted lognormal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.slnVols_1,
                                                             vars.dayCounter);

    // EUR cap floor shifted lognormal ATM curve
    Handle<CapFloorTermVolCurve> atmVolCurve(QuantLib::ext::make_shared<CapFloorTermVolCurve>(
        vars.settlementDays, vars.calendar, vars.bdc, vars.vols.atmTenors, vars.vols.slnAtmVols_1, vars.dayCounter));

    // Create normal stripped optionlet surface
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper1> tempStripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                               vars.yieldCurves.discountEonia, ShiftedLognormal, vars.vols.shift_1, Normal, 0.0));

    // Overlay shifted lognormal ATM curve
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::make_shared<QuantExt::OptionletStripper2>(
        tempStripper, atmVolCurve, vars.yieldCurves.discountEonia, ShiftedLognormal, vars.vols.shift_1);

    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();

    // Create engine
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> engine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> flatEngine = QuantLib::ext::make_shared<BlackCapFloorEngine>(
        vars.yieldCurves.discountEonia, quote, vars.dayCounter, vars.vols.shift_1);

    // Non-ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnVols_1[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }

    // ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.atmTenors.size(); ++i) {
        quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnAtmVols_1[i]));
        // Null strike => cap is set up with ATM strike
        cap = MakeCapFloor(CapFloor::Cap, vars.vols.atmTenors[i], vars.iborIndex, Null<Rate>(),
                           vars.settlementDays * Days)
                  .withPricingEngine(flatEngine);
        Real flatPrice = cap->NPV();

        cap->setPricingEngine(engine);
        Real strippedPrice = cap->NPV();

        Real error = std::fabs(strippedPrice - flatPrice);
        BOOST_CHECK_MESSAGE(error < vars.accuracy,
                            "\noption tenor:       " << vars.vols.atmTenors[i] << "\natm strike:         "
                                                     << io::rate(cap->atmRate(**vars.yieldCurves.discountEonia))
                                                     << "\nstripped vol price: " << io::rate(strippedPrice)
                                                     << "\nconstant vol price: " << io::rate(flatPrice)
                                                     << "\nerror:              " << io::rate(error)
                                                     << "\ntolerance:          " << io::rate(vars.accuracy));
    }
}

BOOST_AUTO_TEST_CASE(testShiftedLognormalToShiftedLognormalStrippingWithAtm) {
    BOOST_TEST_MESSAGE("Testing stripping with shifted lognormal vols with ATM with different shifts...");

    CommonVars vars;

    // EUR cap floor shifted lognormal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.slnVols_1,
                                                             vars.dayCounter);

    // EUR cap floor shifted lognormal ATM curve
    Handle<CapFloorTermVolCurve> atmVolCurve(QuantLib::ext::make_shared<CapFloorTermVolCurve>(
        vars.settlementDays, vars.calendar, vars.bdc, vars.vols.atmTenors, vars.vols.slnAtmVols_1, vars.dayCounter));

    // Create shifted lognormal stripped optionlet surface
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper1> tempStripper =
        QuantLib::ext::shared_ptr<OptionletStripper1>(new OptionletStripper1(
            volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter, vars.yieldCurves.discountEonia,
            ShiftedLognormal, vars.vols.shift_1, ShiftedLognormal, vars.vols.shift_2));

    // Overlay shifted lognormal ATM curve
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::make_shared<QuantExt::OptionletStripper2>(
        tempStripper, atmVolCurve, vars.yieldCurves.discountEonia, ShiftedLognormal, vars.vols.shift_1);

    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();

    // Create engine
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> engine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> flatEngine = QuantLib::ext::make_shared<BlackCapFloorEngine>(
        vars.yieldCurves.discountEonia, quote, vars.dayCounter, vars.vols.shift_1);

    // Non-ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < vars.vols.strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], vars.iborIndex, vars.vols.strikes[j],
                               vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnVols_1[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(vars.vols.strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }

    // ATM pillar points: check flat cap/floor surface price = stripped optionlet surface price
    for (Size i = 0; i < vars.vols.atmTenors.size(); ++i) {
        quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vars.vols.slnAtmVols_1[i]));
        // Null strike => cap is set up with ATM strike
        cap = MakeCapFloor(CapFloor::Cap, vars.vols.atmTenors[i], vars.iborIndex, Null<Rate>(),
                           vars.settlementDays * Days)
                  .withPricingEngine(flatEngine);
        Real flatPrice = cap->NPV();

        cap->setPricingEngine(engine);
        Real strippedPrice = cap->NPV();

        Real error = std::fabs(strippedPrice - flatPrice);
        BOOST_CHECK_MESSAGE(error < vars.accuracy,
                            "\noption tenor:       " << vars.vols.atmTenors[i] << "\natm strike:         "
                                                     << io::rate(cap->atmRate(**vars.yieldCurves.discountEonia))
                                                     << "\nstripped vol price: " << io::rate(strippedPrice)
                                                     << "\nconstant vol price: " << io::rate(flatPrice)
                                                     << "\nerror:              " << io::rate(error)
                                                     << "\ntolerance:          " << io::rate(vars.accuracy));
    }
}

BOOST_AUTO_TEST_CASE(testNormalToLognormalGivesError) {
    BOOST_TEST_MESSAGE("Testing stripping of normal to give lognormal gives error (due to negative strike)...");

    CommonVars vars;

    // EUR cap floor normal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, vars.vols.strikes, vars.vols.nVols,
                                                             vars.dayCounter);

    // Create shifted lognormal stripped optionlet surface and Black engine
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new QuantExt::OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                                         vars.yieldCurves.discountEonia, Normal, 0.0, ShiftedLognormal, 0.0));

    // Error due to negative strike in input matrix
    BOOST_CHECK_THROW(stripper->recalculate(), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testNormalToLognormalModifiedGivesError) {
    BOOST_TEST_MESSAGE("Testing stripping of normal to give lognormal gives error (due to negative forward)...");

    CommonVars vars;

    // Create a reduced capfloor matrix with only positive strikes
    vector<Volatility> strikes(vars.vols.strikes.begin() + 2, vars.vols.strikes.end());
    Matrix vols(vars.vols.tenors.size(), strikes.size());
    for (Size i = 0; i < vols.rows(); ++i)
        for (Size j = 0; j < vols.columns(); ++j)
            vols[i][j] = vars.vols.nVols[i][j + 2];

    // EUR cap floor normal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, strikes, vols, vars.dayCounter);

    // Create shifted lognormal stripped optionlet surface and Black engine
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new QuantExt::OptionletStripper1(volSurface, vars.iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                                         vars.yieldCurves.discountEonia, Normal, 0.0, ShiftedLognormal, 0.0));

    // Error due to forward rates being negative
    BOOST_CHECK_THROW(stripper->recalculate(), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testNormalToLognormalWithPositiveForwards) {
    BOOST_TEST_MESSAGE("Testing stripping of normal to give lognormal when forwards are positive...");

    CommonVars vars;

    // Create a reduced capfloor matrix with only positive strikes
    vector<Volatility> strikes(vars.vols.strikes.begin() + 2, vars.vols.strikes.end());
    Matrix vols(vars.vols.tenors.size(), strikes.size());
    for (Size i = 0; i < vols.rows(); ++i)
        for (Size j = 0; j < vols.columns(); ++j)
            vols[i][j] = vars.vols.nVols[i][j + 2];

    // Link ibor index to shifted forward curve
    Handle<Quote> spread(QuantLib::ext::make_shared<SimpleQuote>(0.015));
    Handle<YieldTermStructure> shiftedForward(
        QuantLib::ext::make_shared<ZeroSpreadedTermStructure>(vars.yieldCurves.forward6M, spread));
    QuantLib::ext::shared_ptr<IborIndex> iborIndex = vars.iborIndex->clone(shiftedForward);

    // EUR cap floor normal volatility surface
    QuantLib::ext::shared_ptr<QuantExt::CapFloorTermVolSurface> volSurface =
        QuantLib::ext::make_shared<QuantExt::CapFloorTermVolSurfaceExact>(vars.settlementDays, vars.calendar, vars.bdc,
                                                             vars.vols.tenors, strikes, vols, vars.dayCounter);

    // Create shifted lognormal stripped optionlet surface and Black engine
    QuantLib::ext::shared_ptr<QuantExt::OptionletStripper> stripper = QuantLib::ext::shared_ptr<OptionletStripper1>(
        new QuantExt::OptionletStripper1(volSurface, iborIndex, Null<Rate>(), vars.accuracy, vars.maxIter,
                                         vars.yieldCurves.discountEonia, Normal, 0.0, ShiftedLognormal, 0.0));
    QuantLib::ext::shared_ptr<StrippedOptionletAdapter> adapter = QuantLib::ext::make_shared<StrippedOptionletAdapter>(stripper);
    Handle<OptionletVolatilityStructure> ovs(adapter);
    ovs->enableExtrapolation();
    QuantLib::ext::shared_ptr<BlackCapFloorEngine> engine =
        QuantLib::ext::make_shared<BlackCapFloorEngine>(vars.yieldCurves.discountEonia, ovs);

    // Price a cap at each pillar point with flat cap/floor surface and stripped optionlet surface and compare price
    QuantLib::ext::shared_ptr<CapFloor> cap;

    RelinkableHandle<Quote> quote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<BachelierCapFloorEngine> flatEngine =
        QuantLib::ext::make_shared<BachelierCapFloorEngine>(vars.yieldCurves.discountEonia, quote, vars.dayCounter);

    for (Size i = 0; i < vars.vols.tenors.size(); ++i) {
        for (Size j = 0; j < strikes.size(); ++j) {
            cap = MakeCapFloor(CapFloor::Cap, vars.vols.tenors[i], iborIndex, strikes[j], vars.settlementDays * Days);

            cap->setPricingEngine(engine);
            Real strippedPrice = cap->NPV();

            quote.linkTo(QuantLib::ext::make_shared<SimpleQuote>(vols[i][j]));
            cap->setPricingEngine(flatEngine);
            Real flatPrice = cap->NPV();

            Real error = std::fabs(strippedPrice - flatPrice);
            BOOST_CHECK_MESSAGE(error < vars.accuracy,
                                "\noption tenor:       " << vars.vols.tenors[i]
                                                         << "\nstrike:             " << io::rate(strikes[j])
                                                         << "\nstripped vol price: " << io::rate(strippedPrice)
                                                         << "\nconstant vol price: " << io::rate(flatPrice)
                                                         << "\nerror:              " << io::rate(error)
                                                         << "\ntolerance:          " << io::rate(vars.accuracy));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
