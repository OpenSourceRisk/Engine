/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>

#include <qle/math/stoplightbounds.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::unit_test_framework;
using std::vector;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(StopLightBoundsTest)

BOOST_AUTO_TEST_CASE(testSinglePortfolioAgainstReferenceResults) {
    BOOST_TEST_MESSAGE("Testing single portfolio stoplight bounds against reference results");

    std::vector<Real> psl = {0.95, 0.9999};

    // in general an even higher number of samples may be required to match all the reference results
    Size samples = 5000000;
    Size seed = 42;
    Real p = 0.99;
    Size days = 10;

    static const std::vector<Size> obs = {200, 300, 500}; // avoid too long running time

    BOOST_TEST_MESSAGE(
        "Observations      MaxExceed Green (computed/expected)   MaxExceedences Amber (computed/expected)");

    for (Size i = 0; i < obs.size(); ++i) {
        auto b = stopLightBounds(psl, obs[i], days, p, 1, Matrix(1, 1, 1.0), samples, seed);
        auto tab = stopLightBoundsTabulated(psl, obs[i], days, p);
        BOOST_TEST_MESSAGE(std::right << std::setw(10) << obs[i] << std::setw(28)
                                      << (std::to_string(b[0]) + " / " + std::to_string(tab[0])) << std::setw(38)
                                      << (std::to_string(b[1]) + " / " + std::to_string(tab[1])));
        BOOST_CHECK_EQUAL(b[0], tab[0]);
        BOOST_CHECK_EQUAL(b[1], tab[1]);
    }
} // testSinglePortfolioAgainstReferenceResults

BOOST_AUTO_TEST_CASE(testMultiplePortfoliosAgainstReferenceResults) {
    BOOST_TEST_MESSAGE("Testing multiple portfolios stoplight bounds against reference results");

    // see ISDA SIMM Backtesting Report, 14Feb17, section 15.3
    static const std::vector<Size> tab = {19, 51};

    std::vector<Real> psl = {0.95, 0.9999};

    Size samples = 1500000;
    Size seed = 42;
    Real p = 0.99;
    Size days = 10;
    Size portfolios = 3;
    Matrix correlation(3, 3);

    for (Size i = 0; i < 3; ++i) {
        for (Size j = 0; j < 3; ++j) {
            correlation[i][j] = i == j ? 1.0 : 0.5;
        }
    }

    static const std::vector<Size> obs = {250};

    BOOST_TEST_MESSAGE(
        "Observations      MaxExceed Green (computed/expected)   MaxExceedences Amber (computed/expected)");

    for (Size i = 0; i < obs.size(); ++i) {
        auto b = stopLightBounds(psl, obs[i], days, p, portfolios, correlation, samples, seed);
        BOOST_TEST_MESSAGE(std::right << std::setw(10) << obs[i] << std::setw(28)
                                      << (std::to_string(b[0]) + " / " + std::to_string(tab[0])) << std::setw(38)
                                      << (std::to_string(b[1]) + " / " + std::to_string(tab[1])));
        BOOST_CHECK_EQUAL(b[0], tab[0]);
        BOOST_CHECK_EQUAL(b[1], tab[1]);
    }
} // testMultiplePortfoliosAgainstReferenceResults

BOOST_AUTO_TEST_CASE(testIidBoundsAgainstReferenceResults) {
    BOOST_TEST_MESSAGE("Testing iid bounds against reference results");
    /* see SUPERVISORY FRAMEWORK FOR THE USE OF "BACKTESTING" IN CONJUNCTION WITH THE
       INTERNAL MODELS APPROACH TO MARKET RISK CAPITAL REQUIREMENTS,
       Basle Committee on Banking Supervision January 1996
       http://www.bis.org/publ/bcbs22.pdf */
    auto bounds = stopLightBounds({0.95, 0.9999}, 250, 0.99);
    BOOST_CHECK_EQUAL(bounds[0], 4);
    BOOST_CHECK_EQUAL(bounds[1], 9);
    Real cumProb = 0.0;
    std::vector<Real> exp = {0.0811, 0.2858, 0.5432, 0.7581, 0.8922, 0.9588, 0.9863, 0.9960, 0.9989, 0.9997, 0.9999};
    for (Size i = 0; i <= 10; ++i) {
        stopLightBounds({0.95, 0.99}, 250, 0.99, i, &cumProb);
        BOOST_CHECK_SMALL(cumProb - exp[i], 0.0001);
    }
} // testIidBoundsAgainstReferenceResults

