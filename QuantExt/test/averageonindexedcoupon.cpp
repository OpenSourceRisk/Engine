/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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
#include "onindexcouponutils.hpp"
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/averageonindexedcouponpricer.hpp>
#include <format>
#include <oret/util/datapaths.hpp>
#include <string>

using namespace QuantLib;
using namespace QuantExt;
using namespace boost::unit_test_framework;
using namespace std;
namespace bdata = boost::unit_test::data;
using OnIndexCouponTest::TestCouponData;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AverageOnIndexCouponTests)

using AOIC = QuantExt::AverageONIndexedCoupon;
using AOICPricer = QuantExt::AverageONIndexedCouponPricer;

// Get the input and ouput file paths for the given coupon ID in the tests below.
pair<path, path> filePaths(const string& cpnId) {
    auto outPath = TEST_OUTPUT_PATH / path(cpnId + ".csv");
    auto expPath = TEST_INPUT_PATH / path(cpnId + ".csv");
    return {outPath, expPath};
}

// Parameter sets for coupon testCouponAccruals below.
// `approx` is true to set Takada approximation in pricer, false to use no approximation.
auto lookbacks   = bdata::make({    0,     0,     0,     2,     2,    2,     2,     2,     2});
auto obsShifts   = bdata::make({false, false, false,  true,  true, true, false, false, false});
auto telescopic  = bdata::make({false, false,  true, false, false, true, false, false,  true});
auto approx      = bdata::make({false,  true,  true, false,  true, true, false,  true,  true});
auto rateCutoffs = bdata::make({0, 3});

// Selection of the above parameters.
auto cpnVariants = (lookbacks ^ obsShifts ^ telescopic ^ approx) * rateCutoffs;

// Use this so that I can have an index available in the test body below.
// Could use auto idx = boost::unit_test::framework::current_test_case().p_id; in test body but may be brittle across 
// different versions of Boost.
auto idxCpnVariants = cpnVariants ^ bdata::xrange(cpnVariants.size().value());

#ifdef __INTELLISENSE__
void testCpnAccruals(Natural lb, bool os, bool ts, bool useApprox, Natural rco, size_t idx)
#else
BOOST_DATA_TEST_CASE(testCpnAccruals, idxCpnVariants, lb, os, ts, useApprox, rco, idx)
#endif
{
    // Create the coupon ID for output file.
    string strOs = os ? "os" : "nos";
    string strTs = ts ? "ts" : "nts";
    string strApprox = useApprox ? "approx" : "napprox";
    string cpnName = format("acc_{:02d}_lb-{}_{}_rco-{}_{}_{}", idx, lb, strOs, rco, strTs, strApprox);

    // Create the coupon.
    TestCouponData tcd;
    Real notional = 100000000;

    AOIC cpn(tcd.pmt, notional, tcd.start, tcd.end, tcd.sofr, 1.0, 0.0, rco, DayCounter(), lb * Days,
        Null<Natural>(), Date(), Date(), ts, os);

    if (!useApprox)
        cpn.setPricer(ext::make_shared<AOICPricer>(AOICPricer::None));

    auto [outFilePath, expFilePath] = filePaths(cpnName);
    OnIndexCouponTest::runCpnAccrualTest(cpn, outFilePath, expFilePath);
}

// Test gearings and spreads with different settings for whether start / end date of coupon is a business day.
auto gearings  = bdata::make({  2.0,   1.0});
auto spreads   = bdata::make({ 10.0,  10.0});
auto startBd   = bdata::make({false,  true});
auto endBd     = bdata::make({ true, false});

auto telescopic_1  = bdata::make({false, false, true});
auto approx_1      = bdata::make({false,  true, true});

auto gsCpnVariants = (gearings ^ spreads ^ startBd ^ endBd) * (telescopic_1 ^ approx_1);
auto idxGsCpnVariants = gsCpnVariants ^ bdata::xrange(gsCpnVariants.size().value());

#ifdef __INTELLISENSE__
void testCpnAccrualsGearingSpread(Real gearing, Spread spread, bool sibd, bool eibd, bool ts,
    bool useApprox, Size idx)
#else
BOOST_DATA_TEST_CASE(testCpnAccrualsGearingSpread, idxGsCpnVariants, gearing, spread, sibd, eibd, ts, useApprox, idx)
#endif
{
    // Create the coupon ID for output file, maintaining format of previous tests with additional fields.
    string strTs = ts ? "ts" : "nts";
    string strSibd = sibd ? "sibd" : "sih";
    string strEibd = eibd ? "eibd" : "eih";
    string strApprox = useApprox ? "approx" : "napprox";
    string cpnName = format("gs_{:02d}_lb-0_nos_rco-0_{}_g-{:g}_s-{:g}_{}_{}_{}",
        idx, strTs, gearing, spread, strSibd, strEibd, strApprox);

    // Create the coupon.
    TestCouponData tcd;
    Real notional = 100000000;
    Date start = sibd ? Date(3, Nov, 2025) : tcd.start;
    Date end = eibd ? Date(28, Nov, 2025) : tcd.end;

    AOIC cpn(tcd.pmt, notional, start, end, tcd.sofr, gearing, spread / 10000, 0, DayCounter(), 0 * Days,
        Null<Natural>(), Date(), Date(), ts, false);

    if (!useApprox)
        cpn.setPricer(ext::make_shared<AOICPricer>(AOICPricer::None));

    auto [outFilePath, expFilePath] = filePaths(cpnName);
    OnIndexCouponTest::runCpnAccrualTest(cpn, outFilePath, expFilePath);
}

// Test coupon with all non-standard arguments set. Telescoping must be false because an external fixing lag of 1 BD 
// is specified != index fixing days.
#ifdef __INTELLISENSE__
void testCpnAccrualsAll(bool useApprox, Size idx)
#else
BOOST_DATA_TEST_CASE(testCpnAccrualsAll, bdata::make({true, false}) ^ bdata::xrange(2), useApprox, idx)
#endif
{
    // Create the coupon ID for output file, maintaining format of previous tests with additional fields.
    // fl-1 indicates fixing lag of 1 BD.
    string strApprox = useApprox ? "approx" : "napprox";
    string cpnName = format("all_{:02d}_lb-2_os_rco-3_nts_g-1p5_s-10_{}_sibd_eibd_fl-1", idx, strApprox);

    // Create the coupon.
    TestCouponData tcd;
    Real notional = 100000000;
    Date start = Date(3, Nov, 2025);
    Date end = Date(28, Nov, 2025);
    AOIC cpn(tcd.pmt, notional, start, end, tcd.sofr, 1.5, 0.0010, 3, DayCounter(), 2 * Days, 1,
        Date(), Date(), false, true);

    if (!useApprox)
        cpn.setPricer(ext::make_shared<AOICPricer>(AOICPricer::None));

    auto [outFilePath, expFilePath] = filePaths(cpnName);
    OnIndexCouponTest::runCpnAccrualTest(cpn, outFilePath, expFilePath);
}
// clang-format on

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
