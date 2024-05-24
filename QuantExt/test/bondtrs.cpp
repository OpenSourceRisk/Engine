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
#include <iostream>
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/schedule.hpp>
#include <qle/indexes/bondindex.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>
#include <qle/pricingengines/discountingbondtrsengine.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace {

struct TestDatum {
    std::string testLabel;
    Real benchmarkRate, defaultSpread, securitySpread, oisRate, euriborRate, bondFixing;
    bool seasoned;
};

std::ostream& operator<<(std::ostream& o, const TestDatum& d) {
    return o << "[" << d.testLabel << ": benchmarkRate=" << d.benchmarkRate << ", defaultSpread=" << d.defaultSpread
             << ", securitySpread=" << d.securitySpread << ", oisRate=" << d.oisRate
             << ", euriborRate=" << d.euriborRate << ", bondFixing=" << d.bondFixing << ", seasoned=" << std::boolalpha
             << d.seasoned << "]";
}

TestDatum testData[] = {{"singleCurve, zero credit spread, no  sec spread", 0.03, 0.00, 0.00, 0.02, 0.02, 1.10, false},
                        {"multiCurve,  zero credit spread, no  sec spread", 0.02, 0.00, 0.01, 0.02, 0.02, 1.10, false},
                        {"singleCurve, pos  credit spread, no  sec spread", 0.02, 0.01, 0.00, 0.02, 0.02, 1.07, false},
                        {"multiCurve,  pos  credit spread, no  sec spread", 0.02, 0.01, 0.00, 0.01, 0.02, 1.07, false},
                        {"singleCurve, pos  credit spread, pos sec spread", 0.02, 0.01, 0.01, 0.02, 0.02, 1.025, false},
                        {"multiCurve,  pos  credit spread, pos sec spread", 0.02, 0.01, 0.01, 0.01, 0.02, 1.025, false},
                        {"singleCurve, zero credit spread, no  sec spread", 0.02, 0.00, 0.00, 0.02, 0.02, 1.10, true},
                        {"multiCurve,  zero credit spread, no  sec spread", 0.02, 0.00, 0.00, 0.01, 0.02, 1.10, true},
                        {"singleCurve, pos  credit spread, no  sec spread", 0.02, 0.01, 0.00, 0.02, 0.02, 1.07, true},
                        {"multiCurve,  pos  credit spread, no  sec spread", 0.02, 0.01, 0.00, 0.01, 0.02, 1.07, true},
                        {"singleCurve, pos  credit spread, pos sec spread", 0.02, 0.01, 0.01, 0.02, 0.02, 1.025, true},
                        {"multiCurve,  pos  credit spread, pos sec spread", 0.02, 0.01, 0.01, 0.01, 0.02, 1.025, true}};

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BondTRSTest)