// this test case runs apprx. 4 min because of the high number of samples used and is therefore disabled by default
// FIXME the test fails on several tabulated values, both with 5m and 50m samples (see ORE ticket 1383 for details)
BOOST_AUTO_TEST_CASE(testGenerateStopLightBoundTable, *boost::unit_test::disabled()) {
    BOOST_TEST_MESSAGE("Testing generating stop light bounds table against reference results");

    std::vector<Size> obs = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120,
    130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 260, 270, 280, 290, 300, 310, 320, 330, 340, 350,
    360, 370, 380, 390, 400, 410, 420, 430, 440, 450, 460, 470, 480, 490, 500, 510, 520, 530, 540, 550, 560, 570, 580,
    590, 600, 610, 620, 630, 640, 650, 660, 670, 680, 690, 700, 710, 720, 730, 740, 750, 760, 770, 780, 790, 800, 810,
    820, 830, 840, 850, 860, 870, 880, 890, 900, 910, 920, 930, 940, 950, 960, 970, 980, 990, 1000, 1010, 1020, 1030,
    1040, 1050, 1060, 1070, 1080, 1090, 1100, 1110, 1120, 1130, 1140, 1150, 1160, 1170, 1180, 1190, 1200, 1210, 1220,
    1230, 1240, 1250, 1260, 1270, 1280, 1290, 1300, 1310, 1320, 1330, 1340, 1350, 1360, 1370, 1380, 1390, 1400, 1410,
    1420, 1430, 1440, 1450, 1460, 1470, 1480, 1490, 1500, 1510, 1520, 1530, 1540, 1550, 1560, 1570, 1580, 1590, 1600,
    1610, 1620, 1630, 1640, 1650, 1660, 1670, 1680, 1690, 1700, 1710, 1720, 1730, 1740, 1750, 1760, 1770, 1780, 1790,
    1800, 1810, 1820, 1830, 1840, 1850, 1860, 1870, 1880, 1890, 1900, 1910, 1920, 1930, 1940, 1950, 1960, 1970, 1980,
    1990, 2000, 2010, 2020, 2030, 2040, 2050, 2060, 2070, 2080, 2090, 2100, 2110, 2120, 2130, 2140, 2150, 2160, 2170,
    2180, 2190, 2200, 2210, 2220, 2230, 2240, 2250, 2260, 2270, 2280, 2290, 2300, 2310, 2320, 2330, 2340, 2350, 2360,
    2370, 2380, 2390, 2400, 2410, 2420, 2430, 2440, 2450, 2460, 2470, 2480, 2490, 2500, 2510, 2520, 2530, 2540, 2550,
    2560, 2570, 2580, 2590, 2600, 2610, 2620, 2630, 2640, 2650, 2660, 2670, 2680, 2690, 2700, 2710, 2720, 2730, 2740,
    2750, 2760, 2770, 2780, 2790, 2800, 2810, 2820, 2830, 2840, 2850, 2860, 2870, 2880, 2890, 2900, 2910, 2920, 2930,
    2940, 2950, 2960, 2970, 2980, 2990, 3000, 3010, 3020, 3030, 3040, 3050, 3060, 3070, 3080, 3090, 3100, 3110, 3120,
    3130, 3140, 3150, 3160, 3170, 3180, 3190, 3200, 3210, 3220, 3230, 3240, 3250, 3260, 3270, 3280, 3290, 3300, 3310,
    3320, 3330, 3340, 3350, 3360, 3370, 3380, 3390, 3400, 3410, 3420, 3430, 3440, 3450, 3460, 3470, 3480, 3490, 3500,
    3510, 3520, 3530, 3540, 3550, 3560, 3570, 3580, 3590, 3600, 3610, 3620, 3630, 3640, 3650, 3660, 3670, 3680, 3690
    };
    std::vector<Real> psl = {0.95, 0.9999};
    Size samples = 100000000;
    Size seed = 42;
    Real p = 0.99;
    Size days = 10;

    auto table = generateStopLightBoundTable(obs, psl, samples, seed, days, p);
    BOOST_REQUIRE(table.size() == obs.size());

    BOOST_TEST_MESSAGE(
        "Observations      MaxExceed Green (computed/expected)   MaxExceedences Amber (computed/expected)");

    for (Size i = 0; i < obs.size(); ++i) {
        BOOST_REQUIRE(table[i].first == obs[i]);
        BOOST_REQUIRE(table[i].second.size() == psl.size());
        auto tab = stopLightBoundsTabulated(psl, obs[i], days, p);
        BOOST_TEST_MESSAGE(std::right << std::setw(10) << obs[i] << std::setw(28)
                                      << (std::to_string(table[i].second[0]) + " / " + std::to_string(tab[0]))
                                      << std::setw(38)
                                      << (std::to_string(table[i].second[1]) + " / " + std::to_string(tab[1])));
        BOOST_CHECK_EQUAL(table[i].second[0], tab[0]);
        BOOST_CHECK_EQUAL(table[i].second[1], tab[1]);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
