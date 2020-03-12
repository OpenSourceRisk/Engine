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

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/indexes/ibor/fedfunds.hpp>
#include <ql/instruments/makecds.hpp>
#include <ql/termstructures/yield/flatforward.hpp>

#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/credit/piecewisedefaultcurve.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <qle/termstructures/oisratehelper.hpp>
#include <qle/termstructures/averageoisratehelper.hpp>
#include <qle/termstructures/defaultprobabilityhelpers.hpp>

#include <boost/make_shared.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CdsRateHelpersTest)

struct MarketCdsQuote {
    Date indexStart;
    Period term;
    Real spread;
    Date swapStart;
};

struct MarketOisQuote {
    Period term;
    Real spread;
};

struct MarketAverageOisQuote {
    Period term;
    Real spread;
    Real rate;
};

Handle<YieldTermStructure> discountCurve(Date asof) {

    boost::shared_ptr<OvernightIndex> on_index(new FedFunds());
    std::vector<boost::shared_ptr<RateHelper> > helpers(1, boost::make_shared<DepositRateHelper>(0.0240000000, on_index));

    std::vector<MarketOisQuote> ois_data = {
        { 1 * Weeks, 0.0240080000 },
        { 2 * Weeks, 0.0240120000 },
        { 3 * Weeks, 0.0240150000 },
        { 1 * Months, 0.0240200000 },
        { 2 * Months, 0.0240400000 },
        { 3 * Months, 0.0240690000 },
        { 4 * Months, 0.0240505000 },
        { 5 * Months, 0.0240415000 },
        { 6 * Months, 0.0240330000 },
        { 7 * Months, 0.0240230000 },
        { 8 * Months, 0.0240140000 },
        { 9 * Months, 0.0239850000 },
        { 10 * Months, 0.0239560000 },
        { 11 * Months, 0.0239260000 },
        { 1 * Years, 0.0238980000 }
    };

    for (MarketOisQuote d: ois_data) {
        helpers.push_back(boost::make_shared<QuantExt::OISRateHelper>(
            2, d.term, Handle<Quote>(boost::make_shared<SimpleQuote>(d.spread)), on_index, Actual360(),
            2, false, Annual, Following, Following, DateGeneration::Backward,
            Handle<YieldTermStructure>(), true));
    }

    std::vector<MarketAverageOisQuote> avg_ois_data = {
        { 2 * Years, 0.0024630000, 0.0253900000 },
        { 3 * Years, 0.0025250000, 0.0247470000 },
        { 4 * Years, 0.0025500000, 0.0244660000 },
        { 5 * Years, 0.0025500000, 0.0244700000 },
        { 7 * Years, 0.0026000000, 0.0249300000 },
        { 10 * Years, 0.0026500000, 0.0258770000 },
        { 15 * Years, 0.0026880000, 0.0270000000 },
        { 20 * Years, 0.0027130000, 0.0274900000 },
        { 30 * Years, 0.0027250000, 0.0275600000 }
    };

    for (MarketAverageOisQuote d: avg_ois_data) {
        helpers.push_back(boost::make_shared<QuantExt::AverageOISRateHelper>(
            Handle<Quote>(boost::make_shared<SimpleQuote>(d.rate)), 2 * Days, d.term,
            6 * Months, Thirty360(), UnitedStates(), ModifiedFollowing, ModifiedFollowing,
            on_index, 3 * Months, Handle<Quote>(boost::make_shared<SimpleQuote>(d.spread)),
            2, Handle<YieldTermStructure>()));
    }

    return Handle<YieldTermStructure>(
        boost::make_shared<PiecewiseYieldCurve<Discount, QuantLib::LogLinear>>(
            asof, helpers, Actual365Fixed(), 0.000000000001));

}