BOOST_DATA_TEST_CASE(testBondTRS, boost::unit_test::data::make(testData), data) {

    BOOST_TEST_MESSAGE("Testing QuantExt Bond TRS pricing.");

    Settings::instance().evaluationDate() = Date(5, Feb, 2016);
    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();

    // bond market data
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(data.benchmarkRate));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(data.defaultSpread));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yieldCurve(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Annual));
    Handle<DefaultProbabilityTermStructure> defaultCurve(
        QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(data.securitySpread));
    Handle<Quote> recoveryRateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.4));

    // derivatives market data
    Handle<Quote> oisQuote(QuantLib::ext::make_shared<SimpleQuote>(data.oisRate));
    Handle<YieldTermStructure> oisCurve(QuantLib::ext::make_shared<FlatForward>(today, oisQuote, dc, Compounded, Annual));
    Handle<Quote> iborQuote(QuantLib::ext::make_shared<SimpleQuote>(data.euriborRate));
    Handle<YieldTermStructure> iborCurve(QuantLib::ext::make_shared<FlatForward>(today, iborQuote, dc, Compounded, Annual));

    // build the underlying bond
    Date startDate = data.seasoned ? today - 3 * Months : TARGET().advance(today, 2 * Days);
    Date endDate = startDate + Period(5, Years);
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, 1 * Years, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);
    Real redemption = 100.0;
    Real couponRate = 0.04;
    Leg leg =
        FixedRateLeg(schedule).withNotionals(redemption).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(QuantLib::ext::make_shared<QuantLib::Bond>(0, calendar, startDate, leg));

    // build associated bond index
    std::string secId = "SECURITY";
    QuantLib::ext::shared_ptr<QuantExt::BondIndex> bondIndex = QuantLib::ext::make_shared<QuantExt::BondIndex>(
        secId, false, false, NullCalendar(), bond, yieldCurve, defaultCurve, recoveryRateQuote, bondSpecificSpread,
        Handle<YieldTermStructure>(), false);
    Date bondFixingDate(5, Nov, 2015);
    bondIndex->addFixing(bondFixingDate, data.bondFixing);

    // build and attach bond engine
    Period timeStep = 1 * Months;
    QuantLib::ext::shared_ptr<PricingEngine> bondEngine(QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(
        yieldCurve, defaultCurve, recoveryRateQuote, bondSpecificSpread, timeStep));
    bond->setPricingEngine(bondEngine);

    // build the TRS funding leg
    Schedule floatingSchedule(startDate, endDate, 6 * Months, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate,
                              lastDate);
    QuantLib::ext::shared_ptr<IborIndex> iborIndex = QuantLib::ext::make_shared<Euribor>(6 * Months, iborCurve);
    Leg fundingLeg = IborLeg(schedule, iborIndex).withNotionals(redemption);
    Date iborFixingDate(3, Nov, 2015);
    iborIndex->addFixing(iborFixingDate, 0.03);
    Leg fundingNotionalLeg; // no notional exchanges on funding leg

	// build the valuation and payment dates from the floating schedule
    std::vector<Date> valuationDates, paymentDates;
    for (const auto& d : floatingSchedule.dates()) {
        valuationDates.push_back(d);
        if (d != floatingSchedule.dates().front())
            paymentDates.push_back(d);
    }

    // build TRS
    QuantLib::ext::shared_ptr<BondTRS> trs = QuantLib::ext::make_shared<BondTRS>(
        bondIndex, 1.0, Null<Real>(), std::vector<QuantLib::Leg>{fundingLeg, fundingNotionalLeg},
        true, valuationDates, paymentDates);
    QuantLib::ext::shared_ptr<PricingEngine> trsEngine(new QuantExt::DiscountingBondTRSEngine(oisCurve));
    trs->setPricingEngine(trsEngine);

    // build floating rate note (risk free, i.e. zero credit spread, security spread)
    QuantLib::ext::shared_ptr<QuantLib::Bond> floater(QuantLib::ext::make_shared<QuantLib::Bond>(0, calendar, startDate, fundingLeg));
    Handle<Quote> floaterIssuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    Handle<DefaultProbabilityTermStructure> floaterDefaultCurve(
        QuantLib::ext::make_shared<FlatHazardRate>(today, floaterIssuerSpreadQuote, dc));
    Handle<Quote> floaterSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(data.securitySpread));
    Handle<Quote> floaterRecoveryRateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<PricingEngine> floaterEngine(QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(
        yieldCurve, floaterDefaultCurve, floaterRecoveryRateQuote, floaterSpecificSpread, timeStep));
    floater->setPricingEngine(floaterEngine);

    std::ostringstream output;

    // Real bondNpv = bond->cleanPrice();
    Real bondNpv = bond->NPV();

    output << "Bond NPV                   = " << bondNpv << "\n";
    output << "Floater NPV                = " << floater->NPV() << "\n";
    output << "TRS NPV                    = " << trs->NPV() << "\n";
    output << "Bond + TRS - Floater       = " << bondNpv + trs->NPV() - floater->NPV() << "\n";

    BOOST_TEST_MESSAGE(output.str());

    // plausibility check only:
    // the package of a long bond and a TRS (pay total return leg, rec Euribor)
    // is similar to a risk free floater, but in addition we receive the difference of the bond npv
    // and par over the lifetime of the swap through the compensation payments
    // notice this check is equivalent to: trs->NPV() ~ floater->NPV() - redemption
    BOOST_CHECK_SMALL((bondNpv + trs->NPV() - floater->NPV()) - (bondNpv - redemption), 1.0);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
