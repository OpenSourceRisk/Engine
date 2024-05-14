/*
 Copyright (C) 2023 Skandinaviska Enskilda Banken AB (publ)
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
#include <ql/indexes/inflation/ukrpi.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <qle/cashflows/cpicoupon.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CpiLegTest)

BOOST_AUTO_TEST_CASE(testCpiLegPaymentLag) {

    BOOST_TEST_MESSAGE("Testing QuantExt::CPILeg for payment lag...");

    Date evaluationDate(6, October, 2023);
    Settings::instance().evaluationDate() = evaluationDate;
    Calendar calendar = WeekendsOnly();
    DayCounter dayCounter = ActualActual(ActualActual::ISDA);

    Date startDate(6, October, 2023);
    Date endDate(6, October, 2026);
    Schedule fixedSchedule = MakeSchedule()
                                 .from(startDate)
                                 .to(endDate)
                                 .withTenor(Period(6, Months))
                                 .withCalendar(calendar)
                                 .withConvention(ModifiedFollowing)
                                 .backwards();

    auto flatYts = ext::shared_ptr<YieldTermStructure>(new FlatForward(evaluationDate, 0.025, dayCounter));
    RelinkableHandle<YieldTermStructure> yTS(flatYts);

    auto ukrpi = ext::make_shared<UKRPI>();
    Leg cpiLeg = QuantExt::CPILeg(fixedSchedule, ukrpi, yTS, 100, Period(3, Months))
                     .withNotionals(1e6)
                     .withFixedRates(0.01)
                     .withPaymentCalendar(calendar)
                     .withPaymentLag(2);

    for (auto& coupon : cpiLeg) {
        if (auto cpiCoupon = ext::dynamic_pointer_cast<CPICoupon>(coupon))
            // The setup leg will have six regular coupons
            BOOST_CHECK_EQUAL(cpiCoupon->date(), cpiCoupon->accrualEndDate() + 2 * Days);
        else if (auto cpiNotionalCashflow = ext::dynamic_pointer_cast<CPICashFlow>(coupon))
            // and one of these flows
            BOOST_CHECK_EQUAL(cpiNotionalCashflow->date(), fixedSchedule.endDate() + 2 * Days);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