BOOST_AUTO_TEST_CASE(testSpreadCdsRateHelpersConsistency) {

    BOOST_TEST_MESSAGE("Testing QuantExt::SpreadCdsHelper consistency...");

    // Market data for CDXHY31v3 on 12 March 2019

    std::vector<MarketCdsQuote> data = {
        { Date(20, Sep, 2018), 3 * Years, 0.0252370000 },
        { Date(20, Sep, 2018), 5 * Years, 0.0346580000 },
        { Date(20, Sep, 2018), 7 * Years, 0.0373870000 },
        { Date(20, Sep, 2018), 10 * Years, 0.0450510000 }
    };

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(12, Mar, 2019);
    Date today = Settings::instance().evaluationDate();
    Real recoveryRate = 0.3;
    Natural settlementDate = 1;
    Calendar calendar = UnitedStates();

    Handle<YieldTermStructure> usd1d = discountCurve(today);

    std::vector<boost::shared_ptr<QuantExt::DefaultProbabilityHelper> > helpers;

    for (MarketCdsQuote d : data) {
        helpers.push_back(boost::make_shared<QuantExt::SpreadCdsHelper>(
            Handle<Quote>(boost::make_shared<SimpleQuote>(d.spread)), d.term, settlementDate,
            calendar, Quarterly, Following, DateGeneration::CDS2015, Actual360(), recoveryRate, usd1d,
            d.indexStart, true, true));
    }

    Handle<DefaultProbabilityTermStructure> curve(
        boost::make_shared<PiecewiseDefaultCurve<QuantLib::HazardRate, QuantLib::BackwardFlat> >(
            today, helpers, Actual365Fixed()));
    curve->enableExtrapolation(true);

    boost::shared_ptr<PricingEngine> engine = boost::make_shared<MidPointCdsEngine>(curve, recoveryRate, usd1d);

    for (MarketCdsQuote d : data) {
        Schedule schedule = MakeSchedule()
            .from(calendar.adjust(d.indexStart, Following))
            .to(d.indexStart + settlementDate * Days + d.term)
            .withFrequency(Quarterly)
            .withRule(DateGeneration::CDS2015)
            .withConvention(Following)
            .withTerminationDateConvention(Unadjusted)
            .withCalendar(calendar);
        CreditDefaultSwap swap = CreditDefaultSwap(Protection::Buyer, 100.0, 1, schedule, Following, Actual360(),
                                 true, true, d.indexStart, boost::shared_ptr<Claim>(), Actual360(true), true);
        swap.setPricingEngine(engine);

        std::cout << swap.couponLegNPV() + swap.accrualRebateNPV() << std::endl;

        BOOST_CHECK_CLOSE(d.spread, swap.fairSpread(), 0.1);
    }

    std::vector<MarketCdsQuote> forward_data = {
        { Date(20, Sep, 2018), 5 * Years, 350.4781642 / 10000, Date(20, Mar, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 357.4480281 / 10000, Date(17, Apr, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 364.7678584 / 10000, Date(15, May, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 374.1404386 / 10000, Date(19, Jun, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 382.0951671 / 10000, Date(17, Jul, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 392.2040452 / 10000, Date(21, Aug, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 400.3046881 / 10000, Date(18, Sep, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 408.6678444 / 10000, Date(16, Oct, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 419.1074385 / 10000, Date(20, Nov, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 427.4210192 / 10000, Date(18, Dec, 2019) },
        { Date(20, Sep, 2018), 5 * Years, 435.7021125 / 10000, Date(15, Jan, 2020) },
        { Date(20, Sep, 2018), 5 * Years, 446.0507816 / 10000, Date(19, Feb, 2020) },
        { Date(20, Sep, 2018), 5 * Years, 454.3281214 / 10000, Date(18, Mar, 2020) },
        { Date(20, Sep, 2018), 5 * Years, 462.6045922 / 10000, Date(15, Apr, 2020) }
    };

    for (MarketCdsQuote d : forward_data) {

        Schedule schedule = MakeSchedule()
            .from(calendar.adjust(d.swapStart, Following))
            .to(d.indexStart + settlementDate * Days + d.term)
            .withFrequency(Quarterly)
            .withRule(DateGeneration::CDS2015)
            .withConvention(Following)
            .withTerminationDateConvention(Unadjusted)
            .withCalendar(calendar);
        CreditDefaultSwap cds = CreditDefaultSwap(Protection::Buyer, 100.0, 1, schedule, Following, Actual360(),
                                                   true, true, d.swapStart, boost::shared_ptr<Claim>(), Actual360(true));
        boost::shared_ptr<Exercise> exercise = boost::make_shared<EuropeanExercise>(d.swapStart);

        // boost::shared_ptr<QuantExt::IndexCdsOption> option =
        //     boost::make_shared<QuantExt::IndexCdsOption>(cds, exercise, false,, upfrontStrike_);


// CdsOption(const ext::shared_ptr<CreditDefaultSwap>& swap,
//                   const ext::shared_ptr<Exercise>& exercise,
//                   bool knocksOut = true);


        // Schedule schedule1 = MakeSchedule()
        //     .from(calendar.adjust(d.indexStart, Following))
        //     .to(d.indexStart + settlementDate * Days + d.term)
        //     .withFrequency(Quarterly)
        //     .withRule(DateGeneration::CDS2015)
        //     .withConvention(Following)
        //     .withTerminationDateConvention(Unadjusted)
        //     .withCalendar(calendar);
        // CreditDefaultSwap swap1 = CreditDefaultSwap(Protection::Buyer, 100.0, 1, schedule1, Following, Actual360(),
        //                          true, true, d.indexStart, boost::shared_ptr<Claim>(), Actual360(true));
        // swap1.setPricingEngine(engine);

        // Schedule schedule2 = MakeSchedule()
        //     .from(calendar.adjust(d.indexStart, Following))
        //     .to(d.swapStart)
        //     .withFrequency(Quarterly)
        //     .withRule(DateGeneration::CDS2015)
        //     .withConvention(Following)
        //     .withTerminationDateConvention(Unadjusted)
        //     .withCalendar(calendar);
        // CreditDefaultSwap swap2 = CreditDefaultSwap(Protection::Buyer, 100.0, 1, schedule2, Following, Actual360(),
        //                          true, true, d.indexStart, boost::shared_ptr<Claim>(), Actual360(true));
        // swap2.setPricingEngine(engine);

        // Real npv = swap1.couponLegNPV() - swap2.couponLegNPV();
        // Real npv_with_accrual = npv + swap1.accrualRebateNPV() - swap2.accrualRebateNPV();

        // std::cout << d.swapStart << std::endl;
        // std::cout << npv << std::endl;
        // std::cout << npv / curve->survivalProbability(d.swapStart) << std::endl;
        // std::cout << npv_with_accrual << std::endl;
        // std::cout << npv_with_accrual / curve->survivalProbability(d.swapStart) << std::endl;

        // BOOST_CHECK_CLOSE(d.spread, swap.fairSpread(), 0.1);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
