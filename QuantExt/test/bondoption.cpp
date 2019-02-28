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

#include "utilities.hpp"
#include <boost/test/unit_test.hpp>
#include "toplevelfixture.hpp"
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/schedule.hpp>
#include <ql/instruments/bond.hpp>
#include <qle/pricingengines/blackbondoptionengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <boost/make_shared.hpp>
#include <iomanip>
#include <iostream>


using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BondOptionTest)

BOOST_AUTO_TEST_CASE(testBondOption) {

        BOOST_TEST_MESSAGE("Testing QuantExt Bond Option pricing.");

        SavedSettings backup;

        Settings::instance().evaluationDate() = Date(5, Feb, 2016);
        Date issueDate = Date(3, Nov, 2015);
        Date today = Settings::instance().evaluationDate();
        Settings::instance().includeReferenceDateEvents() = true;

        // bond market data
        Handle<Quote> rateQuote(boost::make_shared<SimpleQuote>(0.1));
        Handle<Quote> issuerSpreadQuote(boost::make_shared<SimpleQuote>(0.0));
        DayCounter dc = Actual365Fixed();
        Handle<YieldTermStructure> yts(boost::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
        Handle<DefaultProbabilityTermStructure> dpts(boost::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
        Handle<Quote> bondSpecificSpread(boost::make_shared<SimpleQuote>(0.0));

        // build the underlying fixed rate bond
        Date startDate = today;
        Date endDate = startDate + Period(2, Years);
        Period tenor = 6 * Months;
        Calendar calendar = WeekendsOnly();
        BusinessDayConvention bdc = Following;
        BusinessDayConvention bdcEnd = bdc;
        DateGeneration::Rule rule = DateGeneration::Forward;
        bool endOfMonth = false;
        Date firstDate, lastDate;
        Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

        Real redemption = 100.0;
        Real notional = 1000;
        Real couponRate = 0.1;

        Leg leg =
            FixedRateLeg(schedule).withNotionals(notional).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

        // bond option market data
        Handle<Quote> discountQuote(boost::make_shared<SimpleQuote>(0.1));
        Handle<YieldTermStructure> discountTS(
            boost::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));

        boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
            new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                ModifiedFollowing, 0.09, Actual365Fixed(), ShiftedLognormal, 0.0));

        // build the bond option
        Date maturityDate = today;
        Real strikePrice = notional;
        Real faceAmount = notional;
        Natural settlementDays = 2;
        std::vector<Rate> rates = std::vector<Rate>(1, couponRate);
        Callability::Price callabilityPrice = Callability::Price(strikePrice, Callability::Price::Dirty);
        Callability::Type callabilityType = Callability::Call;
        Date exerciseDate = Date(5, Dec, 2016);
        boost::shared_ptr<Callability> callability(new Callability(callabilityPrice, callabilityType, exerciseDate));
        CallabilitySchedule callabilitySchedule = std::vector<boost::shared_ptr<Callability> >(1, callability);

        boost::shared_ptr<QuantExt::BondOption> bondOption(new QuantExt::FixedRateBondOption(settlementDays, faceAmount, schedule, rates,
            dc, bdc, redemption, issueDate, callabilitySchedule));

        boost::shared_ptr<PricingEngine> bondOptionEngine(new QuantExt::BlackBondOptionEngine(
            Handle<QuantLib::SwaptionVolatilityStructure>(svs), discountTS, false));
        bondOption->setPricingEngine(bondOptionEngine);

        BOOST_TEST_MESSAGE("Bond option price = " << bondOption->NPV());
        BOOST_CHECK_CLOSE(bondOption->NPV(), 31.575117046, 0.000001);
    }

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()


