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

#include "toplevelfixture.hpp"
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using namespace std;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BlackVarianceSurfaceSparse)

BOOST_AUTO_TEST_CASE(testBlackVarianceSurface) {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceSurfaceSparse with market data...");

    SavedSettings backup;

    // using data from https://papers.ssrn.com/sol3/papers.cfm?abstract_id=1694972
    // Appendix A: Tables and Figures
    // Table 1: SX5E Implied Volatility Quotes

    Settings::instance().evaluationDate() = Date(1, Mar, 2010);
    Date today = Settings::instance().evaluationDate();

    Real spot = 2772.70;

    // vector of 12 times
    vector<Time> all_times({ 0.025, 0.101, 0.197, 0.274, 0.523, 0.772, 1.769, 2.267, 2.784, 3.781, 4.778, 5.774 });

    // strike (%) and vols.
    // Data is stored here as per the table (vector of vectors, first element is strike, then we have
    // vols. Empty cells are 0.0. This data is re-organised below
    vector<vector<Real> > volData(
        { { 51.31, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 33.66, 32.91 },
          { 58.64, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 31.78, 31.29, 30.08 },
          { 65.97, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 30.19, 29.76, 29.75 },
          { 73.30, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 28.63, 28.48, 28.48 },
          { 76.97, 00.00, 00.00, 00.00, 32.62, 30.79, 30.01, 28.43 },
          { 80.63, 00.00, 00.00, 00.00, 30.58, 29.36, 28.76, 27.53, 27.13, 27.11, 27.11, 27.22, 28.09 },
          { 84.30, 00.00, 00.00, 00.00, 28.87, 27.98, 27.50, 26.66 },
          { 86.13, 33.65 },
          { 87.96, 32.16, 29.06, 27.64, 27.17, 26.63, 26.37, 25.75, 25.55, 25.80, 25.85, 26.11, 26.93 },
          { 89.79, 30.43, 27.97, 26.72 },
          { 91.63, 28.80, 26.90, 25.78, 25.57, 25.31, 25.19, 24.97 },
          { 93.46, 27.24, 25.90, 24.89 },
          { 95.29, 25.86, 24.88, 24.05, 24.07, 24.04, 24.11, 24.18, 24.10, 24.48, 24.69, 25.01, 25.84 },
          { 97.12, 24.66, 23.90, 23.29 },
          { 98.96, 23.58, 23.00, 22.53, 22.69, 22.84, 22.99, 23.47 },
          { 100.79, 22.47, 22.13, 21.84 },
          { 102.62, 21.59, 21.40, 21.23, 21.42, 21.73, 21.98, 22.83, 22.75, 23.22, 23.84, 23.92, 24.86 },
          { 104.45, 20.91, 20.76, 20.69 },
          { 106.29, 20.56, 20.24, 20.25, 20.39, 20.74, 21.04, 22.13 },
          { 108.12, 20.45, 19.82, 19.84 },
          { 109.95, 20.25, 19.59, 19.44, 19.62, 19.88, 20.22, 21.51, 21.61, 22.19, 22.69, 23.05, 23.99 },
          { 111.78, 19.33, 19.29, 19.20 },
          { 113.62, 00.00, 00.00, 00.00, 19.02, 19.14, 19.50, 20.91 },
          { 117.28, 00.00, 00.00, 00.00, 18.85, 18.54, 18.88, 20.39, 20.58, 21.22, 21.86, 22.23, 23.21 },
          { 120.95, 00.00, 00.00, 00.00, 18.67, 18.11, 18.39, 19.90 },
          { 124.61, 00.00, 00.00, 00.00, 18.71, 17.85, 17.93, 19.45, 00.00, 20.54, 21.03, 21.64, 22.51 },
          { 131.94, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 19.88, 20.54, 21.05, 21.90 },
          { 139.27, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 19.30, 20.02, 20.54, 21.35 },
          { 146.60, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 00.00, 18.49, 19.64, 20.12 } });

    // the 3 vectors we pass into the vol term structure
    vector<Date> dates;
    vector<Real> strikes;
    vector<Volatility> vols;

    // populate them with the above table
    for (auto vd : volData) {
        Real strike = spot * vd[0];
        for (Size i = 1; i < vd.size(); i++) {
            Volatility vol = vd[i] / 100.0;
            if (vol > 0.0001) {
                Date d = today + int(all_times[i - 1] * 365); // Roughly correct
                // we have a triple
                dates.push_back(d);
                strikes.push_back(strike);
                vols.push_back(vol);
            }
        }
    }

    Calendar cal = TARGET();
    DayCounter dc = ActualActual(ActualActual::ISDA);

    auto surface = QuantLib::ext::make_shared<QuantExt::BlackVarianceSurfaceSparse>(today, cal, dates, strikes, vols, dc);

    // 1. Check that we recover all of the above inputs
    for (auto vd : volData) {
        Real strike = spot * vd[0];
        for (Size i = 1; i < vd.size(); i++) {
            Volatility expectedVol = vd[i] / 100.0;
            if (expectedVol > 0.0001) {
                Date d = today + int(all_times[i - 1] * 365); // Same as above
                Volatility vol = surface->blackVol(d, strike);
                BOOST_CHECK_CLOSE(vol, expectedVol, 1e-12);
            }
        }
    }

    // 2. Check we don't throw for all points and get a positive vol
    vector<Real> all_strikes;
    vector<Date> all_dates;
    for (auto vd : volData)
        all_strikes.push_back(spot * vd[0]);
    for (auto t : all_times)
        all_dates.push_back(today + int(t * 365));

    for (auto strike : all_strikes) {
        for (auto d : all_dates)
            BOOST_CHECK(surface->blackVol(d, strike) > 0.0001);
        for (auto t : all_times)
            BOOST_CHECK(surface->blackVol(t, strike) > 0.0001);
    }
}

