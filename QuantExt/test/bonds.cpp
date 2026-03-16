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
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/schedule.hpp>
#include <qle/instruments/impliedbondspread.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <boost/make_shared.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BondsTest)

BOOST_AUTO_TEST_CASE(testBondSpreads) {

    BOOST_TEST_MESSAGE("Testing QuantExt bond spread helper");

    SavedSettings backup;
    Settings::instance().evaluationDate() = Date(8, Dec, 2016);
    Date today = Settings::instance().evaluationDate();

    // market data
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.02));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(0.005));

    // build the bond
    Date startDate = today;
    Date endDate = startDate + Period(10, Years);
    Period tenor = 6 * Months;
    Calendar calendar = WeekendsOnly();
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

    Real redemption = 100.0;
    Real couponRate = 0.04;
    Leg leg =
        FixedRateLeg(schedule).withNotionals(redemption).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(QuantLib::ext::make_shared<QuantLib::Bond>(0, WeekendsOnly(), today, leg));
    Handle<Quote> recovery;
    QuantLib::ext::shared_ptr<PricingEngine> pricingEngine(
        QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, bondSpecificSpread, 1 * Months));
    bond->setPricingEngine(pricingEngine);

    Real price = bond->dirtyPrice();
    BOOST_TEST_MESSAGE("Bond price = " << price);

    // now calculated the implied bond spread, given the price
    QuantLib::ext::shared_ptr<SimpleQuote> tmpSpread = QuantLib::ext::make_shared<SimpleQuote>(0.0);
    Handle<Quote> tmpSpreadH(tmpSpread);
    QuantLib::ext::shared_ptr<PricingEngine> tmpEngine;
    tmpEngine.reset(new QuantExt::DiscountingRiskyBondEngine(yts, dpts, recovery, tmpSpreadH, 1 * Months));

    Real impliedSpread = QuantExt::detail::ImpliedBondSpreadHelper::calculate(bond, tmpEngine, tmpSpread, price, false,
                                                                              1.e-12, 10000, -0.02, 1.00);
    BOOST_TEST_MESSAGE("Implied spread = " << impliedSpread);
    BOOST_CHECK_CLOSE(impliedSpread, bondSpecificSpread->value(), 0.0001);

    Real price2 = bond->dirtyPrice();
    BOOST_CHECK_EQUAL(price, price2);

    // which spread would mean the bond price is par?
    Real parRedemption = 100.0;
    Real impliedSpreadPar = QuantExt::detail::ImpliedBondSpreadHelper::calculate(
        bond, tmpEngine, tmpSpread, parRedemption, false, 1.e-12, 10000, -0.02, 1.00);
    BOOST_TEST_MESSAGE("Par bond price would require spread of " << impliedSpreadPar);
    BOOST_CHECK_EQUAL(
        price, bond->dirtyPrice()); // ensure hypothetical impliedSpread calc has not affected the original position

    QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*bondSpecificSpread)->setValue(impliedSpreadPar);
    Real pricePar = bond->dirtyPrice();
    BOOST_TEST_MESSAGE("Bond spread of " << bondSpecificSpread->value() << " means price of " << pricePar);
    BOOST_CHECK_CLOSE(pricePar, parRedemption, 0.0001);

    // now check that bond pricing works even if no credit curve exists

    dpts = Handle<DefaultProbabilityTermStructure>();
    pricingEngine.reset(new QuantExt::DiscountingRiskyBondEngine(yts, dpts, recovery, bondSpecificSpread, 1 * Months));
    tmpEngine.reset(new QuantExt::DiscountingRiskyBondEngine(yts, dpts, recovery, tmpSpreadH, 1 * Months));
    bond->setPricingEngine(pricingEngine);
    Real priceNoIssuerCurve = bond->dirtyPrice();
    BOOST_TEST_MESSAGE("Bond price (ignoring issuer spread) = " << priceNoIssuerCurve);
    impliedSpread = QuantExt::detail::ImpliedBondSpreadHelper::calculate(bond, tmpEngine, tmpSpread, priceNoIssuerCurve,
                                                                         false, 1.e-12, 10000, -0.02, 1.00);
    BOOST_TEST_MESSAGE("Bond spread (ignoring issuer spread) = " << impliedSpread);
    BOOST_CHECK_CLOSE(impliedSpread, bondSpecificSpread->value(), 0.0001);

    // which spread would mean the bond price is par?
    impliedSpreadPar = QuantExt::detail::ImpliedBondSpreadHelper::calculate(bond, tmpEngine, tmpSpread, parRedemption,
                                                                            false, 1.e-12, 10000, -0.02, 1.00);
    BOOST_TEST_MESSAGE("Par bond price would require spread of " << impliedSpreadPar);
    BOOST_CHECK_CLOSE(impliedSpreadPar, bondSpecificSpread->value() + issuerSpreadQuote->value(), 0.0001);
    QuantLib::ext::dynamic_pointer_cast<SimpleQuote>(*bondSpecificSpread)->setValue(impliedSpreadPar);
    pricePar = bond->dirtyPrice();
    BOOST_TEST_MESSAGE("Bond spread of " << bondSpecificSpread->value() << " means price of " << pricePar);
    BOOST_CHECK_CLOSE(pricePar, parRedemption, 0.0001);
}
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