BOOST_AUTO_TEST_CASE(testBlackVarianceSurfaceConstantVol) {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceSurfaceSparse with constant vol data...");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(1, Mar, 2010);
    Date today = Settings::instance().evaluationDate();

    // the 3 vectors we pass into the vol term structure
    // We setup a small grid with 10% everywhere, this should return 10% vol for
    // any point, i.e. a flat surface
    vector<Date> dates = { Date(1, Mar, 2011), Date(1, Mar, 2011), Date(1, Mar, 2012), Date(1, Mar, 2012),
                           Date(1, Mar, 2013) };
    vector<Real> strikes = { 2000, 3000, 2500, 3500, 3000 };
    vector<Volatility> vols(strikes.size(), 0.1); // 10% everywhere

    Calendar cal = TARGET();
    DayCounter dc = ActualActual(ActualActual::ISDA);

    QuantExt::BlackVarianceSurfaceSparse surface(today, cal, dates, strikes, vols, dc);

    // Check we don't throw for all points and get a vol of 10%
    for (Time t = 0.2; t < 20; t += 0.2) {
        for (Real strike = 1500; strike < 6000; strike += 100) {
            BOOST_CHECK_CLOSE(surface.blackVol(t, strike), 0.1, 1e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBlackVarianceSurfaceInputs) {
    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceSurfaceSparse with erroneous inputs");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(1, Mar, 2010);
    Date today = Settings::instance().evaluationDate();

    // the 3 vectors we pass into the vol term structure
    // We insure that the vectors don't match in size.
    vector<Date> dates = { Date(1, Mar, 2011), Date(1, Mar, 2011), Date(1, Mar, 2012), Date(1, Mar, 2012) };
    vector<Real> strikes = { 2000, 3000, 2500, 3500, 3000 };
    vector<Volatility> vols(strikes.size(), 0.1);

    Calendar cal = TARGET();
    DayCounter dc = ActualActual(ActualActual::ISDA);

    BOOST_CHECK_THROW(QuantExt::BlackVarianceSurfaceSparse(today, cal, dates, strikes, vols, dc), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testBalckVarianceEdgeCases) {
    // Asking on/past first/last points on strikes/expiries
    BOOST_TEST_MESSAGE("Testing edge cases");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(1, Mar, 2010);
    Date today = Settings::instance().evaluationDate();

    // the 3 vectors we pass into the vol term structure
    // We setup a small grid with 10% everywhere, this should return 10% vol for
    // any point, i.e. a flat surface
    vector<Date> dates = { Date(1, Mar, 2011), Date(1, Mar, 2011), Date(1, Mar, 2012), Date(1, Mar, 2012),
                           Date(1, Mar, 2013) };
    vector<Real> strikes = { 2000, 3000, 2500, 3500, 3000 };
    vector<Volatility> vols(strikes.size(), 0.1); // 10% everywhere

    Calendar cal = TARGET();
    DayCounter dc = ActualActual(ActualActual::ISDA);

    Time t = 0.0;                   // vol at reference should be 0
    Real strike1 = 0.0;             // 0 strike
    Real strike2 = strikes.front(); // lowest strike given
    Real strike3 = 1500.0;          // between strikes strike
    Real strike4 = strikes.back();  // highest strike

    Real strikeNeg = -1000;
    // at reference date
    QuantExt::BlackVarianceSurfaceSparse surface(today, cal, dates, strikes, vols, dc);
    BOOST_CHECK_CLOSE(surface.blackVol(t, strike1), 0.1, 1e-12);
    BOOST_CHECK_CLOSE(surface.blackVol(t, strike2), 0.1, 1e-12);
    BOOST_CHECK_CLOSE(surface.blackVol(t, strike3), 0.1, 1e-12);
    BOOST_CHECK_CLOSE(surface.blackVol(t, strike4), 0.1, 1e-12);

    // passed reference date
    Time t1 = surface.timeFromReference(Date(1, Mar, 2014));
    Real expectedVol = 0.1;
    BOOST_CHECK_CLOSE(surface.blackVol(t1, strike2), expectedVol, 1e-12);

    // negative strike
    BOOST_CHECK_THROW(surface.blackVol(t, strikeNeg), QuantLib::Error);
}

BOOST_AUTO_TEST_CASE(testBlackVarianceSinglePoint) {
    BOOST_TEST_MESSAGE("Testing surface from single point");
    // Given a single point, every request should be expected extrapolated value.

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(1, Mar, 2010);
    Date today = Settings::instance().evaluationDate();

    // the 3 vectors we pass into the vol term structure
    // We setup a single point with 10% - should be a flat surface
    vector<Date> dates = { Date(1, Mar, 2011) };
    vector<Real> strikes = { 2500 };
    vector<Volatility> vols(strikes.size(), 0.1); // 10% everywhere

    Calendar cal = TARGET();
    DayCounter dc = ActualActual(ActualActual::ISDA);

    QuantExt::BlackVarianceSurfaceSparse surface(today, cal, dates, strikes, vols, dc);

    // Check we don't throw for all points and get a vol of 10%
    for (Time t = 0.0; t < 20; t += 0.2) {
        for (Real strike = 1500; strike < 6000; strike += 100) {
            BOOST_CHECK_CLOSE(surface.blackVol(t, strike), 0.1, 1e-12);
        }
    }
}

BOOST_AUTO_TEST_CASE(testBlackVarianceSurfaceAxisInterp) {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceSurfaceSparse axis interpolations");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(1, Mar, 2010);
    Date today = Settings::instance().evaluationDate();

    // the 3 vectors we pass into the vol term structure
    // We setup a 4 X 4 grid with different vols everywhere.
    // We test interpolations on grid edges and in centre
    vector<Date> dates = { Date(1, Mar, 2011), Date(1, Mar, 2011), Date(1, Mar, 2012), Date(1, Mar, 2012) };
    vector<Real> strikes = { 2000, 3000, 2000, 3000 };
    vector<Volatility> vols = { 0.105, 0.12, 0.17, 0.15 };

    Calendar cal = TARGET();
    DayCounter dc = ActualActual(ActualActual::ISDA);

    auto surface = QuantLib::ext::make_shared<QuantExt::BlackVarianceSurfaceSparse>(today, cal, dates, strikes, vols, dc);

    // query points
    auto t1 = surface->timeFromReference(Date(1, Mar, 2011)); // on first date
    auto t2 = surface->timeFromReference(Date(1, Sep, 2011)); // between 2 dates
    auto t3 = surface->timeFromReference(Date(1, Mar, 2012)); // on last date
    Real s1 = 2000;                                           // on first strike
    Real s2 = 2500;                                           // between 2 strikes
    Real s3 = 3000;                                           // on last strike

    // expected vals
    Volatility e1 = 0.151634737915710;
    Volatility e2 = 0.112749722837797;
    Volatility e3 = 0.146315408895419;
    Volatility e4 = 0.160312195418814;
    Volatility e5 = 0.140795255664746;

    // checks
    BOOST_CHECK_CLOSE(e1, surface->blackVol(t2, s1), 1e-12);
    BOOST_CHECK_CLOSE(e2, surface->blackVol(t1, s2), 1e-12);
    BOOST_CHECK_CLOSE(e3, surface->blackVol(t2, s2), 1e-12);
    BOOST_CHECK_CLOSE(e4, surface->blackVol(t3, s2), 1e-12);
    BOOST_CHECK_CLOSE(e5, surface->blackVol(t2, s3), 1e-12);
}

BOOST_AUTO_TEST_CASE(testBlackVarianceSurfaceFlatExtrapolation) {

    BOOST_TEST_MESSAGE("Testing QuantExt::BlackVarianceSurfaceSparse flat extrapolation");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(1, Mar, 2010);
    Date today = Settings::instance().evaluationDate();

    // the 3 vectors we pass into the vol term structure
    // We setup a 4 X 4 grid with different vols everywhere.
    vector<Date> dates = { Date(1, Mar, 2011), Date(1, Mar, 2011), Date(1, Mar, 2012), Date(1, Mar, 2012) };
    vector<Real> strikes = { 2000, 3000, 2000, 3000 };
    vector<Volatility> vols = { 0.105, 0.12, 0.17, 0.15 };

    Calendar cal = TARGET();
    DayCounter dc = ActualActual(ActualActual::ISDA);

    auto surface = QuantLib::ext::make_shared<QuantExt::BlackVarianceSurfaceSparse>(today, cal, dates, strikes, vols, dc, true,
                                                                            true, true);

    Real s1 = 2000; // on first strike
    Real s2 = 2500; // between 2 strikes
    Real s3 = 3000; // on last strike

    Time tb = surface->times().back();
    Real edgeVol1 = surface->blackVol(tb, s1);
    Real edgeVol2 = surface->blackVol(tb, s2);
    Real edgeVol3 = surface->blackVol(tb, s3);

    Real edgeVar1 = surface->blackVariance(tb, s1);
    Real edgeVar2 = surface->blackVariance(tb, s2);
    Real edgeVar3 = surface->blackVariance(tb, s3);

    for (Real t = 1; t < 10; t++) {
        BOOST_CHECK_CLOSE(edgeVol1, surface->blackVol(tb + t, s1), 1e-12);
        BOOST_CHECK_CLOSE(edgeVol2, surface->blackVol(tb + t, s2), 1e-12);
        BOOST_CHECK_CLOSE(edgeVol3, surface->blackVol(tb + t, s3), 1e-12);

        BOOST_CHECK_CLOSE(edgeVar1, surface->blackVariance(tb + t, s1) * tb / (tb + t), 1e-12);
        BOOST_CHECK_CLOSE(edgeVar2, surface->blackVariance(tb + t, s2) * tb / (tb + t), 1e-12);
        BOOST_CHECK_CLOSE(edgeVar3, surface->blackVariance(tb + t, s3) * tb / (tb + t), 1e-12);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
